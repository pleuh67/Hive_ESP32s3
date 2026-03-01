// menus_master.h — Menus specifiques master ESP32-S3
// Phase 1 — master uniquement
//
// M0  Menu principal   : Infos, Config Systeme, Config LoRa, Calib Tensions, Calib Balance
// M02 Config LoRa      : AppKey, AppEUI, SF, periode, Join (stub), Envoyer (stub)
// M03 Calib Tensions   : VBat scale, VSol scale
//
// saisieActive codes master :
//   'B' = VBat scale (numerique)
//   'V' = VSol scale (numerique)
//   'K' = AppKey (hex 32 chars)
//   'U' = AppEUI (hex 16 chars)
//   'P' = Periode envoi (numerique)
//   'F' = Spreading Factor (liste → index)

#ifndef MENUS_MASTER_H
#define MENUS_MASTER_H

#include <Arduino.h>
#include <stdint.h>

// ===== POINT D'ENTREE =====

// Demarre le systeme de menus master (pousse M0 sur la pile)
// Appeler depuis main.cpp quand l'utilisateur presse une touche
void initMasterMenuSystem(void);

#endif // MENUS_MASTER_H
