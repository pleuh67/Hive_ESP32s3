// main.cpp — Slave ESP32-S3 Phase 2
// Cycle : init -> HX711 + VBat -> OLED -> BLE advertising -> deep sleep

#include <Arduino.h>
#include <Wire.h>
#include "types.h"
#include "config.h"
#include "common/eeprom_manager.h"
#include "common/rtc_manager.h"
#include "common/hx711_manager.h"
#include "common/display_manager.h"
#include "common/keypad.h"
#include "common/power_manager.h"
#include "common/menus_common.h"
#include "menus_slave.h"
#include "ble_slave.h"

// ===== VARIABLES GLOBALES =====
// Partagees avec les modules via extern
ConfigGenerale_t  config;
HiveSensor_Data_t HiveSensor_Data;

// ===== CONSTANTES PHASE 1 =====
static const uint32_t DISPLAY_TIMEOUT_MS = 30000; // 30 s avant deep sleep

// ===== PROTOTYPES =====
static void readVBat(void);
static void showSlaveOnOLED(void);
static void printSlaveSerial(void);
static void handleSlaveWakeupPayload(void);

// ---------------------------------------------------------------------------
// @brief Lit la tension batterie via ADC (diviseur resistif) et stocke dans
//        HiveSensor_Data.Bat_Voltage
// Identique a sensorReadVBat() du master, porte directement ici car
// sensor_manager est master-only.
// ---------------------------------------------------------------------------
static void readVBat(void)
{
  uint32_t sum = 0;
  for (uint8_t i = 0; i < 8; i++)
  {
    sum += (uint32_t)analogRead(PIN_VBAT_ADC);
  }
  float adcVal = (float)(sum / 8);
  uint8_t carte = config.materiel.Num_Carte;
  if (carte >= 10) carte = 0;
  float scale = (config.materiel.VBatScale != 0.0f)
                ? config.materiel.VBatScale
                : VBatScale_List[carte];
  HiveSensor_Data.Bat_Voltage = adcVal * scale;
}

// ---------------------------------------------------------------------------
// @brief Initialisation complete du slave
// ---------------------------------------------------------------------------
void setup()
{
  Serial.begin(115200);
  delay(500);

  // 1. Cause du reveil (incremente bootCount en RTC RAM)
  powerPrintWakeupCause();

  // 2. Bus I2C (RTC + EEPROM + OLED + HX711 partage sur meme bus)
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

  // 3. OLED : splash pendant l'init
  displayInit();
  displayText(0, 0, "Slave ESP32-S3");
  displayText(0, 1, VERSION);
  displayText(0, 2, "Init...");
  displayFlush();

  // 4. Configuration persistante (EEPROM AT24C32)
  E24C32initConfig();

  // 5. RTC DS3231 + alarmes
  initRTC();
  DS3231setRTCAlarm1();   // Tick 1 seconde (mode interactif)
  DS3231setRTCAlarm2();   // Alarme payload (toutes les WAKEUP_INTERVAL_MIN min)
  attachInterrupt(digitalPinToInterrupt(PIN_RTC_INT), onRTCAlarm, FALLING);

  // 6. Clavier analogique 5 touches
  keypadInit();

  // 7. HX711 (cellule de charge 200 kg)
  hx711Init();

  // 8. Premiere lecture
  hx711GetWeightGrams();
  readVBat();

  // 9. Affichage initial
  showSlaveOnOLED();
  printSlaveSerial();

  LOG_INFO("Slave setup OK");
}

// ---------------------------------------------------------------------------
// @brief Boucle principale (mode interactif / debug)
//
// Cycle normal (deep sleep actif) :
//   setup() -> mesure -> OLED -> 30 s -> powerDeepSleep()
//   Reveil EXT0 -> setup() a nouveau
//
// Mode interactif (touche clavier) :
//   Loop continue, OLED rafraichi chaque seconde via alarme RTC
// ---------------------------------------------------------------------------
void loop()
{
  static bool     interactiveMode = false;
  static uint32_t lastActivityMs  = millis();

  // --- Clavier non bloquant ---
  processContinuousKeyboard();
  key_code_t key = readKeyNonBlocking();
  if (key != KEY_NONE)
  {
    interactiveMode = true;
    lastActivityMs  = millis();
    if (currentMenuDepth == 0)
    {
      // Premiere touche : ouvrir le systeme de menus
      initMenuSystem();
    }
    else
    {
      // Menus actifs : transmettre la touche a la machine a etats
      touche = key;
    }
  }

  // --- Alarmes RTC ---
  processRTCAlarms();

  // Tick 1 seconde : rafraichir OLED (seulement hors menus)
  if (wakeup1Sec)
  {
    wakeup1Sec = false;
    hx711GetWeightGrams();
    readVBat();
    if (currentMenuDepth == 0)
    {
      showSlaveOnOLED();
    }
  }

  // Alarme payload : cycle de mesure complet (seulement hors menus)
  if (wakeupPayload)
  {
    wakeupPayload  = false;
    lastActivityMs = millis();
    if (currentMenuDepth == 0)
    {
      handleSlaveWakeupPayload();
    }
  }

  // --- Menus actifs : traitement saisies/navigation ---
  if (currentMenuDepth > 0)
  {
    processActiveInputs();
    // Sortie menu : revenir a l'affichage capteurs + relancer le timeout
    if (currentMenuDepth == 0)
    {
      showSlaveOnOLED();
      interactiveMode = false;
      lastActivityMs  = millis();
    }
  }

  // --- Deep sleep si inactif depuis DISPLAY_TIMEOUT_MS ---
  if (!interactiveMode && (millis() - lastActivityMs >= DISPLAY_TIMEOUT_MS))
  {
    DS3231setRTCAlarm2(); // Reconfigurer l'alarme pour le prochain cycle
    powerDeepSleep();
    // powerDeepSleep() ne retourne jamais
  }
}

// ---------------------------------------------------------------------------
// @brief Affiche les donnees du slave sur l'OLED (5 lignes)
//
// Row 0 : Identifiant slave (numero carte)
// Row 1 : Poids HX711
// Row 2 : Tension batterie
// Row 3 : Horodatage DS3231
// Row 4 : Numero de boot
// ---------------------------------------------------------------------------
static void showSlaveOnOLED(void)
{
  char buf[22];
  DateTime now = rtc.now();

  displayClear();

  snprintf(buf, sizeof(buf), "Slave #%u", config.materiel.Num_Carte);
  displayText(0, 0, buf);

  snprintf(buf, sizeof(buf), "Poids: %.0fg",
           HiveSensor_Data.HX711Weight[0]);
  displayText(0, 1, buf);

  snprintf(buf, sizeof(buf), "Vbat: %.2fV",
           HiveSensor_Data.Bat_Voltage);
  displayText(0, 2, buf);

  snprintf(buf, sizeof(buf), "%02u:%02u %02u/%02u/%02u",
           now.hour(), now.minute(),
           now.day(), now.month(), (uint8_t)(now.year() % 100));
  displayText(0, 3, buf);

  snprintf(buf, sizeof(buf), "Boot #%lu", (unsigned long)bootCount);
  displayText(0, 4, buf);

  displayFlush();
}

// ---------------------------------------------------------------------------
// @brief Affiche les donnees du slave sur Serial
// ---------------------------------------------------------------------------
static void printSlaveSerial(void)
{
  DateTime now = rtc.now();
  char buf[64];
  snprintf(buf, sizeof(buf),
           "Poids=%.0fg Vb=%.2fV %02u:%02u:%02u",
           HiveSensor_Data.HX711Weight[0],
           HiveSensor_Data.Bat_Voltage,
           now.hour(), now.minute(), now.second());
  Serial.println(buf);
}

// ---------------------------------------------------------------------------
// @brief Encode la tension batterie pour BLE (uint8_t)
// Encodage : (V - 2.0) * 10 — plage 0-25 pour 2.0-4.5 V
// ---------------------------------------------------------------------------
static uint8_t encodVBat(float voltage)
{
  float encoded = (voltage - 2.0f) * 10.0f;
  if (encoded < 0.0f)   encoded = 0.0f;
  if (encoded > 254.0f) encoded = 254.0f;
  return static_cast<uint8_t>(encoded);
}

// ---------------------------------------------------------------------------
// @brief Cycle de mesure complet declenche par l'alarme payload RTC
//
// Phase 2 :
//   1. Mesures HX711 + VBat
//   2. Affichage OLED + Serial
//   3. BLE advertising (BLE_ADVERTISING_SEC s) — GATT server : poids, VBat, ts
//   4. Attente connexion master (boucle non bloquante)
//   5. BLE stop -> deep sleep
// ---------------------------------------------------------------------------
static void handleSlaveWakeupPayload(void)
{
  LOG_INFO("=== Slave cycle payload ===");

  hx711GetWeightGrams();
  readVBat();
  showSlaveOnOLED();
  printSlaveSerial();

  // Encodage des valeurs pour les caracteristiques BLE
  // Poids : int16_t en unite 0.01 kg (10 g par unite)
  int16_t weightRaw = static_cast<int16_t>(HiveSensor_Data.HX711Weight[0] / 10.0f);
  uint8_t vbatEnc   = encodVBat(HiveSensor_Data.Bat_Voltage);
  uint32_t ts       = static_cast<uint32_t>(rtc.now().unixtime());

  // Lancer le serveur GATT BLE
  bleSlaveInit(weightRaw, vbatEnc, ts);
  bleSlaveStart();

  // Attente non bloquante : fin advertising ou connexion master
  uint32_t t0 = millis();
  const uint32_t BLE_WAIT_MS = (BLE_ADVERTISING_SEC + 5) * 1000UL;
  while (!bleSlaveIsComplete() && (millis() - t0 < BLE_WAIT_MS))
  {
    delay(50); // NimBLE tourne sur sa propre tache FreeRTOS
  }

  bleSlaveStop();

  // Reconfigurer l'alarme et dormir jusqu'au prochain cycle
  DS3231setRTCAlarm2();
  powerDeepSleep();
  // Ne retourne jamais
}
