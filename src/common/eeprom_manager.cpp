// eeprom_manager.cpp — Gestion configuration EEPROM AT24C32
// Porte depuis 24C32.cpp (POC_ATSAMD) — 95% portable
// Modifications : debugSerial -> Serial, include adaptes

#include <Arduino.h>
#include <Wire.h>
#include <stddef.h>
#include "eeprom_manager.h"
#include "types.h"
#include "config.h"

// ===== VARIABLES EXTERNES =====
// config est definie dans le main du module concerne
extern ConfigGenerale_t config;

// Forward declarations — fonctions definies dans d'autres modules
extern void SETUPSetStructDefaultValues(void) __attribute__((weak));

// ---------------------------------------------------------------------------
// ===== FONCTIONS DE GESTION EEPROM BAS NIVEAU =====
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// @brief Ecriture d'un octet en EEPROM AT24C32
// @param address Adresse memoire (0x0000 a 0x0FFF pour 4Ko)
// @param data Octet a ecrire
// @return void
// ---------------------------------------------------------------------------
void EPR_24C32writeByte(uint16_t address, uint8_t data)
{
  Wire.beginTransmission(EEPROM_ADDRESS);
  Wire.write((uint8_t)(address >> 8));    // MSB de l'adresse
  Wire.write((uint8_t)(address & 0xFF));  // LSB de l'adresse
  Wire.write(data);
  Wire.endTransmission();
  delay(5);  // Delai d'ecriture EEPROM (tW = 5ms max pour AT24C32)
}

// ---------------------------------------------------------------------------
// @brief Lecture d'un octet depuis l'EEPROM AT24C32
// @param address Adresse memoire (0x0000 a 0x0FFF pour 4Ko)
// @return Octet lu depuis l'EEPROM
// ---------------------------------------------------------------------------
uint8_t EPR_24C32readByte(uint16_t address)
{
  Wire.beginTransmission(EEPROM_ADDRESS);
  Wire.write((uint8_t)(address >> 8));    // MSB de l'adresse
  Wire.write((uint8_t)(address & 0xFF));  // LSB de l'adresse
  Wire.endTransmission();

  Wire.requestFrom(EEPROM_ADDRESS, 1);
  if (Wire.available())
  {
    return Wire.read();
  }
  return 0xFF;  // Valeur par defaut si erreur
}

// ---------------------------------------------------------------------------
// @brief Ecriture d'un bloc de donnees en EEPROM AT24C32
// @param address Adresse de depart
// @param data Pointeur vers les donnees a ecrire
// @param length Nombre d'octets a ecrire
// @return void
// ---------------------------------------------------------------------------
void EPR_24C32writeBlock(uint16_t address, uint8_t* data, uint16_t length)
{
  for (uint16_t i = 0; i < length; i++)
  {
    EPR_24C32writeByte(address + i, data[i]);
  }
}

// ---------------------------------------------------------------------------
// @brief Lecture d'un bloc de donnees depuis l'EEPROM AT24C32
// @param address Adresse de depart
// @param data Pointeur vers le buffer de reception
// @param length Nombre d'octets a lire
// @return void
// ---------------------------------------------------------------------------
void EPR_24C32readBlock(uint16_t address, uint8_t* data, uint16_t length)
{
  for (uint16_t i = 0; i < length; i++)
  {
    data[i] = EPR_24C32readByte(address + i);
  }
}

// ---------------------------------------------------------------------------
// ===== FONCTIONS DE CALCUL ET VALIDATION =====
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// @brief Calcule le checksum de la configuration (CRC16)
// @param cfg Pointeur vers la structure de configuration
// @return Valeur du checksum calcule (uint16_t)
// ---------------------------------------------------------------------------
uint16_t EPR_24C32calcChecksum(ConfigGenerale_t* cfg)
{
  uint16_t crc = 0xFFFF;
  uint8_t* data = (uint8_t*)cfg;

  // Calcul sur toute la structure sauf le champ checksum lui-meme
  size_t checksumOffset = offsetof(ConfigGenerale_t, checksum);
  for (uint16_t i = 0; i < checksumOffset; i++)
  {
    crc ^= data[i];
    for (uint8_t j = 0; j < 8; j++)
    {
      if (crc & 0x0001)
      {
        crc = (crc >> 1) ^ 0xA001;  // Polynome CRC16
      }
      else
      {
        crc = crc >> 1;
      }
    }
  }
  return crc;
}

// ---------------------------------------------------------------------------
// ===== FONCTIONS DE GESTION CONFIGURATION =====
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// @brief Charge la configuration depuis l'EEPROM avec validation
// @param void
// @return void
// ---------------------------------------------------------------------------
void E24C32loadConfig(void)
{
  // Lecture de la configuration
  E24C32readConfig();

  // Verification du nombre magique
  if (config.magicNumber != CONFIG_MAGIC_NUMBER)
  {
    Serial.print(F("ERREUR: Magic number invalide! Attendu: 0x"));
    Serial.print(CONFIG_MAGIC_NUMBER, HEX);
    Serial.print(F(" / Lu: 0x"));
    Serial.print(config.magicNumber, HEX);
    Serial.println(F(" => Chargement config par defaut..."));
    if (SETUPSetStructDefaultValues)
    {
      SETUPSetStructDefaultValues();
    }
    else
    {
      LOG_WARNING("SETUPSetStructDefaultValues() non definie");
    }
    E24C32saveConfig();
    return;
  }

  // Calcul et verification du checksum
  uint16_t calculatedChecksum = EPR_24C32calcChecksum(&config);
  if (calculatedChecksum != config.checksum)
  {
    Serial.print(F("ERREUR: Checksum invalide! Attendu: 0x"));
    Serial.print(config.checksum, HEX);
    Serial.print(F(" / Calcule: 0x"));
    Serial.print(calculatedChecksum, HEX);
    Serial.println(F(" => Chargement config par defaut..."));
    if (SETUPSetStructDefaultValues)
    {
      SETUPSetStructDefaultValues();
    }
    E24C32saveConfig();
    return;
  }

  E24C32DumpConfigToJSON();
}

// ---------------------------------------------------------------------------
// @brief Lit la configuration brute depuis l'EEPROM
// @param void
// @return void
// ---------------------------------------------------------------------------
void E24C32readConfig(void)
{
  LOG_INFO(F("Lecture EEPROM..."));
  EPR_24C32readBlock(CONFIG_EEPROM_START, (uint8_t*)&config, sizeof(ConfigGenerale_t));
}

// ---------------------------------------------------------------------------
// @brief Sauvegarde la configuration en EEPROM avec calcul du checksum
// @param void
// @return void
// ---------------------------------------------------------------------------
void E24C32saveConfig(void)
{
  LOG_INFO(F("Sauvegarde config en EEPROM..."));

  // S'assurer que le nombre magique est present
  config.magicNumber = CONFIG_MAGIC_NUMBER;

  // Recalculer le checksum avant sauvegarde
  config.checksum = EPR_24C32calcChecksum(&config);

  Serial.print(F("Checksum calcule: 0x"));
  Serial.println(config.checksum, HEX);

  // Ecriture en EEPROM
  EPR_24C32writeBlock(CONFIG_EEPROM_START, (uint8_t*)&config, sizeof(ConfigGenerale_t));

  LOG_INFO(F("Config sauvegardee avec succes"));
}

// ---------------------------------------------------------------------------
// ===== FONCTIONS D'AFFICHAGE ET DEBUG =====
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// @brief Affiche un tableau d'octets au format JSON hexadecimal
// @param array Pointeur vers le tableau d'octets
// @param length Nombre d'octets a afficher
// @return void
// ---------------------------------------------------------------------------
void E24C32printJSON(uint8_t* array, uint8_t length)
{
  Serial.print("\"");
  for (uint8_t i = 0; i < length; i++)
  {
    if (array[i] < 0x10)
    {
      Serial.print("0");
    }
    Serial.print(array[i], HEX);
  }
  Serial.print("\"");
}

// ---------------------------------------------------------------------------
// @brief Dump complet de la configuration au format JSON sur Serial
// @param void
// @return void
// ---------------------------------------------------------------------------
void E24C32DumpConfigToJSON(void)
{
  Serial.println(F("{"));
  Serial.println(F("  \"configuration\": {"));

  // En-tete de configuration
  Serial.println(F("    \"header\": {"));
  Serial.print(F("      \"magicNumber\": \"0x"));
  Serial.print(config.magicNumber, HEX);
  Serial.println(F("\","));
  Serial.print(F("      \"checksum\": \"0x"));
  Serial.print(config.checksum, HEX);
  Serial.println(F("\","));
  Serial.print(F("      \"checksumCalcule\": \"0x"));
  Serial.print(EPR_24C32calcChecksum(&config), HEX);
  Serial.println(F("\","));
  Serial.print(F("      \"checksumValide\": "));
  Serial.print((EPR_24C32calcChecksum(&config) == config.checksum) ? "true" : "false");
  Serial.println(F(","));
  Serial.print(F("      \"tailleStructure\": "));
  Serial.println(sizeof(ConfigGenerale_t));
  Serial.println(F("    },"));

  // Configuration Applicatif
  Serial.println(F("    \"applicatif\": {"));
  Serial.print(F("      \"version\": "));
  Serial.print(config.applicatif.version);
  Serial.println(F(","));

  // LED
  Serial.println(F("      \"led\": {"));
  Serial.print(F("        \"redDuration\": "));
  Serial.print(config.applicatif.redLedDuration);
  Serial.println(F(","));
  Serial.print(F("        \"greenDuration\": "));
  Serial.print(config.applicatif.greenLedDuration);
  Serial.println(F(","));
  Serial.print(F("        \"blueDuration\": "));
  Serial.print(config.applicatif.blueLedDuration);
  Serial.println(F(","));
  Serial.print(F("        \"builtinDuration\": "));
  Serial.println(config.applicatif.builtinLedDuration);
  Serial.println(F("      },"));

  // Rucher
  Serial.println(F("      \"rucher\": {"));
  Serial.print(F("        \"id\": "));
  Serial.print(config.applicatif.RucherID);
  Serial.println(F(","));
  Serial.print(F("        \"nom\": \""));
  Serial.print(config.applicatif.RucherName);
  Serial.println(F("\""));
  Serial.println(F("      },"));

  // LoRa
  Serial.println(F("      \"lora\": {"));
  Serial.print(F("        \"appEUI\": "));
  E24C32printJSON(config.applicatif.AppEUI, 8);
  Serial.println(F(","));
  Serial.print(F("        \"appKey\": "));
  E24C32printJSON(config.applicatif.AppKey, 16);
  Serial.println(F(","));
  Serial.print(F("        \"spreadingFactor\": "));
  Serial.print(config.applicatif.SpreadingFactor);
  Serial.println(F(","));
  Serial.print(F("        \"sendingPeriod\": "));
  Serial.print(config.applicatif.SendingPeriod);
  Serial.println(F(","));
  Serial.print(F("        \"oledRefreshPeriod\": "));
  Serial.println(config.applicatif.OLEDRefreshPeriod);
  Serial.println(F("      }"));
  Serial.println(F("    },"));

  // Configuration Materiel
  Serial.println(F("    \"materiel\": {"));
  Serial.print(F("      \"version\": "));
  Serial.print(config.materiel.version);
  Serial.println(F(","));

  // Adresses I2C
  Serial.println(F("      \"adressesI2C\": {"));
  Serial.print(F("        \"rtc\": \"0x"));
  Serial.print(config.materiel.adresseRTC, HEX);
  Serial.println(F("\","));
  Serial.print(F("        \"oled\": \"0x"));
  Serial.print(config.materiel.adresseOLED, HEX);
  Serial.println(F("\","));
  Serial.print(F("        \"eeprom\": \"0x"));
  Serial.print(config.materiel.adresseEEPROM, HEX);
  Serial.println(F("\""));
  Serial.println(F("      },"));

  // Identification
  Serial.println(F("      \"identification\": {"));
  Serial.print(F("        \"numCarte\": "));
  Serial.print(config.materiel.Num_Carte);
  Serial.println(F(","));
  Serial.print(F("        \"devEUI\": "));
  E24C32printJSON(config.materiel.DevEUI, 8);
  Serial.println(F(","));
  Serial.print(F("        \"poidsTare\": "));
  Serial.println(config.materiel.poidsTare);
  Serial.println(F("      },"));

  // Peripheriques presents
  Serial.println(F("      \"peripheriques\": {"));
  Serial.print(F("        \"rtc\": "));
  Serial.print(config.materiel.Rtc ? "true" : "false");
  Serial.println(F(","));
  Serial.print(F("        \"kbdAnalogique\": "));
  Serial.print(config.materiel.KBD_Ana ? "true" : "false");
  Serial.println(F(","));
  Serial.print(F("        \"oled\": "));
  Serial.print(config.materiel.Oled ? "true" : "false");
  Serial.println(F(","));
  Serial.print(F("        \"sdhc\": "));
  Serial.print(config.materiel.SDHC ? "true" : "false");
  Serial.println(F(","));
  Serial.print(F("        \"lipo\": "));
  Serial.print(config.materiel.LiPo ? "true" : "false");
  Serial.println(F(","));
  Serial.print(F("        \"solaire\": "));
  Serial.println(config.materiel.Solaire ? "true" : "false");
  Serial.println(F("      },"));

  // Facteurs d'echelle analogiques
  Serial.println(F("      \"analogScale\": {"));
  Serial.print(F("        \"ldrBrightness\": "));
  Serial.print(config.materiel.LDRBrightnessScale, 6);
  Serial.println(F(","));
  Serial.print(F("        \"vSolaire\": "));
  Serial.print(config.materiel.VSolScale, 6);
  Serial.println(F(","));
  Serial.print(F("        \"vBatterie\": "));
  Serial.println(config.materiel.VBatScale, 6);
  Serial.println(F("      },"));

  // Pesons HX711
  Serial.println(F("      \"pesons\": ["));
  uint8_t pesonNums[4]  = {config.materiel.Peson_0,            config.materiel.Peson_1,            config.materiel.Peson_2,            config.materiel.Peson_3};
  uint8_t clkPins[4]   = {config.materiel.HX711Clk_0,         config.materiel.HX711Clk_1,         config.materiel.HX711Clk_2,         config.materiel.HX711Clk_3};
  uint8_t dtaPins[4]   = {config.materiel.HX711Dta_0,         config.materiel.HX711Dta_1,         config.materiel.HX711Dta_2,         config.materiel.HX711Dta_3};
  float noloads[4]     = {config.materiel.HX711NoloadValue_0, config.materiel.HX711NoloadValue_1, config.materiel.HX711NoloadValue_2, config.materiel.HX711NoloadValue_3};
  float tareTemps[4]   = {config.materiel.HX711Tare_Temp_0,   config.materiel.HX711Tare_Temp_1,   config.materiel.HX711Tare_Temp_2,   config.materiel.HX711Tare_Temp_3};
  float scalings[4]    = {config.materiel.HX711Scaling_0,     config.materiel.HX711Scaling_1,     config.materiel.HX711Scaling_2,     config.materiel.HX711Scaling_3};
  float corTemps[4]    = {config.materiel.HX711Cor_Temp_0,    config.materiel.HX711Cor_Temp_1,    config.materiel.HX711Cor_Temp_2,    config.materiel.HX711Cor_Temp_3};

  for (uint8_t p = 0; p < 4; p++)
  {
    Serial.println(F("        {"));
    Serial.print(F("          \"numero\": "));       Serial.print(pesonNums[p]);    Serial.println(F(","));
    Serial.print(F("          \"pinClk\": "));       Serial.print(clkPins[p]);      Serial.println(F(","));
    Serial.print(F("          \"pinData\": "));      Serial.print(dtaPins[p]);      Serial.println(F(","));
    Serial.print(F("          \"noloadValue\": "));  Serial.print(noloads[p], 2);   Serial.println(F(","));
    Serial.print(F("          \"tareTemp\": "));     Serial.print(tareTemps[p], 2); Serial.println(F(","));
    Serial.print(F("          \"scaling\": "));      Serial.print(scalings[p], 4);  Serial.println(F(","));
    Serial.print(F("          \"corTemp\": "));      Serial.println(corTemps[p], 4);
    Serial.print(F("        }"));
    Serial.println((p < 3) ? "," : "");
  }

  Serial.println(F("      ]"));
  Serial.println(F("    }"));
  Serial.println(F("  }"));
  Serial.println(F("}"));
}

// ---------------------------------------------------------------------------
// @brief Initialise la configuration (lecture + validation)
// @param void
// @return void
// ---------------------------------------------------------------------------
void E24C32initConfig(void)
{
  LOG_INFO("Debut lecture configuration EEPROM");
  E24C32loadConfig();
  LOG_INFO("Lecture configuration EEPROM executee");
}
