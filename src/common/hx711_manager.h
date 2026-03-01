// hx711_manager.h — Lecture cellule de charge HX711 (1 par noeud)
// Phase 1 — common master + slave
// ESP32-S3 : 1 seule cellule 200 kg par noeud

#ifndef HX711_MANAGER_H
#define HX711_MANAGER_H

#include <Arduino.h>
#include <HX711.h>
#include <stdint.h>

// ===== INITIALISATION =====

// Initialise le HX711 avec les pins et la calibration du noeud courant
void hx711Init(void);

// ===== LECTURE =====

// Retourne la valeur brute moyenne (samples lectures)
float hx711ReadRaw(uint8_t samples);

// Retourne le poids en grammes (avec calibration)
float hx711GetWeightGrams(void);

// ===== CALIBRATION =====

// Pose une tare a vide (enregistre la valeur courante comme zero)
void hx711Tare(void);

// Calcule l'echelle depuis un poids de reference connu (en grammes)
float hx711CalcScale(float poidsRef_g);

// ===== GESTION ENERGIE =====

// Met le HX711 en veille (< 1 uA)
void hx711PowerDown(void);

// Reveille le HX711 (attente 400 ms stabilisation requise)
void hx711PowerUp(void);

// Verifie si le HX711 repond (timeout 2s)
bool hx711IsReady(void);

// ===== OBJET GLOBAL (extern pour acces depuis menus) =====
extern HX711 scale;

#endif // HX711_MANAGER_H
