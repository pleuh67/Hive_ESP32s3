// ble_master.h — Client GATT BLE master ESP32-S3
// Phase 2 : scan + collecte donnees des slaves
//
// Protocole :
//   1. Scan BLE passif (BLE_SCAN_SEC secondes)
//   2. Pour chaque slave trouve (service UUID), connexion + lecture 3 chars
//   3. Remplissage de SlaveReading_t readings[]

#ifndef BLE_MASTER_H
#define BLE_MASTER_H

#include <stdint.h>
#include "types.h"

// Duree du scan de decouverte des slaves (secondes)
// Doit etre inferieure a BLE_ADVERTISING_SEC pour laisser le temps de se connecter
#define BLE_SCAN_SEC  10

// Scanne le reseau BLE et collecte les donnees des slaves.
// Bloquant jusqu'a la fin du scan + connexions.
//   readings : tableau de SlaveReading_t a remplir (taille count)
//   count    : nombre de slaves attendus (generalement NUM_SLAVES)
void bleMasterCollect(SlaveReading_t* readings, uint8_t count);

#endif // BLE_MASTER_H
