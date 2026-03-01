// hx711_manager.cpp — Lecture cellule de charge HX711 (1 par noeud)
// Phase 1 — common master + slave
// Port de Mesures.cpp (POC_ATSAMD) — simplifie : 1 seule cellule par noeud

#include <Arduino.h>
#include <HX711.h>
#include "hx711_manager.h"
#include "types.h"
#include "config.h"

// ===== OBJET GLOBAL =====
HX711 scale;

// ===== VARIABLES EXTERNES =====
extern ConfigGenerale_t config;
extern HiveSensor_Data_t HiveSensor_Data;

// ---------------------------------------------------------------------------
// @brief Initialise le HX711 avec les pins et la calibration du noeud
// @param void
// @return void
// ---------------------------------------------------------------------------
void hx711Init(void)
{
  scale.begin(PIN_HX711_DOUT, PIN_HX711_SCK);

  if (!hx711IsReady())
  {
    LOG_ERROR("HX711 non detecte");
    return;
  }

  // Appliquer la calibration depuis la config persistante
  scale.set_offset((long)config.materiel.HX711NoloadValue_0);
  scale.set_scale(config.materiel.HX711Scaling_0);

  char buf[48];
  snprintf(buf, sizeof(buf), "HX711 OK tare=%.0f scale=%.4f",
           config.materiel.HX711NoloadValue_0,
           config.materiel.HX711Scaling_0);
  LOG_INFO(buf);
}

// ---------------------------------------------------------------------------
// @brief Retourne la valeur brute moyenne sur N lectures
// @param samples Nombre de lectures a moyenner (typiquement 3 ou 10)
// @return float Valeur brute moyennee
// ---------------------------------------------------------------------------
float hx711ReadRaw(uint8_t samples)
{
  if (!hx711IsReady())
  {
    LOG_ERROR("HX711 non pret");
    return 0.0f;
  }
  return (float)scale.read_average(samples);
}

// ---------------------------------------------------------------------------
// @brief Retourne le poids en grammes avec la calibration courante
// @param void
// @return float Poids en grammes (positif)
// ---------------------------------------------------------------------------
float hx711GetWeightGrams(void)
{
  if (!hx711IsReady())
  {
    return 0.0f;
  }

  float raw = (float)scale.read_average(HX711_AVR_CYCLE);
  float tare  = config.materiel.HX711NoloadValue_0;
  float echelle = config.materiel.HX711Scaling_0;

  if (echelle == 0.0f)
  {
    LOG_WARNING("HX711 echelle=0, calibration absente");
    return 0.0f;
  }

  float poids_g = (raw - tare) / echelle;
  HiveSensor_Data.HX711Weight[0] = poids_g;
  return poids_g;
}

// ---------------------------------------------------------------------------
// @brief Pose une tare a vide
// @param void
// @return void
// ---------------------------------------------------------------------------
void hx711Tare(void)
{
  if (!hx711IsReady())
  {
    LOG_ERROR("HX711 non pret pour tare");
    return;
  }

  // Moyenne sur HX711_NB_LECTURES pour la tare
  float valTare = hx711ReadRaw(HX711_NB_LECTURES);
  config.materiel.HX711NoloadValue_0 = valTare;
  scale.set_offset((long)valTare);

  char buf[40];
  snprintf(buf, sizeof(buf), "Tare effectuee: %.0f", valTare);
  LOG_INFO(buf);
}

// ---------------------------------------------------------------------------
// @brief Calcule le coefficient d'echelle depuis un poids de reference
// @param poidsRef_g Poids de reference pose sur la balance (en grammes)
// @return float Coefficient d'echelle calcule
// ---------------------------------------------------------------------------
float hx711CalcScale(float poidsRef_g)
{
  if (poidsRef_g <= 0.0f || !hx711IsReady())
  {
    LOG_ERROR("CalcScale: parametres invalides");
    return 0.0f;
  }

  float valCharge = hx711ReadRaw(HX711_NB_LECTURES);
  float tare      = config.materiel.HX711NoloadValue_0;
  float echelle   = (valCharge - tare) / poidsRef_g;

  config.materiel.HX711Scaling_0 = echelle;
  scale.set_scale(echelle);

  char buf[64];
  snprintf(buf, sizeof(buf),
           "Echelle calculee: valVide=%.0f valCharge=%.0f echelle=%.4f",
           tare, valCharge, echelle);
  LOG_INFO(buf);

  return echelle;
}

// ---------------------------------------------------------------------------
// @brief Met le HX711 en veille (<1 uA)
// @param void
// @return void
// ---------------------------------------------------------------------------
void hx711PowerDown(void)
{
  scale.power_down();
}

// ---------------------------------------------------------------------------
// @brief Reveille le HX711 (stabilisation 400 ms obligatoire)
// @param void
// @return void
// ---------------------------------------------------------------------------
void hx711PowerUp(void)
{
  scale.power_up();
  delay(400); // stabilisation obligatoire apres power up
}

// ---------------------------------------------------------------------------
// @brief Verifie si le HX711 est pret (timeout 2s)
// @param void
// @return bool true si pret
// ---------------------------------------------------------------------------
bool hx711IsReady(void)
{
  return scale.wait_ready_timeout(2000);
}
