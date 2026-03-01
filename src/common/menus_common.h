// menus_common.h — Navigation menus + menus communs (M01, M04)
// Phase 1 — common master + slave
//
// Architecture : pile de navigation (menuStack) + machines a etats saisies_nb
//
// M01 Config Systeme : date, heure, num/nom rucher, EEPROM lire/ecrire
// M04 Calib Balance  : info poids, tare, echelle (1 seule balance par noeud)
//
// Points d'extension (weak) pour master/slave :
//   initMenuSystem()             → surcharger avec initMasterMenuSystem()
//   processExtraMenuSelection()  → M0, M02, M03, etc.
//   processExtraInputComplete()  → saisies specifiques master/slave

#ifndef MENUS_COMMON_H
#define MENUS_COMMON_H

#include <Arduino.h>
#include <stdint.h>
#include "types.h"

// ===== NAVIGATION =====

// Pousse un menu sur la pile et demarre la saisie liste correspondante
void pushMenu(const char* title, const char** list, uint8_t size, uint8_t initIdx);

// Retour au menu precedent (decremente la pile)
void popMenu(void);

// Rafraichit l'affichage du menu courant (apres execution d'une fonction)
void backMenu(void);

// Dispatche la selection (index) vers le handler du menu actif
void processMenuSelection(uint8_t idx);

// ===== BOUCLE =====

// Traite la saisie active courante — appeler depuis loop()
// Gere : liste (navigation), date, heure, chaine, numerique, hexadecimale
void processActiveInputs(void);

// ===== POINT D'ENTREE =====

// Demarre le systeme de menus (surcharger dans menus_master / menus_slave)
void initMenuSystem(void);

// ===== EXTENSION HOOKS (weak — surcharger dans menus_master/slave) =====

// Appele par processMenuSelection pour les menus non reconnus par common
// Retourne true si la selection a ete traitee
bool processExtraMenuSelection(const char** menuList, uint8_t idx);

// Appele par processActiveInputs quand une saisie se termine avec un code
// saisieActive non reconnu par common (ex: 'B' VBat, 'K' AppKey, 'F' SF)
// result : valeur saisie (chaine numerique, hex, ou index liste en ascii)
// Retourne true si la saisie a ete traitee
bool processExtraInputComplete(char saisieCode, const char* result);

// ===== MENUS COMMUNS (accessibles depuis menus_master/slave via pushMenu) =====

// M01 — Configuration Systeme (7 items)
extern const char* m01_ConfigSystem[];
extern const uint8_t M01_SIZE;

// M04 — Calibration Balance unique (4 items, ESP32 : 1 cellule par noeud)
extern const char* m04_CalibBalance[];
extern const uint8_t M04_SIZE;

// ===== VARIABLES GLOBALES (definies dans saisies_nb.cpp) =====

extern menuLevel_t menuStack[];      // Pile de navigation (MAX_MENU_DEPTH)
extern uint8_t     currentMenuDepth; // Profondeur courante (0 = hors menu)
extern char        saisieActive;     // Code de la saisie en cours (0 = aucune)
extern key_code_t  touche;           // Touche courante pour les machines a etats

#endif // MENUS_COMMON_H
