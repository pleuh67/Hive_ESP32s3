// keypad.h — Clavier analogique 5 touches (ADC 12 bits)
// Phase 1 — common master + slave
// Porte depuis KEYPAD.cpp (POC_ATSAMD)
// Adaptation : analogRead 10 bits -> 12 bits (KBD_LEVELS dans config.h)

#ifndef KEYPAD_H
#define KEYPAD_H

#include <Arduino.h>
#include "types.h"

// ===== INITIALISATION =====

// Configure le pin ADC du clavier
void keypadInit(void);

// ===== LECTURE =====

// Lecture instantanee (sans anti-rebond)
key_code_t readKeyOnce(void);

// Lecture non-bloquante (retourne la touche stabilisee ou KEY_NONE)
key_code_t readKeyNonBlocking(void);

// ===== TRAITEMENT (appel dans loop) =====

// Traite le clavier en continu (anti-rebond non-bloquant)
// A appeler a chaque iteration de loop()
void processContinuousKeyboard(void);

// ===== UTILITAIRES =====

// Convertit un code de touche en chaine lisible
const char* keyToString(key_code_t key);

// ===== CONTEXTE CLAVIER (extern pour saisies_nb.cpp) =====
extern clavier_context_t clavierContext;
extern key_code_t touche;

#endif // KEYPAD_H
