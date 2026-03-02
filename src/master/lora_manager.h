// lora_manager.h — Couche LoRaWAN master ESP32-S3
// Phase 3 : SX1262 E22-900M22S + RadioLib 7.x, OTAA, EU868
//
// Payload V2 (24 octets) — voir CLAUDE.md pour la specification complete
// Session LoRaWAN persistee en NVM (EEPROM ESP32) — survive au deep sleep

#ifndef LORA_MANAGER_H
#define LORA_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "types.h"

// Initialise le bus SPI et le module SX1262.
// A appeler une fois dans setup() avant toute autre fonction lora*.
// Retourne true si le module repond correctement.
bool loraInit(void);

// Effectue un join OTAA ou restaure la session depuis la NVM.
// Bloquant : jusqu'a 30 s lors d'un premier join (sans session stockee).
// Rapide (<1 s) si une session valide est disponible en NVM.
// Retourne true si la session est active et prete pour l'envoi.
bool loraJoin(void);

// Retourne true si la session LoRaWAN est active (join reussi ou restaure).
bool loraIsJoined(void);

// Construit le payload V2 (24 octets) et l'envoie via LoRaWAN.
// Bloquant : TX + fenetres RX1/RX2 (~2-5 s selon SF).
// Retourne true si le reseau a acquitte (ou ACK implicite pour non confirme).
//   data       : mesures master (capteurs + HX711)
//   slaves     : tableau des lectures BLE (NUM_SLAVES entrees)
//   slaveCount : nombre d'entrees dans slaves
bool loraSendPayload(const HiveSensor_Data_t* data,
                     const SlaveReading_t*    slaves,
                     uint8_t                  slaveCount);

// Copie le dernier message de statut LoRa (join/send) dans buf.
// Utile pour affichage OLED.
void loraGetStatus(char* buf, uint8_t bufLen);

#endif // LORA_MANAGER_H
