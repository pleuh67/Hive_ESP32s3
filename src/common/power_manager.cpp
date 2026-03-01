// power_manager.cpp — Gestion energie et deep sleep ESP32-S3
// Phase 1 — common master + slave
// Reecriture complete depuis Power.cpp (POC_ATSAMD)
//
// Sur ATSAMD : LowPower.sleep() + ArduinoLowPower
// Sur ESP32  : esp_deep_sleep_start() + reveil EXT0 sur pin RTC

#include <Arduino.h>
#include <esp_sleep.h>
#include <driver/gpio.h>
#include "power_manager.h"
#include "config.h"
#include "hx711_manager.h"
#include "display_manager.h"
#include "rtc_manager.h"

// ===== ETAT PERSISTANT (survit au deep sleep) =====
RTC_DATA_ATTR uint32_t bootCount = 0;
RTC_DATA_ATTR bool     blePaired = false;

// ---------------------------------------------------------------------------
// @brief Entre en deep sleep, reveil par interrupt EXT0 sur pin RTC DS3231
//
// Ordre d'arret avant sleep :
//   1. Log + flush serie
//   2. HX711 power down
//   3. OLED off
//   4. Configurer reveil EXT0 (FALLING sur pin RTC, DS3231 tire LOW)
//   5. esp_deep_sleep_start()
//
// A la sortie du deep sleep, le programme repart depuis setup().
// ---------------------------------------------------------------------------
void powerDeepSleep(void)
{
  char buf[40];
  snprintf(buf, sizeof(buf), "=== DEEP SLEEP (boot #%lu) ===", (unsigned long)bootCount);
  Serial.println(buf);
  Serial.flush();

  // 1. HX711 en veille
  hx711PowerDown();

  // 2. OLED off
  u8g2.setPowerSave(1);

  // 3. Configurer le reveil EXT0 sur le pin INT du DS3231
  //    Le DS3231 tire la ligne SQW/INT a LOW quand l'alarme est active
  esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_RTC_INT, 0); // 0 = LOW

  // 4. Deep sleep
  esp_deep_sleep_start();

  // Cette ligne n'est jamais atteinte
}

// ---------------------------------------------------------------------------
// @brief Retourne la cause du reveil ESP32
// @param void
// @return esp_sleep_wakeup_cause_t
// ---------------------------------------------------------------------------
esp_sleep_wakeup_cause_t powerGetWakeupCause(void)
{
  return esp_sleep_get_wakeup_cause();
}

// ---------------------------------------------------------------------------
// @brief Affiche la cause du reveil sur Serial
// @param void
// @return void
// ---------------------------------------------------------------------------
void powerPrintWakeupCause(void)
{
  bootCount++;
  char buf[40];
  snprintf(buf, sizeof(buf), "=== REVEIL (boot #%lu) ===", (unsigned long)bootCount);
  Serial.println(buf);

  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  switch (cause)
  {
    case ESP_SLEEP_WAKEUP_EXT0:
      Serial.println("Cause: EXT0 (alarme RTC DS3231)");
      break;
    case ESP_SLEEP_WAKEUP_EXT1:
      Serial.println("Cause: EXT1");
      break;
    case ESP_SLEEP_WAKEUP_TIMER:
      Serial.println("Cause: timer interne");
      break;
    case ESP_SLEEP_WAKEUP_UNDEFINED:
      Serial.println("Cause: premier demarrage (pas de deep sleep precedent)");
      break;
    default:
      Serial.println("Cause: autre");
      break;
  }
}

// ---------------------------------------------------------------------------
// @brief Verifie si le reveil vient de l'alarme RTC
// @param void
// @return bool true si reveil EXT0 (DS3231)
// ---------------------------------------------------------------------------
bool powerWokeFromRTC(void)
{
  return (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0);
}
