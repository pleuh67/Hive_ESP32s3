// ble_slave.h — Serveur GATT BLE slave ESP32-S3
// Phase 2 : advertising + exposition poids, VBat, timestamp au master
//
// Encodage des caracteristiques :
//   WEIGHT    : int16_t  poids en 0.01 kg (ex: 20000 = 200.00 kg)
//   VBAT      : uint8_t  (V - 2.0) * 10    (ex: 17 = 3.7 V)
//   TIMESTAMP : uint32_t epoch Unix (DS3231)

#ifndef BLE_SLAVE_H
#define BLE_SLAVE_H

#include <stdint.h>
#include <stdbool.h>

// Initialise le serveur NimBLE et definit le service GATT avec les valeurs
// courantes. A appeler apres les mesures HX711 et VBat.
//   weight_raw : poids / 10 en entier (unite 0.01 kg)
//   vbat_enc   : (V - 2.0) * 10 encode en uint8_t
//   timestamp  : epoch Unix depuis DS3231
void bleSlaveInit(int16_t weight_raw, uint8_t vbat_enc, uint32_t timestamp);

// Lance l'advertising BLE (non bloquant).
// S'arrete automatiquement apres BLE_ADVERTISING_SEC secondes.
void bleSlaveStart(void);

// Retourne true si la session est terminee :
//   - le master s'est connecte et deconnecte (donnees lues), OU
//   - le timeout d'advertising a expire
bool bleSlaveIsComplete(void);

// Arrete l'advertising et libere les ressources NimBLE
void bleSlaveStop(void);

#endif // BLE_SLAVE_H
