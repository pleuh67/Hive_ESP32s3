// rtc_manager.h — Gestion RTC DS3231 + alarmes
// Porte depuis DS3231.cpp (POC_ATSAMD) — 90% portable
// Modifications : ISR avec IRAM_ATTR, pas de Serial/I2C dans ISR

#ifndef RTC_MANAGER_H
#define RTC_MANAGER_H

#include <Arduino.h>
#include <RTClib.h>

// ===== INITIALISATION =====

// Initialise le module RTC DS3231
void initRTC(void);

// ===== ALARMES =====

// Configure alarme 1 (toutes les secondes, mode programmation)
void DS3231setRTCAlarm1(void);

// Configure alarme 2 (payload toutes les X minutes)
void DS3231setRTCAlarm2(void);

// Efface les 2 alarmes du RTC
void DS3231clearRTCAlarms(void);

// ===== RESET =====

// Hard reset du RTC (registres de controle)
void DS3231hardReset(void);

// Reset complet du RTC (tous les registres)
void DS3231CompleteReset(void);

// ===== SYNCHRONISATION =====

// Synchronise l'heure du DS3231 vers le micro
void DS3231synchronizeTimeToMicro(void);

// Copie l'heure du DS3231 vers le micro avec option de forcage
void DS3231copyTimeToMicro(bool forcer);

// Force une synchronisation immediate
void DS3231forcerSynchronisation(void);

// ===== ISR + TRAITEMENT ALARMES =====

// Handler d'interruption RTC (alarme 1 ou 2)
// IRAM_ATTR obligatoire sur ESP32 — ne jamais appeler directement
void IRAM_ATTR onRTCAlarm(void);

// Traite les alarmes depuis la loop() (jamais depuis ISR)
void processRTCAlarms(void);

// ===== FLAGS VOLATILS (extern pour loop()) =====
extern volatile bool wakeupPayload;     // Alarme 2 : heure d'envoyer le payload
extern volatile bool wakeup1Sec;        // Alarme 1 : tick 1 seconde (mode programmation)
extern volatile bool displayNextPayload; // Rafraichir l'affichage prochain payload
extern volatile bool rtcAlarmFlag;      // Flag brut ISR -> loop

// ===== OBJET RTC (extern pour acces direct depuis d'autres modules) =====
extern RTC_DS3231 rtc;

#endif // RTC_MANAGER_H
