// menus_common.cpp — Navigation menus + menus communs (M01, M04)
// Phase 1 — common master + slave
//
// Mecanisme :
//   pushMenu()           → empile le menu et lance startListInput()
//   processListInput()   → detecte VALIDE → processMenuSelection(idx)
//   processMenuSelection → dispatche vers le handler (ou processExtraMenuSelection)
//   Handler              → soit startXxxInput (saisieActive != 0)
//                          soit action directe + backMenu()
//                          soit pushMenu() (sous-menu)
//                          soit popMenu() (RET)
//   processActiveInputs  → conduit la machine a etats active depuis loop()
//
// saisieActive codes communs :
//   'D' = date (m01)    'H' = heure (m01)
//   'R' = num rucher    'N' = nom rucher (string)
//   'E' = echelle balance ref (numerique)

#include <Arduino.h>
#include <RTClib.h>
#include <stdlib.h>
#include <string.h>
#include "menus_common.h"
#include "saisies_nb.h"
#include "rtc_manager.h"
#include "hx711_manager.h"
#include "eeprom_manager.h"
#include "display_manager.h"
#include "types.h"
#include "config.h"

// ===== VARIABLES EXTERNES =====
extern ConfigGenerale_t  config;
extern HiveSensor_Data_t HiveSensor_Data;

// ===== MENU M01 — CONFIGURATION SYSTEME =====

const char* m01_ConfigSystem[] = {
  "Saisie DATE   (S)",
  "Saisie HEURE  (S)",
  "Num. RUCHER   (S)",
  "Nom  RUCHER   (S)",
  "Lire EEPROM   (F)",
  "Ecrire EEPROM (F)",
  "RET           (M)",
};
const uint8_t M01_SIZE = 7;

// ===== MENU M04 — CALIBRATION BALANCE =====
// ESP32 : 1 seule balance par noeud (simplifie vs ATSAMD 4 balances)

const char* m04_CalibBalance[] = {
  "Info Poids    (F)",
  "Tare Balance  (F)",
  "Echelle Bal.  (S)",
  "RET           (M)",
};
const uint8_t M04_SIZE = 4;

// ===========================================================================
// NAVIGATION
// ===========================================================================

// ---------------------------------------------------------------------------
// @brief Pousse un menu sur la pile et demarre l'affichage du selecteur
// ---------------------------------------------------------------------------
void pushMenu(const char* title, const char** list, uint8_t size, uint8_t initIdx)
{
  if (currentMenuDepth >= MAX_MENU_DEPTH)
  {
    OLEDDisplayMessageL8(">5 niveaux menu!", false, false);
    return;
  }
  menuStack[currentMenuDepth].menuList      = list;
  menuStack[currentMenuDepth].menuSize      = size;
  menuStack[currentMenuDepth].selectedIndex = initIdx;
  strncpy(menuStack[currentMenuDepth].title, title, 20);
  menuStack[currentMenuDepth].title[20] = '\0';
  currentMenuDepth++;
  startListInput(title, list, size, initIdx);
}

// ---------------------------------------------------------------------------
// @brief Retour au menu precedent (decremente la pile)
// ---------------------------------------------------------------------------
void popMenu(void)
{
  if (currentMenuDepth == 0) return;
  currentMenuDepth--;
  if (currentMenuDepth == 0) return; // Sortie du systeme menu
  menuLevel_t* prev = &menuStack[currentMenuDepth - 1];
  startListInput(prev->title, prev->menuList, prev->menuSize, prev->selectedIndex);
}

// ---------------------------------------------------------------------------
// @brief Rafraichit l'affichage du menu courant (apres une fonction)
// ---------------------------------------------------------------------------
void backMenu(void)
{
  if (currentMenuDepth == 0) return;
  menuLevel_t* cur = &menuStack[currentMenuDepth - 1];
  startListInput(cur->title, cur->menuList, cur->menuSize, cur->selectedIndex);
}

// ===========================================================================
// HANDLERS M01
// ===========================================================================

static void m01_0F_GetDate(void)
{
  DateTime now = rtc.now();
  char dateBuf[11];
  snprintf(dateBuf, sizeof(dateBuf), "%02u/%02u/%04u",
           now.day(), now.month(), now.year());
  saisieActive = 'D';
  startDateInput("Saisie DATE", dateBuf);
}

static void m01_1F_GetTime(void)
{
  DateTime now = rtc.now();
  char timeBuf[9];
  snprintf(timeBuf, sizeof(timeBuf), "%02u:%02u:%02u",
           now.hour(), now.minute(), now.second());
  saisieActive = 'H';
  startTimeInput("Saisie HEURE", timeBuf);
}

static void m01_2F_GetNumRucher(void)
{
  char numBuf[5];
  snprintf(numBuf, sizeof(numBuf), "%u", config.applicatif.RucherID);
  saisieActive = 'R';
  startNumInput("Num. RUCHER", numBuf, 3, false, false, 0, 99);
}

static void m01_3F_GetNameRucher(void)
{
  saisieActive = 'N';
  startStringInput("Nom RUCHER", config.applicatif.RucherName, 20);
}

static void m01_4F_readConfig(void)
{
  E24C32loadConfig();
  OLEDDisplayMessageL8("Config lue EEPROM", false, false);
  backMenu();
}

static void m01_5F_writeConfig(void)
{
  E24C32saveConfig();
  OLEDDisplayMessageL8("Config sauvee OK", false, false);
  backMenu();
}

// ===========================================================================
// HANDLERS M04
// ===========================================================================

static void m04_0F_InfoPoids(void)
{
  char buf[22];
  float w = hx711GetWeightGrams();
  snprintf(buf, sizeof(buf), "Poids: %.0fg", w);
  OLEDDisplayMessageL8(buf, false, false);
  backMenu();
}

static void m04_1F_TareBalance(void)
{
  OLEDDisplayMessageL8("Tare en cours...", false, false);
  hx711Tare();
  E24C32saveConfig();
  char buf[22];
  snprintf(buf, sizeof(buf), "Tare:%.0f OK",
           config.materiel.HX711NoloadValue_0);
  OLEDDisplayMessageL8(buf, false, false);
  backMenu();
}

static void m04_2F_EchelleBal(void)
{
  // Demarre saisie du poids de reference en grammes
  saisieActive = 'E';
  startNumInput("Poids ref.(g)", "10000", 7, false, true, 1, 9999999);
}

// ===========================================================================
// DISPATCHER PRINCIPAL
// ===========================================================================

// ---------------------------------------------------------------------------
// @brief Dispatche la selection vers le handler du menu actif
// ---------------------------------------------------------------------------
void processMenuSelection(uint8_t idx)
{
  if (currentMenuDepth == 0) return;
  menuLevel_t* cur = &menuStack[currentMenuDepth - 1];
  cur->selectedIndex = idx;

  if (cur->menuList == m01_ConfigSystem)
  {
    switch (idx)
    {
      case 0: m01_0F_GetDate();       break;
      case 1: m01_1F_GetTime();       break;
      case 2: m01_2F_GetNumRucher();  break;
      case 3: m01_3F_GetNameRucher(); break;
      case 4: m01_4F_readConfig();    break;
      case 5: m01_5F_writeConfig();   break;
      case 6: popMenu();              break;
    }
    return;
  }

  if (cur->menuList == m04_CalibBalance)
  {
    switch (idx)
    {
      case 0: m04_0F_InfoPoids();   break;
      case 1: m04_1F_TareBalance(); break;
      case 2: m04_2F_EchelleBal();  break;
      case 3: popMenu();            break;
    }
    return;
  }

  // Menu non reconnu → deleger au code specifique (master/slave)
  if (!processExtraMenuSelection(cur->menuList, idx))
  {
    // Fallback : dernier item = RET → popMenu
    if (idx == (uint8_t)(cur->menuSize - 1)) popMenu();
  }
}

// ===========================================================================
// TRAITEMENT DES SAISIES ACTIVES (appel depuis loop)
// ===========================================================================

// ---------------------------------------------------------------------------
// @brief Conduit la machine a etats de la saisie active
//
// Distinction liste navigation / liste donnee :
//   saisieActive == 0 → liste = navigation menu → processMenuSelection(idx)
//   saisieActive != 0 → liste = selection donnee → processExtraInputComplete
// ---------------------------------------------------------------------------
void processActiveInputs(void)
{
  if (currentMenuDepth == 0) return;

  // --- Saisie LISTE ---
  if (isListInputActive())
  {
    listInputState_t s = processListInput();

    if (s == LIST_INPUT_COMPLETED)
    {
      uint8_t idx = finalizeListInput(NULL);
      if (saisieActive == 0)
      {
        // Navigation normale dans un menu
        processMenuSelection(idx);
      }
      else
      {
        // Selection de donnee dans une liste (ex: Spreading Factor)
        char idxStr[4];
        snprintf(idxStr, sizeof(idxStr), "%u", idx);
        processExtraInputComplete(saisieActive, idxStr);
        saisieActive = 0;
        backMenu();
      }
    }
    else if (s == LIST_INPUT_CANCELLED)
    {
      finalizeListInput(NULL);
      saisieActive = 0;
      if (currentMenuDepth > 1) popMenu();
      else                      currentMenuDepth = 0; // Sortie menu
    }
    return;
  }

  // --- Saisie DATE ---
  if (isDateInputActive())
  {
    dateInputState_t s = processDateInput();
    if (s == DATE_INPUT_COMPLETED)
    {
      char dateBuf[11];
      finalizeDateInput(dateBuf);
      // Parse JJ/MM/AAAA
      uint8_t  d = (uint8_t)((dateBuf[0]-'0')*10 + (dateBuf[1]-'0'));
      uint8_t  m = (uint8_t)((dateBuf[3]-'0')*10 + (dateBuf[4]-'0'));
      uint16_t y = (uint16_t)atoi(dateBuf + 6);
      DateTime now = rtc.now();
      rtc.adjust(DateTime(y, m, d, now.hour(), now.minute(), now.second()));
      OLEDDisplayMessageL8("Date regle OK", false, false);
      saisieActive = 0;
      backMenu();
    }
    else if (s == DATE_INPUT_CANCELLED)
    {
      cancelDateInput();
      saisieActive = 0;
      backMenu();
    }
    return;
  }

  // --- Saisie HEURE ---
  if (isTimeInputActive())
  {
    timeInputState_t s = processTimeInput();
    if (s == TIME_INPUT_COMPLETED)
    {
      char timeBuf[9];
      finalizeTimeInput(timeBuf);
      // Parse HH:MM:SS
      uint8_t h  = (uint8_t)((timeBuf[0]-'0')*10 + (timeBuf[1]-'0'));
      uint8_t mi = (uint8_t)((timeBuf[3]-'0')*10 + (timeBuf[4]-'0'));
      uint8_t sc = (uint8_t)((timeBuf[6]-'0')*10 + (timeBuf[7]-'0'));
      DateTime now = rtc.now();
      rtc.adjust(DateTime(now.year(), now.month(), now.day(), h, mi, sc));
      OLEDDisplayMessageL8("Heure regle OK", false, false);
      saisieActive = 0;
      backMenu();
    }
    else if (s == TIME_INPUT_CANCELLED)
    {
      cancelTimeInput();
      saisieActive = 0;
      backMenu();
    }
    return;
  }

  // --- Saisie CHAINE ---
  if (isStringInputActive())
  {
    stringInputState_t s = processStringInput();
    if (s == STRING_INPUT_COMPLETED)
    {
      char result[21] = {0};
      finalizeStringInput(result);
      if (saisieActive == 'N')
      {
        strncpy(config.applicatif.RucherName, result, 20);
        config.applicatif.RucherName[20] = '\0';
        E24C32saveConfig();
        OLEDDisplayMessageL8("Nom sauvegarde", false, false);
      }
      saisieActive = 0;
      backMenu();
    }
    else if (s == STRING_INPUT_CANCELLED)
    {
      cancelStringInput();
      saisieActive = 0;
      backMenu();
    }
    return;
  }

  // --- Saisie NUMERIQUE ---
  if (isNumInputActive())
  {
    numInputState_t s = processNumInput();
    if (s == NUM_INPUT_COMPLETED)
    {
      char result[21] = {0};
      finalizeNumInput(result);
      switch (saisieActive)
      {
        case 'R': // Num rucher
          config.applicatif.RucherID = (uint8_t)atoi(result);
          E24C32saveConfig();
          OLEDDisplayMessageL8("Rucher sauvegarde", false, false);
          break;

        case 'E': // Echelle balance : poids de reference saisi
          {
            OLEDDisplayMessageL8("Mesure en cours..", false, false);
            float refG   = atof(result);
            float echelle = hx711CalcScale(refG);
            E24C32saveConfig();
            char buf[22];
            snprintf(buf, sizeof(buf), "Ech:%.4f OK", echelle);
            OLEDDisplayMessageL8(buf, false, false);
          }
          break;

        default:
          processExtraInputComplete(saisieActive, result);
          break;
      }
      saisieActive = 0;
      backMenu();
    }
    else if (s == NUM_INPUT_CANCELLED)
    {
      cancelNumInput();
      saisieActive = 0;
      backMenu();
    }
    return;
  }

  // --- Saisie HEXADECIMALE ---
  if (isHexInputActive())
  {
    hexInputState_t s = processHexInput();
    if (s == HEX_INPUT_COMPLETED)
    {
      char result[41] = {0};
      finalizeHexInput(result);
      processExtraInputComplete(saisieActive, result);
      saisieActive = 0;
      backMenu();
    }
    else if (s == HEX_INPUT_CANCELLED)
    {
      cancelHexInput();
      saisieActive = 0;
      backMenu();
    }
    return;
  }
}

// ===========================================================================
// IMPLEMENTATIONS WEAK (surcharger dans menus_master / menus_slave)
// ===========================================================================

void __attribute__((weak)) initMenuSystem(void)
{
  LOG_WARNING("initMenuSystem: pas de menu specifique defini");
}

bool __attribute__((weak)) processExtraMenuSelection(const char** menuList, uint8_t idx)
{
  (void)menuList;
  (void)idx;
  return false;
}

bool __attribute__((weak)) processExtraInputComplete(char saisieCode, const char* result)
{
  (void)saisieCode;
  (void)result;
  return false;
}
