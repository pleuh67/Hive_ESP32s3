// main.cpp — Master ESP32-S3
// Ruches Connectees : WiFi + BLE + LoRaWAN + Serveur Web + Capteurs

#include <Arduino.h>
#include "types.h"
#include "config.h"

// Variables globales partagees entre les modules (extern dans les .cpp communs)
ConfigGenerale_t config;
HiveSensor_Data_t HiveSensor_Data;

void setup()
{
  Serial.begin(115200);
  delay(1000);
  Serial.println("=== Ruches Connectees — Master ===");
  Serial.println("Phase 0 : structure OK, compilation OK");
}

void loop()
{
  // TODO Phase 1 : lecture capteurs
  // TODO Phase 2 : collecte BLE slaves
  // TODO Phase 3 : envoi LoRaWAN
  // TODO Phase 4 : serveur web
}
