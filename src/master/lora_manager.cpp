// lora_manager.cpp — Couche LoRaWAN master ESP32-S3
// Phase 3 : RadioLib 7.x, SX1262 E22-900M22S, OTAA, EU868
//
// Architecture :
//   - Module SX1262 sur SPI avec pins dedies (config.h)
//   - E22-900M22S : DIO2 pilote le commutateur antenne RF (setDio2AsRfSwitch)
//   - Session OTAA stockee en RTC RAM — survive au deep sleep ESP32
//   - Restauration : setBufferSession() + isActivated()
//   - Nouveau join : clearSession() implicite dans beginOTAA()
//
// Payload V2 (24 octets) :
//   [0]    version (0x02)       [1]    nodeCount
//   [2]    flags                [3-4]  poids master (int16, x0.01 kg)
//   [5-6]  temperature (x0.1C) [7]    humidite (x0.5%)
//   [8-9]  pression (x0.1hPa)  [10-11] luminosite (lux)
//   [12]   VBat master (x0.1V) [13]   VSol (x0.1V)
//   [14]   ISol (x10mA)        [15-16] poids slave 1
//   [17]   VBat slave 1        [18-19] poids slave 2
//   [20]   VBat slave 2        [21-22] poids slave 3
//   [23]   VBat slave 3

#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>
#include "lora_manager.h"
#include "config.h"
#include "credentials.h"
#include "types.h"

// ---------------------------------------------------------------------------
// Instances statiques (pas d'allocation dynamique)
// Module() stocke uniquement les numeros de pins — aucun acces GPIO ici
// ---------------------------------------------------------------------------

static Module      s_module(PIN_LORA_NSS, PIN_LORA_DIO1,
                             PIN_LORA_RESET, PIN_LORA_BUSY);
static SX1262      s_radio(&s_module);
static LoRaWANNode s_node(&s_radio, &EU868);

// ---------------------------------------------------------------------------
// Session persistee en RTC RAM (survit au deep sleep ESP32)
// getBufferSession() retourne un pointeur vers le buffer interne de s_node
// setBufferSession() restaure le contexte sans re-join
// ---------------------------------------------------------------------------

RTC_DATA_ATTR static uint8_t s_sessionBuf[RADIOLIB_LORAWAN_SESSION_BUF_SIZE];
RTC_DATA_ATTR static bool    s_sessionSaved = false;

// Etat interne courante
static bool s_initOK = false;
static bool s_joined = false;
static char s_status[32] = "Non init";

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Convertit un tableau de 8 octets MSB-first en uint64_t pour RadioLib
static uint64_t euiToUint64(const uint8_t* eui)
{
  uint64_t result = 0;
  for (uint8_t i = 0; i < 8; i++)
  {
    result = (result << 8) | eui[i];
  }
  return result;
}

// Clamp float en uint8_t avec saturation
static uint8_t clampU8(float v, float scale)
{
  float s = v * scale;
  if (s < 0.0f)   return 0;
  if (s > 254.0f) return 254;
  return static_cast<uint8_t>(s);
}

// Clamp float en int16_t avec saturation
static int16_t clampI16(float v, float scale)
{
  float s = v * scale;
  if (s >  32766.0f) return  32766;
  if (s < -32767.0f) return -32767;
  return static_cast<int16_t>(s);
}

// ---------------------------------------------------------------------------
// Construction du payload V2 (24 octets)
// ---------------------------------------------------------------------------

static void buildPayloadV2(uint8_t* buf,
                            const HiveSensor_Data_t* data,
                            const SlaveReading_t*    slaves,
                            uint8_t                  slaveCount)
{
  uint8_t nodeCount = 1;   // master
  uint8_t flags     = 0x01; // bit 0 = master OK

  for (uint8_t i = 0; i < slaveCount && i < 3; i++)
  {
    if (slaves[i].valid)
    {
      nodeCount++;
      flags |= static_cast<uint8_t>(1 << (i + 1)); // bit 1/2/3 = slave 1/2/3
    }
  }

  // [0-1] Version + NodeCount
  buf[0] = PAYLOAD_VERSION;
  buf[1] = nodeCount;

  // [2] Flags
  buf[2] = flags;

  // [3-4] Poids master — int16_t en 0.01 kg (HX711 en grammes, /10)
  int16_t weightMaster = clampI16(data->HX711Weight[0], 0.1f);
  buf[3] = static_cast<uint8_t>(weightMaster >> 8);
  buf[4] = static_cast<uint8_t>(weightMaster & 0xFF);

  // [5-6] Temperature — int16_t en x0.1 degC
  int16_t temp = clampI16(data->DHT_Temp, 10.0f);
  buf[5] = static_cast<uint8_t>(temp >> 8);
  buf[6] = static_cast<uint8_t>(temp & 0xFF);

  // [7] Humidite — uint8_t en x0.5 %
  buf[7] = clampU8(data->DHT_Hum, 2.0f);

  // [8-9] Pression — uint16_t en x0.1 hPa
  uint16_t press = static_cast<uint16_t>(data->Pressure * 10.0f);
  buf[8]  = static_cast<uint8_t>(press >> 8);
  buf[9]  = static_cast<uint8_t>(press & 0xFF);

  // [10-11] Luminosite — uint16_t en lux
  uint16_t lux = (data->Brightness > 65535.0f)
                 ? 65535u
                 : static_cast<uint16_t>(data->Brightness);
  buf[10] = static_cast<uint8_t>(lux >> 8);
  buf[11] = static_cast<uint8_t>(lux & 0xFF);

  // [12] VBat master — uint8_t en x0.1V
  buf[12] = clampU8(data->Bat_Voltage, 10.0f);

  // [13] VSol — uint8_t en x0.1V
  buf[13] = clampU8(data->Solar_Voltage, 10.0f);

  // [14] ISol — uint8_t en x10mA
  buf[14] = clampU8(data->SolarCurrent, 0.1f);

  // [15-23] Slaves 1, 2, 3 — 3 octets chacun : poids H, poids L, VBat
  for (uint8_t i = 0; i < 3; i++)
  {
    uint8_t base = 15 + i * 3;
    if (i < slaveCount && slaves[i].valid)
    {
      // Poids slave deja en 0.01 kg (encode par ble_slave.cpp : poids_g / 10)
      buf[base]     = static_cast<uint8_t>(slaves[i].weight >> 8);
      buf[base + 1] = static_cast<uint8_t>(slaves[i].weight & 0xFF);
      // VBat slave : vbat_enc = (V - 2.0) * 10
      // Payload x0.1V : V * 10 = vbat_enc + 20
      buf[base + 2] = static_cast<uint8_t>(slaves[i].vbat + 20);
    }
    else
    {
      buf[base]     = static_cast<uint8_t>(PAYLOAD_INVALID_WEIGHT >> 8);
      buf[base + 1] = static_cast<uint8_t>(PAYLOAD_INVALID_WEIGHT & 0xFF);
      buf[base + 2] = PAYLOAD_INVALID_VBAT;
    }
  }
  // buf[23] = buf[15 + 2*3 + 2] = buf[23] → inclus dans la boucle (i=2, base=21, +2=23)
}

// ---------------------------------------------------------------------------

bool loraInit(void)
{
  // SPI avec pins LoRa dedies
  SPI.begin(PIN_LORA_SCK, PIN_LORA_MISO, PIN_LORA_MOSI, PIN_LORA_NSS);

  int16_t state = s_radio.begin();
  if (state != RADIOLIB_ERR_NONE)
  {
    snprintf(s_status, sizeof(s_status), "SX1262 err %d", (int)state);
    LOG_ERROR(s_status);
    s_initOK = false;
    return false;
  }

  // E22-900M22S : DIO2 pilote l'antenne RF switch TX/RX
  s_radio.setDio2AsRfSwitch(true);

  s_initOK = true;
  snprintf(s_status, sizeof(s_status), "SX1262 OK");
  LOG_INFO(s_status);
  return true;
}

// ---------------------------------------------------------------------------

bool loraJoin(void)
{
  if (!s_initOK)
  {
    snprintf(s_status, sizeof(s_status), "Init requis");
    return false;
  }

  // 1. Tenter de restaurer la session depuis RTC RAM
  if (s_sessionSaved)
  {
    int16_t state = s_node.setBufferSession(s_sessionBuf);
    if (state == RADIOLIB_ERR_NONE && s_node.isActivated())
    {
      s_joined = true;
      snprintf(s_status, sizeof(s_status), "Session restauree");
      LOG_INFO(s_status);
      return true;
    }
    // Session corrompue ou expirée — forcer un nouveau join
    s_sessionSaved = false;
    Serial.println("[LoRa] Session invalide, nouveau join...");
  }

  // 2. Join OTAA (commence toujours par clearSession() en interne)
  static const uint8_t devEUI[]  = LORA_DEV_EUI;
  static const uint8_t joinEUI[] = LORA_JOIN_EUI;
  static const uint8_t appKey[]  = LORA_APP_KEY;
  static const uint8_t nwkKey[]  = LORA_NWK_KEY;

  uint64_t devEUI64  = euiToUint64(devEUI);
  uint64_t joinEUI64 = euiToUint64(joinEUI);

  Serial.println("[LoRa] OTAA Join...");
  int16_t state = s_node.beginOTAA(joinEUI64, devEUI64,
                                    const_cast<uint8_t*>(nwkKey),
                                    const_cast<uint8_t*>(appKey));
  if (state != RADIOLIB_ERR_NONE)
  {
    s_joined = false;
    snprintf(s_status, sizeof(s_status), "Join err %d", (int)state);
    LOG_ERROR(s_status);
    return false;
  }

  // 3. Sauvegarder la session en RTC RAM pour les prochains reveil
  memcpy(s_sessionBuf, s_node.getBufferSession(),
         RADIOLIB_LORAWAN_SESSION_BUF_SIZE);
  s_sessionSaved = true;
  s_joined       = true;
  snprintf(s_status, sizeof(s_status), "Join OK");
  LOG_INFO(s_status);
  return true;
}

// ---------------------------------------------------------------------------

bool loraIsJoined(void)
{
  return s_joined;
}

// ---------------------------------------------------------------------------

bool loraSendPayload(const HiveSensor_Data_t* data,
                     const SlaveReading_t*    slaves,
                     uint8_t                  slaveCount)
{
  if (!s_joined)
  {
    snprintf(s_status, sizeof(s_status), "Non joint");
    LOG_WARNING(s_status);
    return false;
  }

  // Construction du payload V2
  uint8_t payload[PAYLOAD_SIZE_V2];
  buildPayloadV2(payload, data, slaves, slaveCount);

  // Log hex du payload
  char hexBuf[PAYLOAD_SIZE_V2 * 2 + 1];
  for (uint8_t i = 0; i < PAYLOAD_SIZE_V2; i++)
  {
    snprintf(hexBuf + i * 2, 3, "%02X", payload[i]);
  }
  Serial.printf("[LoRa] Payload V2 (%u B): %s\n",
                (unsigned)PAYLOAD_SIZE_V2, hexBuf);

  // Envoi LoRaWAN — port 1, non confirme
  int16_t state = s_node.sendReceive(payload, PAYLOAD_SIZE_V2, 1, false);

  // Mettre a jour la session (frame counter incremente)
  if (s_sessionSaved)
  {
    memcpy(s_sessionBuf, s_node.getBufferSession(),
           RADIOLIB_LORAWAN_SESSION_BUF_SIZE);
  }

  // RADIOLIB_ERR_NONE : TX OK (avec ou sans downlink)
  // RADIOLIB_ERR_NO_RX_WINDOW : TX OK, pas de fenetre RX (acceptable)
  if (state == RADIOLIB_ERR_NONE || state == RADIOLIB_ERR_NO_RX_WINDOW)
  {
    snprintf(s_status, sizeof(s_status), "Envoye OK");
    LOG_INFO(s_status);
    return true;
  }

  snprintf(s_status, sizeof(s_status), "Send err %d", (int)state);
  LOG_ERROR(s_status);
  return false;
}

// ---------------------------------------------------------------------------

void loraGetStatus(char* buf, uint8_t bufLen)
{
  snprintf(buf, bufLen, "%s", s_status);
}
