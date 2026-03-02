// main.cpp — Master ESP32-S3 Phase 3
// Cycle : init -> capteurs -> BLE slaves -> payload LoRaWAN -> OLED -> deep sleep

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
#include "sensor_manager.h"
#include "menus_master.h"
#include "ble_master.h"
#include "lora_manager.h"

// ===== VARIABLES GLOBALES =====
// Partagees avec les modules via extern
ConfigGenerale_t  config;
HiveSensor_Data_t HiveSensor_Data;

// Donnees BLE des slaves (remplies par bleMasterCollect, utilisees par payload)
SlaveReading_t slaveReadings[NUM_SLAVES];

// ===== CONSTANTES PHASE 1 =====
// Delai avant deep sleep (laisser le temps de lire l'OLED et de brancher un clavier)
static const uint32_t DISPLAY_TIMEOUT_MS = 30000; // 30 s

// ===== PROTOTYPES =====
static void showSensorsOnOLED(void);
static void handleWakeupPayload(void);

// ---------------------------------------------------------------------------
// @brief Initialisation complete du master
// ---------------------------------------------------------------------------
void setup()
{
  Serial.begin(115200);
  delay(500);

  // 1. Cause du reveil (incremente bootCount en RTC RAM)
  powerPrintWakeupCause();

  // 2. Bus I2C (RTC + EEPROM + OLED + BME280 + BH1750 + INA219)
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

  // 3. OLED : splash pendant l'init
  // displayText(col, row, text) — col en caracteres (0..20), row en lignes (0..7)
  displayInit();
  displayText(0, 0, "Ruches ESP32-S3");
  displayText(0, 1, VERSION);
  displayText(0, 2, "Init...");
  displayFlush();

  // 4. Configuration persistante (EEPROM AT24C32)
  E24C32initConfig();

  // 5. RTC DS3231 + alarmes
  initRTC();
  DS3231setRTCAlarm1();   // Tick 1 seconde (mode programmation / affichage)
  DS3231setRTCAlarm2();   // Alarme payload (toutes les WAKEUP_INTERVAL_MIN min)
  attachInterrupt(digitalPinToInterrupt(PIN_RTC_INT), onRTCAlarm, FALLING);

  // 6. Clavier analogique 5 touches
  keypadInit();

  // 7. HX711 (cellule de charge)
  hx711Init();

  // 8. Capteurs I2C master (BME280, BH1750, INA219)
  bool sensorsOK = sensorsInit();
  if (!sensorsOK)
  {
    LOG_WARNING("Un ou plusieurs capteurs absents");
  }

  // 9. Premiere lecture complete
  sensorsReadAll();
  hx711GetWeightGrams();

  // 10. Affichage initial
  showSensorsOnOLED();
  sensorsPrintAll();

  // 11. LoRaWAN : init module SX1262 (SPI + radio)
  // La session est restauree depuis NVM au premier cycle payload
  loraInit();

  LOG_INFO("Setup termine");
}

// ---------------------------------------------------------------------------
// @brief Boucle principale (mode interactif / debug)
//
// Cycle normal (deep sleep actif) :
//   setup() -> lecture -> OLED -> 30 s -> powerDeepSleep()
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
    sensorReadBME280();
    sensorReadBH1750();
    sensorReadVBat();
    if (currentMenuDepth == 0)
    {
      showSensorsOnOLED();
    }
  }

  // Alarme payload : cycle de mesure complet (seulement hors menus)
  if (wakeupPayload)
  {
    wakeupPayload  = false;
    lastActivityMs = millis();
    if (currentMenuDepth == 0)
    {
      handleWakeupPayload();
    }
  }

  // --- Menus actifs : traitement saisies/navigation ---
  if (currentMenuDepth > 0)
  {
    processActiveInputs();
    // Sortie menu : revenir a l'affichage capteurs + relancer le timeout
    if (currentMenuDepth == 0)
    {
      showSensorsOnOLED();
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
// @brief Affiche toutes les valeurs capteurs sur l'OLED (8 lignes x 21 chars)
//
// Row 0 : Temperature + Humidite
// Row 1 : Pression + Luminosite
// Row 2 : Poids HX711
// Row 3 : VBat + VSol
// Row 4 : Courant solaire
// Row 5 : Numero de boot
// ---------------------------------------------------------------------------
static void showSensorsOnOLED(void)
{
  char buf[22];

  displayClear();

  snprintf(buf, sizeof(buf), "T:%.1fC HR:%.0f%%",
           HiveSensor_Data.DHT_Temp, HiveSensor_Data.DHT_Hum);
  displayText(0, 0, buf);

  snprintf(buf, sizeof(buf), "P:%.0fhPa L:%.0flux",
           HiveSensor_Data.Pressure, HiveSensor_Data.Brightness);
  displayText(0, 1, buf);

  snprintf(buf, sizeof(buf), "Poids: %.0fg",
           HiveSensor_Data.HX711Weight[0]);
  displayText(0, 2, buf);

  snprintf(buf, sizeof(buf), "Vb:%.2fV Vs:%.2fV",
           HiveSensor_Data.Bat_Voltage, HiveSensor_Data.Solar_Voltage);
  displayText(0, 3, buf);

  snprintf(buf, sizeof(buf), "Isol: %.0fmA",
           HiveSensor_Data.SolarCurrent);
  displayText(0, 4, buf);

  snprintf(buf, sizeof(buf), "Boot #%lu", (unsigned long)bootCount);
  displayText(0, 5, buf);

  displayFlush();
}

// ---------------------------------------------------------------------------
// @brief Cycle de mesure complet declenche par l'alarme payload RTC
//
// Phase 3 :
//   1. Lecture capteurs master (BME280, BH1750, INA219, HX711, VBat)
//   2. Collecte BLE : scan + connexion slaves + lecture poids/VBat/timestamp
//   3. Join LoRaWAN (ou restauration session NVM) si pas encore joint
//   4. Construction payload V2 (24 octets) + envoi LoRaWAN
//   5. Affichage OLED + Serial
// ---------------------------------------------------------------------------
static void handleWakeupPayload(void)
{
  LOG_INFO("=== Cycle payload ===");

  // 1. Mesures master
  sensorsReadAll();
  hx711GetWeightGrams();

  // 2. Collecte BLE slaves (bloquant : scan 10 s + connexions)
  bleMasterCollect(slaveReadings, NUM_SLAVES);

  for (uint8_t i = 0; i < NUM_SLAVES; i++)
  {
    if (slaveReadings[i].valid)
    {
      Serial.printf("[BLE] Slave %u : poids=%dg vbat=%u.%uV ts=%lu\n",
                    (unsigned)i,
                    (int)slaveReadings[i].weight * 10,
                    (unsigned)(slaveReadings[i].vbat / 10 + 2),
                    (unsigned)(slaveReadings[i].vbat % 10),
                    (unsigned long)slaveReadings[i].timestamp);
    }
    else
    {
      Serial.printf("[BLE] Slave %u : absent\n", (unsigned)i);
    }
  }

  // 3. Join LoRaWAN si pas encore joint (NVM ou OTAA frais)
  if (!loraIsJoined())
  {
    loraJoin();
  }

  // 4. Envoi payload V2 (24 octets)
  if (loraIsJoined())
  {
    loraSendPayload(&HiveSensor_Data, slaveReadings, NUM_SLAVES);
  }

  // 5. Affichage OLED + Serial
  showSensorsOnOLED();
  sensorsPrintAll();

  // Ligne 7 de l'OLED : statut LoRa
  char loraStatus[22];
  loraGetStatus(loraStatus, sizeof(loraStatus));
  displayText(0, 7, loraStatus);
  displayFlush();
}
