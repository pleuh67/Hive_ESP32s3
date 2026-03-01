// menus_slave.cpp — Menus specifiques slave ESP32-S3
// Phase 1 — slave uniquement
//
// Surcharge les fonctions weak de menus_common :
//   initMenuSystem()            → appelle initSlaveMenuSystem()
//   processExtraMenuSelection() → gere M0 slave
//   processExtraInputComplete() → gere 'B' VBat scale

#include <Arduino.h>
#include <RTClib.h>
#include <stdlib.h>
#include <string.h>
#include "menus_slave.h"
#include "common/menus_common.h"
#include "common/saisies_nb.h"
#include "common/rtc_manager.h"
#include "common/eeprom_manager.h"
#include "common/display_manager.h"
#include "common/keypad.h"
#include "common/power_manager.h"
#include "types.h"
#include "config.h"

// ===== VARIABLES EXTERNES =====
extern ConfigGenerale_t  config;
extern HiveSensor_Data_t HiveSensor_Data;

// ===========================================================================
// MENU M0 SLAVE — PRINCIPAL (4 items)
// ===========================================================================

static const char* m0_Demarrage_S[] = {
  "Page INFOS    (P)",   // case 0 → m0_0E_PageInfosSlave()
  "CONFIG. SYST  (M)",   // case 1 → pushMenu(m01_ConfigSystem)
  "CALIB. Bal.   (M)",   // case 2 → pushMenu(m04_CalibBalance)
  "CALIB. VBat   (S)",   // case 3 → m0_3S_CalibVBat()
};
static const uint8_t M0S_SIZE = 4;

// ===========================================================================
// HELPERS
// ===========================================================================

// Attente non bloquante jusqu'a touche VALIDE ou timeout (ms)
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
// HANDLERS M0 SLAVE
// ===========================================================================

static void m0_0E_PageInfosSlave(void)
{
  DateTime now = rtc.now();
  char buf[22];

  displayClear();
  displayText(0, 0, "=== SLAVE INFO ===");
  displayText(0, 1, VERSION);

  snprintf(buf, sizeof(buf), "Slave #%u  Boot#%lu",
           config.materiel.Num_Carte, (unsigned long)bootCount);
  displayText(0, 2, buf);

  snprintf(buf, sizeof(buf), "%02u:%02u %02u/%02u/%02u",
           now.hour(), now.minute(),
           now.day(), now.month(), (uint8_t)(now.year() % 100));
  displayText(0, 3, buf);

  snprintf(buf, sizeof(buf), "Poids: %.0fg",
           HiveSensor_Data.HX711Weight[0]);
  displayText(0, 4, buf);

  snprintf(buf, sizeof(buf), "Vbat: %.2fV",
           HiveSensor_Data.Bat_Voltage);
  displayText(0, 5, buf);

  snprintf(buf, sizeof(buf), "Rucher: %s", config.applicatif.RucherName);
  displayText(0, 6, buf);

  displayText(0, 7, "VALIDE -> retour");
  displayFlush();

  waitValideOrTimeout(TIMEOUT_SAISIE);
  backMenu();
}

static void m0_3S_CalibVBat(void)
{
  char buf[12];
  uint8_t c = (config.materiel.Num_Carte < 10) ? config.materiel.Num_Carte : 0;
  float cur = (config.materiel.VBatScale != 0.0f)
              ? config.materiel.VBatScale
              : VBatScale_List[c];
  snprintf(buf, sizeof(buf), "%.7f", cur);
  saisieActive = 'B';
  startNumInput("VBat Scale", buf, 9, false, true, 0, 100);
}

// ===========================================================================
// POINT D'ENTREE
// ===========================================================================

void initSlaveMenuSystem(void)
{
  pushMenu("MENU SLAVE", m0_Demarrage_S, M0S_SIZE, 0);
}

// Surcharge initMenuSystem() (weak dans menus_common)
void initMenuSystem(void)
{
  initSlaveMenuSystem();
}

// ===========================================================================
// DISPATCHER SLAVE (surcharge processExtraMenuSelection weak)
// ===========================================================================

bool processExtraMenuSelection(const char** menuList, uint8_t idx)
{
  if (menuList == m0_Demarrage_S)
  {
    switch (idx)
    {
      case 0: m0_0E_PageInfosSlave();                                          break;
      case 1: pushMenu("CONFIG. SYSTEME", m01_ConfigSystem, M01_SIZE, 0);     break;
      case 2: pushMenu("CALIB. BALANCE",  m04_CalibBalance,  M04_SIZE, 0);    break;
      case 3: m0_3S_CalibVBat();                                               break;
    }
    return true;
  }

  return false; // Menu non reconnu
}

// ===========================================================================
// COMPLETION SAISIES SLAVE (surcharge processExtraInputComplete weak)
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
  }

  return false;
}
