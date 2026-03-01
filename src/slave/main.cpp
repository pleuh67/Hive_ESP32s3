// main.cpp — Slave ESP32-S3 Phase 1
// Cycle : init -> HX711 + VBat -> OLED -> deep sleep (alarme RTC)
// Phase 2 : BLE advertising + GATT server (poids, VBat, timestamp)

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
  HiveSensor_Data.Bat_Voltage = adcVal * VBatScale_List[carte];
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
    char buf[32];
    snprintf(buf, sizeof(buf), "Touche: %s", keyToString(key));
    LOG_DEBUG(buf);
    // TODO Phase 1 (menus) : dispatcher selon key (tare, calibration, info)
  }

  // --- Alarmes RTC ---
  processRTCAlarms();

  // Tick 1 seconde : rafraichir OLED
  if (wakeup1Sec)
  {
    wakeup1Sec = false;
    hx711GetWeightGrams();
    readVBat();
    showSlaveOnOLED();
  }

  // Alarme payload : cycle de mesure complet
  if (wakeupPayload)
  {
    wakeupPayload  = false;
    lastActivityMs = millis();
    handleSlaveWakeupPayload();
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
// @brief Cycle de mesure complet declenche par l'alarme payload RTC
//
// Phase 1 : mesure + OLED + Serial
// Phase 2 : + BLE advertising 30 s (GATT server : poids, VBat, timestamp)
//           puis deep sleep
// ---------------------------------------------------------------------------
static void handleSlaveWakeupPayload(void)
{
  LOG_INFO("=== Slave cycle payload ===");

  hx711GetWeightGrams();
  readVBat();

  showSlaveOnOLED();
  printSlaveSerial();

  // TODO Phase 2 : lancer BLE advertising (30 s) + GATT server
  //   ble_slave_start();
  //   while (!ble_slave_timeout()) { ... }
  //   ble_slave_stop();
}
