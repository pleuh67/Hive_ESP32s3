// rtc_manager.cpp — Gestion RTC DS3231 + alarmes + ISR
// Porte depuis DS3231.cpp + ISR.cpp (POC_ATSAMD) — 90% portable
// Modifications : debugSerial -> Serial, IRAM_ATTR sur ISR,
//                 pas de Serial/I2C dans ISR (flag only)

#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>
#include "rtc_manager.h"
#include "types.h"
#include "config.h"

// ===== VARIABLES GLOBALES RTC =====
RTC_DS3231 rtc;
DateTime nextPayload;

// Flags d'interruption (ISR <-> loop)
volatile bool alarm1_enabled = true;
volatile bool alarm2_enabled = true;
volatile bool wakeupPayload = false;
volatile bool wakeup1Sec = false;
volatile bool displayNextPayload = false;
volatile bool rtcAlarmFlag = false;  // Nouveau flag simplifie pour ESP32

// Debug flags
bool DEBUG_INTERVAL_1SEC = false;
bool DEBUG_WAKEUP_PAYLOAD = true;

// ===== VARIABLES EXTERNES =====
extern ConfigGenerale_t config;

// Forward declarations — fonctions OLED (seront definies dans display_manager)
void OLEDDebugDisplay(const char* message) __attribute__((weak));
void OLEDDisplayMessageL8(const char* message, bool defilant, bool inverse) __attribute__((weak));

// Implementations faibles par defaut (si OLED pas encore porte)
void __attribute__((weak)) OLEDDebugDisplay(const char* message)
{
  Serial.print("[OLED] ");
  Serial.println(message);
}

void __attribute__((weak)) OLEDDisplayMessageL8(const char* message, bool defilant, bool inverse)
{
  Serial.print("[OLED-L8] ");
  Serial.println(message);
}

// ---------------------------------------------------------------------------
// ===== ISR — Handler d'interruption RTC =====
// IRAM_ATTR obligatoire sur ESP32
// REGLE : PAS de Serial, PAS de I2C, PAS d'allocation — flag only
// ---------------------------------------------------------------------------
void IRAM_ATTR onRTCAlarm(void)
{
  rtcAlarmFlag = true;
}

// ---------------------------------------------------------------------------
// @brief Traite les alarmes RTC (appele depuis loop, PAS depuis ISR)
// @param void
// @return void
// ---------------------------------------------------------------------------
void processRTCAlarms(void)
{
  if (!rtcAlarmFlag) return;
  rtcAlarmFlag = false;

  // Verifier quelle alarme a declenche
  if (rtc.alarmFired(1) && alarm1_enabled)
  {
    wakeup1Sec = true;
    rtc.clearAlarm(1);
  }

  if (rtc.alarmFired(2) && alarm2_enabled)
  {
    wakeupPayload = true;
    displayNextPayload = true;
    rtc.clearAlarm(2);
  }
}

// ---------------------------------------------------------------------------
// @brief Initialise le module RTC DS3231
// @param void
// @return void
// ---------------------------------------------------------------------------
void initRTC(void)
{
  if (!rtc.begin())
  {
    LOG_ERROR("Erreur: RTC introuvable");
    OLEDDebugDisplay("RTC introuvable");
    DS3231hardReset();
    if (!rtc.begin())
    {
      LOG_ERROR("Erreur: RESET RTC pas OK => FIN");
      while (0)
      {
        delay(1000);  // TODO: remplacer par blink LED
      }
    }
  }
  else
  {
    if (rtc.lostPower())
    {
      Serial.println("RTC a perdu l'heure, mise a jour avec l'heure de compilation");
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }

    DateTime now = rtc.now();
    char buf[32];
    snprintf(buf, sizeof(buf), "%04d/%02d/%02d %02d:%02d:%02d",
             now.year(), now.month(), now.day(),
             now.hour(), now.minute(), now.second());
    Serial.print("RTC: ");
    Serial.println(buf);

    OLEDDebugDisplay("initRTC DS3231    OK");
  }

  // Configurer l'interruption RTC sur ESP32
  pinMode(PIN_RTC_INT, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_RTC_INT), onRTCAlarm, FALLING);
}

// ---------------------------------------------------------------------------
// @brief Configure alarme 1 : toutes les secondes (mode programmation)
// @param void
// @return void
// ---------------------------------------------------------------------------
void DS3231setRTCAlarm1(void)
{
  rtc.clearAlarm(1);
  Serial.print("=== CONFIGURATION ALARME1 ");
  if (DEBUG_INTERVAL_1SEC)
  {
    DateTime nextSecond = rtc.now() + TimeSpan(0, 0, 0, 1);
    rtc.setAlarm1(nextSecond, DS3231_A1_PerSecond);
  }
  Serial.println("=== 1s DONE ===");
}

// ---------------------------------------------------------------------------
// @brief Configure alarme 2 : payload toutes les X minutes
// @param void
// @return void
// ---------------------------------------------------------------------------
void DS3231setRTCAlarm2(void)
{
  if (!config.applicatif.SendingPeriod)
    return;

  if (DEBUG_WAKEUP_PAYLOAD)
  {
    nextPayload = rtc.now() + TimeSpan(0, 0, config.applicatif.SendingPeriod, 0);
    rtc.setAlarm2(nextPayload, DS3231_A2_Minute);

    char buf[40];
    snprintf(buf, sizeof(buf), "Prochaine IRQ2: %02d:%02d (dans %d min)",
             nextPayload.hour(), nextPayload.minute(),
             config.applicatif.SendingPeriod);
    Serial.println(buf);
  }

  rtc.writeSqwPinMode(DS3231_OFF);
}

// ---------------------------------------------------------------------------
// @brief Efface les 2 alarmes du RTC
// @param void
// @return void
// ---------------------------------------------------------------------------
void DS3231clearRTCAlarms(void)
{
  rtc.clearAlarm(1);
  rtc.clearAlarm(2);
}

// ---------------------------------------------------------------------------
// @brief Hard Reset du RTC (registres de controle)
// @param void
// @return void
// ---------------------------------------------------------------------------
void DS3231hardReset(void)
{
  // Reset des registres de controle
  Wire.beginTransmission(0x68);
  Wire.write(0x0E);  // Registre de controle
  Wire.write(0x1C);  // Valeur de reset par defaut
  Wire.endTransmission();

  Wire.beginTransmission(0x68);
  Wire.write(0x0F);  // Registre de status
  Wire.write(0x00);  // Clear tous les flags
  Wire.endTransmission();

  // Remettre l'heure
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
}

// ---------------------------------------------------------------------------
// @brief Reset complet du RTC (tous les registres)
// @param void
// @return void
// ---------------------------------------------------------------------------
void DS3231CompleteReset(void)
{
  Wire.beginTransmission(0x68);
  for (int i = 0; i <= 0x12; i++)
  {
    Wire.write(i);
    Wire.write(0x00);
    Wire.endTransmission();
    Wire.beginTransmission(0x68);
  }

  Wire.write(0x0E);
  Wire.write(0x1C);
  Wire.endTransmission();

  delay(100);
  rtc.begin();
}

// ---------------------------------------------------------------------------
// @brief Synchronise l'heure du DS3231 vers le micro
// @param void
// @return void
// ---------------------------------------------------------------------------
void DS3231synchronizeTimeToMicro(void)
{
  Serial.println("=== SYNCHRONISATION DS3231 -> MICRO ===");

  DateTime heureDS3231 = rtc.now();
  char buf[32];
  snprintf(buf, sizeof(buf), "DS3231: %02d/%02d/%04d %02d:%02d:%02d",
           heureDS3231.day(), heureDS3231.month(), heureDS3231.year(),
           heureDS3231.hour(), heureDS3231.minute(), heureDS3231.second());
  Serial.println(buf);

  Serial.println("Synchronisation terminee");
  OLEDDisplayMessageL8("Heure synchronisee", false, false);
}

// ---------------------------------------------------------------------------
// @brief Copie l'heure du DS3231 vers le micro avec option de forcage
// @param forcer True pour forcer la copie meme si les heures sont proches
// @return void
// ---------------------------------------------------------------------------
void DS3231copyTimeToMicro(bool forcer)
{
  static unsigned long derniereCopie = 0;
  const unsigned long INTERVALLE_COPIE_MS = 60000;

  if (!forcer && (millis() - derniereCopie < INTERVALLE_COPIE_MS))
  {
    return;
  }

  Serial.println("=== COPIE DS3231 -> MICRO ===");

  DateTime heureDS3231 = rtc.now();

  // Note : sur ESP32, pas de RTC interne separee
  // Le DS3231 est la reference de temps unique
  char buf[21];
  snprintf(buf, 21, "Sync: %02d:%02d:%02d",
           heureDS3231.hour(), heureDS3231.minute(), heureDS3231.second());
  OLEDDisplayMessageL8(buf, false, false);
  Serial.println(buf);
  derniereCopie = millis();
}

// ---------------------------------------------------------------------------
// @brief Force une synchronisation immediate
// @param void
// @return void
// ---------------------------------------------------------------------------
void DS3231forcerSynchronisation(void)
{
  Serial.println("=== SYNCHRONISATION FORCEE ===");
  DS3231copyTimeToMicro(true);
}
