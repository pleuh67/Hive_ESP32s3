// menus_master.cpp — Menus specifiques master ESP32-S3
// Phase 1 — master uniquement
//
// Surcharge les fonctions weak de menus_common :
//   initMenuSystem()            → appelle initMasterMenuSystem()
//   processExtraMenuSelection() → gere M0, M02, M03
//   processExtraInputComplete() → gere 'B','V','K','U','P','F'

#include <Arduino.h>
#include <RTClib.h>
#include <stdlib.h>
#include <string.h>
#include "menus_master.h"
#include "common/menus_common.h"
#include "common/saisies_nb.h"
#include "common/rtc_manager.h"
#include "common/eeprom_manager.h"
#include "common/display_manager.h"
#include "common/keypad.h"
#include "common/power_manager.h"
#include "sensor_manager.h"
#include "types.h"
#include "config.h"

// ===== VARIABLES EXTERNES =====
extern ConfigGenerale_t  config;
extern HiveSensor_Data_t HiveSensor_Data;

// ===========================================================================
// LISTES DE DONNEES
// ===========================================================================

static const char* List_SF[] = {
  "SF7", "SF9", "SF12",
};
static const uint8_t LIST_SF_SIZE = 3;

// Correspondance index liste -> valeur SF
static const uint8_t SF_VALUES[]  = {7, 9, 12};

// ===========================================================================
// MENU M0 — PRINCIPAL (5 items)
// ===========================================================================

static const char* m0_Demarrage[] = {
  "Page INFOS    (P)",
  "CONFIG. SYST  (M)",
  "CONFIG. LoRa  (M)",
  "CALIB. Tens.  (M)",
  "CALIB. Bal.   (M)",
};
static const uint8_t M0_SIZE = 5;

// ===========================================================================
// MENU M02 — CONFIG LoRa (8 items)
// ===========================================================================

static const char* m02_ConfigLoRa[] = {
  "Infos LoRa    (P)",
  "AppKEY        (S)",
  "AppEUI        (S)",
  "Spread Factor (S)",
  "Periode envoi (S)",
  "Join LoRa     (F)",
  "Envoyer Pay.  (F)",
  "RET           (M)",
};
static const uint8_t M02_SIZE = 8;

// ===========================================================================
// MENU M03 — CALIBRATION TENSIONS (3 items)
// ===========================================================================

static const char* m03_CalibTensions[] = {
  "Calib. VBat   (S)",
  "Calib. VSol   (S)",
  "RET           (M)",
};
static const uint8_t M03_SIZE = 3;

// ===========================================================================
// HELPERS
// ===========================================================================

// Convertit un tableau d'octets en chaine hexadecimale (majuscules)
static void bytesToHex(const uint8_t* src, uint8_t len, char* dst)
{
  for (uint8_t i = 0; i < len; i++)
    snprintf(dst + i * 2, 3, "%02X", src[i]);
  dst[len * 2] = '\0';
}

// Attente bloquante jusqu'a touche VALIDE ou timeout (ms)
// Utilise dans les pages info pour laisser le temps de lire avant backMenu
static void waitValideOrTimeout(uint32_t timeoutMs)
{
  uint32_t t0 = millis();
  while (millis() - t0 < timeoutMs)
  {
    processContinuousKeyboard();
    key_code_t k = readKeyNonBlocking();
    if (k == VALIDE) break;
  }
}

// ===========================================================================
// HANDLERS M0
// ===========================================================================

static void m0_0E_PageInfosSyst(void)
{
  DateTime now = rtc.now();
  char buf[22];

  displayClear();
  displayText(0, 0, "=== MASTER INFO ===");
  displayText(0, 1, VERSION);

  snprintf(buf, sizeof(buf), "Carte #%u  Boot#%lu",
           config.materiel.Num_Carte, (unsigned long)bootCount);
  displayText(0, 2, buf);

  snprintf(buf, sizeof(buf), "%02u:%02u %02u/%02u/%02u",
           now.hour(), now.minute(),
           now.day(), now.month(), (uint8_t)(now.year() % 100));
  displayText(0, 3, buf);

  snprintf(buf, sizeof(buf), "T:%.1fC HR:%.0f%%",
           HiveSensor_Data.DHT_Temp, HiveSensor_Data.DHT_Hum);
  displayText(0, 4, buf);

  snprintf(buf, sizeof(buf), "%.0fg Vb:%.2fV",
           HiveSensor_Data.HX711Weight[0], HiveSensor_Data.Bat_Voltage);
  displayText(0, 5, buf);

  snprintf(buf, sizeof(buf), "Rucher: %s", config.applicatif.RucherName);
  displayText(0, 6, buf);

  displayText(0, 7, "VALIDE -> retour");
  displayFlush();

  waitValideOrTimeout(TIMEOUT_SAISIE);
  backMenu();
}

// ===========================================================================
// HANDLERS M02
// ===========================================================================

static void m02_0E_InfosLoRa(void)
{
  char hexBuf[17];
  char buf[22];

  displayClear();
  displayText(0, 0, "=== CONFIG LoRa ===");

  snprintf(buf, sizeof(buf), "SF%u T=%us",
           config.applicatif.SpreadingFactor,
           config.applicatif.SendingPeriod);
  displayText(0, 1, buf);

  // AppEUI (8 octets = 16 hex chars)
  bytesToHex(config.applicatif.AppEUI, 8, hexBuf);
  hexBuf[8] = '\0'; // Tronquer pour affichage (8 chars max)
  snprintf(buf, sizeof(buf), "EUI: %s", hexBuf);
  displayText(0, 2, buf);

  displayText(0, 3, "AppKey: voir EEPROM");
  displayText(0, 7, "VALIDE -> retour");
  displayFlush();

  waitValideOrTimeout(TIMEOUT_SAISIE);
  backMenu();
}

static void m02_5F_JoinLoRa(void)
{
  OLEDDisplayMessageL8("Join: Phase 3", false, false);
  backMenu();
}

static void m02_6F_SendPayload(void)
{
  OLEDDisplayMessageL8("Send: Phase 3", false, false);
  backMenu();
}

// ===========================================================================
// HANDLERS M03
// ===========================================================================

static void m03_0F_CalibVBat(void)
{
  char buf[12];
  // Afficher la valeur actuelle (config ou liste selon Num_Carte)
  uint8_t c = (config.materiel.Num_Carte < 10) ? config.materiel.Num_Carte : 0;
  float cur = (config.materiel.VBatScale != 0.0f)
              ? config.materiel.VBatScale
              : VBatScale_List[c];
  snprintf(buf, sizeof(buf), "%.7f", cur);
  saisieActive = 'B';
  startNumInput("VBat Scale", buf, 9, false, true, 0, 100);
}

static void m03_1F_CalibVSol(void)
{
  char buf[12];
  uint8_t c = (config.materiel.Num_Carte < 10) ? config.materiel.Num_Carte : 0;
  float cur = (config.materiel.VSolScale != 0.0f)
              ? config.materiel.VSolScale
              : VSolScale_List[c];
  snprintf(buf, sizeof(buf), "%.7f", cur);
  saisieActive = 'V';
  startNumInput("VSol Scale", buf, 9, false, true, 0, 100);
}

// ===========================================================================
// POINT D'ENTREE
// ===========================================================================

void initMasterMenuSystem(void)
{
  pushMenu("MENU PRINCIPAL", m0_Demarrage, M0_SIZE, 0);
}

// Surcharge initMenuSystem() (weak dans menus_common)
void initMenuSystem(void)
{
  initMasterMenuSystem();
}

// ===========================================================================
// DISPATCHER MASTER (surcharge processExtraMenuSelection weak)
// ===========================================================================

bool processExtraMenuSelection(const char** menuList, uint8_t idx)
{
  // --- M0 Menu principal ---
  if (menuList == m0_Demarrage)
  {
    switch (idx)
    {
      case 0: m0_0E_PageInfosSyst(); break;
      case 1: pushMenu("CONFIG. SYSTEME", m01_ConfigSystem, M01_SIZE, 0); break;
      case 2: pushMenu("CONFIG. LoRa",    m02_ConfigLoRa,   M02_SIZE, 0); break;
      case 3: pushMenu("CALIB. TENSIONS", m03_CalibTensions, M03_SIZE, 0); break;
      case 4: pushMenu("CALIB. BALANCE",  m04_CalibBalance,  M04_SIZE, 0); break;
    }
    return true;
  }

  // --- M02 Config LoRa ---
  if (menuList == m02_ConfigLoRa)
  {
    switch (idx)
    {
      case 0: m02_0E_InfosLoRa(); break;

      case 1: // AppKEY (16 octets = 32 hex chars)
        {
          char hexBuf[33];
          bytesToHex(config.applicatif.AppKey, 16, hexBuf);
          saisieActive = 'K';
          startHexInput("AppKEY (32 hex)", hexBuf, 32);
        }
        break;

      case 2: // AppEUI (8 octets = 16 hex chars)
        {
          char hexBuf[17];
          bytesToHex(config.applicatif.AppEUI, 8, hexBuf);
          saisieActive = 'U';
          startHexInput("AppEUI (16 hex)", hexBuf, 16);
        }
        break;

      case 3: // Spreading Factor (selection dans liste)
        saisieActive = 'F';
        startListInput("Spread Factor", List_SF, LIST_SF_SIZE, 0);
        break;

      case 4: // Periode d'envoi en secondes
        {
          char buf[8];
          snprintf(buf, sizeof(buf), "%u", config.applicatif.SendingPeriod);
          saisieActive = 'P';
          startNumInput("Periode (s)", buf, 5, false, false, 60, 3600);
        }
        break;

      case 5: m02_5F_JoinLoRa();   break;
      case 6: m02_6F_SendPayload(); break;
      case 7: popMenu();            break;
    }
    return true;
  }

  // --- M03 Calib Tensions ---
  if (menuList == m03_CalibTensions)
  {
    switch (idx)
    {
      case 0: m03_0F_CalibVBat(); break;
      case 1: m03_1F_CalibVSol(); break;
      case 2: popMenu();          break;
    }
    return true;
  }

  return false; // Menu non reconnu
}

// ===========================================================================
// COMPLETION SAISIES MASTER (surcharge processExtraInputComplete weak)
// ===========================================================================

bool processExtraInputComplete(char saisieCode, const char* result)
{
  switch (saisieCode)
  {
    case 'B': // VBat scale
      config.materiel.VBatScale = atof(result);
      E24C32saveConfig();
      OLEDDisplayMessageL8("VBat scale sauve", false, false);
      return true;

    case 'V': // VSol scale
      config.materiel.VSolScale = atof(result);
      E24C32saveConfig();
      OLEDDisplayMessageL8("VSol scale sauve", false, false);
      return true;

    case 'K': // AppKey (32 hex chars -> 16 octets)
      for (uint8_t i = 0; i < 16; i++)
      {
        char hex[3] = {result[i * 2], result[i * 2 + 1], '\0'};
        config.applicatif.AppKey[i] = (uint8_t)strtol(hex, NULL, 16);
      }
      config.applicatif.AppKey[16] = 0;
      E24C32saveConfig();
      OLEDDisplayMessageL8("AppKey sauvegarde", false, false);
      return true;

    case 'U': // AppEUI (16 hex chars -> 8 octets)
      for (uint8_t i = 0; i < 8; i++)
      {
        char hex[3] = {result[i * 2], result[i * 2 + 1], '\0'};
        config.applicatif.AppEUI[i] = (uint8_t)strtol(hex, NULL, 16);
      }
      config.applicatif.AppEUI[8] = 0;
      E24C32saveConfig();
      OLEDDisplayMessageL8("AppEUI sauvegarde", false, false);
      return true;

    case 'P': // Periode d'envoi (secondes)
      config.applicatif.SendingPeriod = (uint16_t)atoi(result);
      E24C32saveConfig();
      OLEDDisplayMessageL8("Periode sauvee", false, false);
      return true;

    case 'F': // Spreading Factor (index dans List_SF -> valeur SF)
      {
        uint8_t sfIdx = (uint8_t)atoi(result);
        if (sfIdx < sizeof(SF_VALUES))
        {
          config.applicatif.SpreadingFactor = SF_VALUES[sfIdx];
          E24C32saveConfig();
          char buf[18];
          snprintf(buf, sizeof(buf), "SF%u sauvegarde",
                   config.applicatif.SpreadingFactor);
          OLEDDisplayMessageL8(buf, false, false);
        }
      }
      return true;
  }

  return false;
}
