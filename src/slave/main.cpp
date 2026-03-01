// main.cpp — Slave ESP32-S3
// Ruches Connectees : BLE + HX711 + OLED debug + DS3231

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
  Serial.println("=== Ruches Connectees — Slave ===");
  Serial.println("Phase 0 : structure OK, compilation OK");
}

void loop()
{
  // TODO Phase 1 : lecture HX711 + VBat
  // TODO Phase 2 : BLE advertising + GATT server
}
