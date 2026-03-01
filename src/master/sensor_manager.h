// sensor_manager.h — Capteurs I2C master : BME280, BH1750, INA219
// Phase 1 — master uniquement
// Nouveau : remplace DHT22 (Temp/Hum) + LDR (Lum) du POC_ATSAMD

#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include <stdint.h>

// ===== INITIALISATION =====

// Initialise tous les capteurs du master (BME280, BH1750, INA219)
// Retourne false si un capteur est absent
bool sensorsInit(void);

// ===== LECTURE =====

// Lit tous les capteurs et stocke dans HiveSensor_Data
// Retourne false si au moins un capteur a echoue
bool sensorsReadAll(void);

// Lecture individuelle
bool sensorReadBME280(void);   // T/HR/Pression -> HiveSensor_Data
bool sensorReadBH1750(void);   // Luminosite (lux) -> HiveSensor_Data
bool sensorReadINA219(void);   // Courant solaire (mA) -> HiveSensor_Data
bool sensorReadVBat(void);     // Tension batterie (V) -> HiveSensor_Data
bool sensorReadVSol(void);     // Tension solaire (V) -> HiveSensor_Data

// ===== DEBUG =====

// Affiche toutes les valeurs sur Serial
void sensorsPrintAll(void);

#endif // SENSOR_MANAGER_H
