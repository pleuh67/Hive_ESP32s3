// keypad.cpp — Clavier analogique 5 touches (ADC 12 bits ESP32-S3)
// Phase 1 — common master + slave
// Porte depuis KEYPAD.cpp (POC_ATSAMD)
// Diff : analogRead 12 bits (0..4095) vs 10 bits (0..1023) sur ATSAMD

#include <Arduino.h>
#include "keypad.h"
#include "types.h"
#include "config.h"

// ===== VARIABLES GLOBALES =====
// clavierContext et touche sont definis dans saisies_nb.cpp
// (ils sont extern'd depuis ce fichier)
// Note : si saisies_nb.cpp n'est pas inclus, definir ici.

// ---------------------------------------------------------------------------
// @brief Configure le pin ADC du clavier analogique
// @param void
// @return void
// ---------------------------------------------------------------------------
void keypadInit(void)
{
  // Sur ESP32, analogRead fonctionne par defaut en 12 bits (0..4095)
  analogSetAttenuation(ADC_11db); // plage 0..3.3V
  analogReadResolution(12);
  LOG_INFO("Clavier analogique initialise (ADC 12 bits)");
}

// ---------------------------------------------------------------------------
// @brief Lecture instantanee d'une touche du clavier analogique
// @param void
// @return key_code_t Code de la touche ou KEY_NONE si aucune
// ---------------------------------------------------------------------------
key_code_t readKeyOnce(void)
{
  static const key_code_t keycodes[NB_KEYS] = {KEY_1, KEY_2, KEY_3, KEY_4, KEY_5};
  uint16_t val = (uint16_t)analogRead(PIN_KBD_ANA);

  // Aucune touche : valeur superieure au dernier niveau + tolerance
  if (val > KBD_LEVELS[NB_KEYS - 1] + KBD_TOLERANCE)
  {
    return KEY_NONE;
  }

  // Identifier la touche par proximite avec les niveaux ADC
  for (uint8_t i = 0; i < NB_KEYS; i++)
  {
    int16_t diff = (int16_t)val - (int16_t)KBD_LEVELS[i];
    if (diff < 0) diff = -diff;
    if (diff <= KBD_TOLERANCE)
    {
      return keycodes[i];
    }
  }

  // Valeur non identifiable
  char buf[32];
  snprintf(buf, sizeof(buf), "KEY_INVALID: %u (max=%u)", val,
           (uint16_t)(KBD_LEVELS[NB_KEYS - 1] + KBD_TOLERANCE));
  LOG_WARNING(buf);
  return KEY_INVALID;
}

// ---------------------------------------------------------------------------
// @brief Traitement anti-rebond non-bloquant (a appeler dans loop)
// @param void
// @return void
// ---------------------------------------------------------------------------
void processContinuousKeyboard(void)
{
  unsigned long currentTime = millis();

  // Vérifier l'intervalle minimum entre lectures
  if (currentTime - clavierContext.derniereLecture < DEBOUNCE_DELAY_MS)
  {
    return;
  }
  clavierContext.derniereLecture = currentTime;

  key_code_t toucheActuelle = readKeyOnce();

  if (toucheActuelle == clavierContext.derniereTouche)
  {
    clavierContext.compteStable++;

    if (clavierContext.compteStable >= DEBOUNCE_COUNT)
    {
      if (toucheActuelle != clavierContext.toucheStable)
      {
        clavierContext.toucheStable = toucheActuelle;
        if (toucheActuelle != KEY_NONE)
        {
          clavierContext.toucheDisponible = true;
        }
      }
    }
  }
  else
  {
    clavierContext.derniereTouche = toucheActuelle;
    clavierContext.compteStable   = 0;
  }
}

// ---------------------------------------------------------------------------
// @brief Retourne la touche stabilisee sans attendre (non-bloquant)
// @param void
// @return key_code_t Touche disponible ou KEY_NONE
// ---------------------------------------------------------------------------
key_code_t readKeyNonBlocking(void)
{
  if (clavierContext.toucheDisponible)
  {
    clavierContext.toucheDisponible = false;
    return clavierContext.toucheStable;
  }
  return KEY_NONE;
}

// ---------------------------------------------------------------------------
// @brief Convertit un code de touche en chaine lisible
// @param key Code de touche
// @return const char* Nom lisible
// ---------------------------------------------------------------------------
const char* keyToString(key_code_t key)
{
  switch (key)
  {
    case KEY_NONE:    return "NONE";
    case KEY_1:       return "KEY_1";
    case KEY_2:       return "KEY_2";
    case KEY_3:       return "KEY_3";
    case KEY_4:       return "KEY_4";
    case KEY_5:       return "KEY_5";
    case KEY_INVALID: return "INVALID";
    default:          return "UNKNOWN";
  }
}
