// power_manager.h — Gestion energie et deep sleep ESP32-S3
// Phase 1 — common master + slave
// Reecriture complete (original utilisait ArduinoLowPower ATSAMD)
// Reveil par interrupt EXT0 sur pin RTC (DS3231 alarm)

#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <Arduino.h>
#include <esp_sleep.h>
#include <stdint.h>

// ===== DEEP SLEEP =====

// Entre en deep sleep avec reveil sur alarme RTC (pin INT DS3231)
// Avant d'appeler : configurer l'alarme DS3231, appeler hx711PowerDown()
void powerDeepSleep(void);

// ===== INFORMATION AU REVEIL =====

// Retourne la cause du reveil (a appeler en debut de setup)
esp_sleep_wakeup_cause_t powerGetWakeupCause(void);

// Affiche la cause du reveil sur Serial
void powerPrintWakeupCause(void);

// Retourne true si le reveil vient de l'alarme RTC
bool powerWokeFromRTC(void);

// ===== ETAT PERSISTANT EN RTC RAM =====
// Les variables RTC_DATA_ATTR survivent au deep sleep

// Compteur de cycles (incremente a chaque reveil)
extern RTC_DATA_ATTR uint32_t bootCount;

// Flag : pairing BLE effectue (evite de repairer a chaque cycle)
extern RTC_DATA_ATTR bool blePaired;

#endif // POWER_MANAGER_H
