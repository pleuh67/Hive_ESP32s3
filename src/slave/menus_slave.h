// menus_slave.h — Menus specifiques slave ESP32-S3
// Phase 1 — slave uniquement
//
// M0  Menu principal : Infos, Config Systeme, Calib Balance, Calib VBat
//
// saisieActive codes slave :
//   'B' = VBat scale (numerique)

#ifndef MENUS_SLAVE_H
#define MENUS_SLAVE_H

#include <Arduino.h>

// ===== POINT D'ENTREE =====

// Demarre le systeme de menus slave (pousse M0 sur la pile)
// Appeler depuis main.cpp quand l'utilisateur presse une touche
void initSlaveMenuSystem(void);

#endif // MENUS_SLAVE_H
