// ---------------------------------------------------------------------------*
// convert.cpp — Fonctions de conversion hexadecimale et decimale
// Projet POC_ATSAMD : Surveillance de Ruches
//
// Conversions hexa string <-> byte array, conversions decimales uint8_t <-> char,
// validation Spreading Factor LoRaWAN, fonctions d'affichage debug.
// ---------------------------------------------------------------------------*
#include <Arduino.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "convert.h"
#include "config.h"

// ---------------------------------------------------------------------------*
// @brief Convertit un caractère hexadécimal en valeur numérique (nibble)
// @param c Caractère hexadécimal ('0'-'9', 'A'-'F', 'a'-'f')
// @return uint8_t Valeur numérique (0-15), ou 0xFF si invalide
// ---------------------------------------------------------------------------*
uint8_t hexCharToNibble(char c)
{
  if (c >= '0' && c <= '9')
  {
    return (uint8_t)(c - '0');
  }
  else if (c >= 'A' && c <= 'F')
  {
    return (uint8_t)(c - 'A' + 10);
  }
  else if (c >= 'a' && c <= 'f')
  {
    return (uint8_t)(c - 'a' + 10);
  }
  else
  {
    return 0xFF; // Caractère invalide
  }
}

// ---------------------------------------------------------------------------*
// @brief Convertit une valeur numérique (nibble) en caractère hexadécimal
// @param nibble Valeur numérique (0-15)
// @return char Caractère hexadécimal ('0'-'9', 'A'-'F'), ou '?' si invalide
// ---------------------------------------------------------------------------*
char nibbleToHexChar(uint8_t nibble)
{
  if (nibble <= 9)
  {
    return (char)('0' + nibble);
  }
  else if (nibble <= 15)
  {
    return (char)('A' + nibble - 10);
  }
  else
  {
    return '?'; // Valeur invalide
  }
}

// ---------------------------------------------------------------------------*
// @brief Convertit une chaîne hexadécimale en tableau d'octets
// @param source Chaîne de caractères contenant les valeurs hexadécimales
// @param destination Tableau d'octets où stocker le résultat
// @param len Nombre d'octets à convertir (longueur du tableau destination)
// @return void
// ---------------------------------------------------------------------------*
void CONVERTfconvertByteArray(const char *source, uint8_t *destination, uint8_t len)
{
  for (uint8_t i = 0; i < len; i++)
  {
    // Extraire deux caractères hexadécimaux de la source
    char hexByte[3] = {source[i * 2], source[i * 2 + 1], '\0'};

    // Convertir en valeur numérique et stocker dans destination
    destination[i] = (uint8_t)strtol(hexByte, NULL, 16);
  }

   // Print the Hardware EUI
  Serial.print("CONVERTfconvertByteArray()/destination => DevEUI: ");
  for (uint8_t i = 0; i < len; i++)
  {
      Serial.print((char)NIBBLE_TO_HEX_CHAR(HIGH_NIBBLE(destination[i])));
      Serial.print((char)NIBBLE_TO_HEX_CHAR(LOW_NIBBLE(destination[i])));
  }
  Serial.println(" de CONVERTfconvertByteArray()");
}

// ---------------------------------------------------------------------------*
// @brief Convertit une chaîne hexadécimale en tableau d'octets
// @param hexString Chaîne hexadécimale source (ex: "5048494C...")
// @param byteArray Tableau destination pour les octets
// @param maxBytes Taille maximale du tableau destination
// @return bool True si conversion réussie, false sinon
// @note La chaîne doit avoir un nombre pair de caractères
// @note Le tableau sera terminé par 0x00 si possible
// ---------------------------------------------------------------------------*
bool hexStringToByteArray(const char* hexString, uint8_t* byteArray, uint8_t maxBytes)
{
  if (!hexString || !byteArray || maxBytes == 0)
  {
    Serial.println("Erreur: parametres invalides");
    return false;
  }

  uint8_t hexLength = strlen(hexString);

  // Vérifier que la longueur est paire
  if (hexLength % 2 != 0)
  {
    Serial.print("Erreur: longueur impaire (");
    Serial.print(hexLength);
    Serial.println(" caracteres)");
    return false;
  }

  // Calculer le nombre d'octets à générer
  uint8_t numBytes = hexLength / 2;

  // Vérifier que le tableau destination est assez grand
  // (+1 pour le terminateur 0x00)
  if (numBytes >= maxBytes)
  {
    Serial.print("Erreur: tableau trop petit (besoin de ");
    Serial.print(numBytes + 1);
    Serial.print(" octets, disponible: ");
    Serial.print(maxBytes);
    Serial.println(")");
    return false;
  }

  // Conversion
  for (uint8_t i = 0; i < numBytes; i++)
  {
    // Convertir les 2 caractères hexa en 1 octet
    uint8_t highNibble = hexCharToNibble(hexString[i * 2]);
    uint8_t lowNibble = hexCharToNibble(hexString[i * 2 + 1]);

    // Vérifier la validité
    if (highNibble == 0xFF || lowNibble == 0xFF)
    {
      Serial.print("Erreur: caractere hexa invalide a la position ");
      Serial.println(i * 2);
      return false;
    }

    // Combiner les nibbles en un octet
    byteArray[i] = (highNibble << 4) | lowNibble;
  }

  // Ajouter le terminateur 0x00
  byteArray[numBytes] = 0x00;

  Serial.print("Conversion reussie: ");
  Serial.print(hexLength);
  Serial.print(" caracteres hexa -> ");
  Serial.print(numBytes);
  Serial.println(" octets");

  return true;
}

// ---------------------------------------------------------------------------*
// @brief Convertit un tableau d'octets en chaîne hexadécimale
// @param byteArray Tableau source d'octets
// @param numBytes Nombre d'octets à convertir
// @param hexString Chaîne destination pour le résultat hexa
// @param maxChars Taille maximale de la chaîne destination
// @return bool True si conversion réussie, false sinon
// @note La chaîne sera terminée par '\0'
// @note La chaîne résultante aura (numBytes * 2) caractères + '\0'
// ---------------------------------------------------------------------------*
bool byteArrayToHexString(const uint8_t* byteArray, uint8_t numBytes, char* hexString, uint8_t maxChars)
{
  if (!byteArray || !hexString || maxChars == 0)
  {
    Serial.println("Erreur: parametres invalides");
    return false;
  }

  // Vérifier que la chaîne destination est assez grande
  // (numBytes * 2 caractères hexa + 1 pour '\0')
  uint8_t requiredChars = (numBytes * 2) + 1;
  if (requiredChars > maxChars)
  {
    Serial.print("Erreur: chaine trop petite (besoin de ");
    Serial.print(requiredChars);
    Serial.print(" caracteres, disponible: ");
    Serial.print(maxChars);
    Serial.println(")");
    return false;
  }

  // Conversion
  for (uint8_t i = 0; i < numBytes; i++)
  {
    uint8_t octet = byteArray[i];

    // Extraire les nibbles (4 bits hauts et 4 bits bas)
    uint8_t highNibble = (octet >> 4) & 0x0F;
    uint8_t lowNibble = octet & 0x0F;

    // Convertir en caractères hexa
    hexString[i * 2] = nibbleToHexChar(highNibble);
    hexString[i * 2 + 1] = nibbleToHexChar(lowNibble);
  }

  // Ajouter le terminateur '\0'
  hexString[numBytes * 2] = '\0';
/*
  Serial.print("Conversion reussie: ");
  Serial.print(numBytes);
  Serial.print(" octets -> ");
  Serial.print(numBytes * 2);
  Serial.println(" caracteres hexa");
  */
  return true;
}

// ===== CONVERSIONS DECIMALES (uint8_t <-> char buffer) =====

// ---------------------------------------------------------------------------*
// @brief Convertit un uint8_t en chaîne décimale
// @param value Valeur numérique à convertir (0-255)
// @param buffer Chaîne destination pour le résultat décimal
// @param maxChars Taille maximale de la chaîne destination (min 4)
// @return bool True si conversion réussie, false sinon
// @note La chaîne sera terminée par '\0'
// @note Exemples : 7 -> "7", 12 -> "12", 255 -> "255"
// ---------------------------------------------------------------------------*
bool uint8ToDecimalString(uint8_t value, char* buffer, uint8_t maxChars)
{
  if (!buffer || maxChars < 4)
  {
    Serial.println("Erreur: buffer trop petit (min 4 caracteres)");
    return false;
  }

  // Conversion avec snprintf
  snprintf(buffer, maxChars, "%u", value);

  Serial.print("Conversion uint8 -> string: ");
  Serial.print(value);
  Serial.print(" -> \"");
  Serial.print(buffer);
  Serial.println("\"");

  return true;
}

// ---------------------------------------------------------------------------*
// @brief Convertit une chaîne décimale en uint8_t
// @param buffer Chaîne décimale source (ex: "12", "255")
// @param value Pointeur vers la variable destination uint8_t
// @return bool True si conversion réussie, false sinon
// @note La chaîne doit contenir uniquement des chiffres 0-9
// @note La valeur doit être entre 0 et 255
// @note Les espaces en début/fin sont ignorés
// ---------------------------------------------------------------------------*
bool decimalStringToUint8(const char* buffer, uint8_t* value)
{
  if (!buffer || !value)
  {
    Serial.println("Erreur: parametres invalides");
    return false;
  }

  // Ignorer les espaces en début de chaîne
  while (*buffer == ' ')
  {
    buffer++;
  }

  // Vérifier que la chaîne n'est pas vide
  if (*buffer == '\0')
  {
    Serial.println("Erreur: chaine vide");
    return false;
  }

  // Vérifier que tous les caractères sont des chiffres
  const char* ptr = buffer;
  while (*ptr != '\0' && *ptr != ' ')
  {
    if (*ptr < '0' || *ptr > '9')
    {
      Serial.print("Erreur: caractere invalide '");
      Serial.print(*ptr);
      Serial.println("' (chiffres 0-9 uniquement)");
      return false;
    }
    ptr++;
  }

  // Conversion avec atol
  long temp = atol(buffer);

  // Vérifier la plage (0-255)
  if (temp < 0 || temp > 255)
  {
    Serial.print("Erreur: valeur hors plage (");
    Serial.print(temp);
    Serial.println("), plage valide: 0-255");
    return false;
  }

  *value = (uint8_t)temp;

  Serial.print("Conversion string -> uint8: \"");
  Serial.print(buffer);
  Serial.print("\" -> ");
  Serial.println(*value);

  return true;
}

// ===== VALIDATION SPREADING FACTOR LoRaWAN =====

// ---------------------------------------------------------------------------*
// @brief Vérifie si un Spreading Factor est valide pour LoRaWAN
// @param sf Valeur du Spreading Factor à vérifier
// @return bool True si SF est valide (7, 9 ou 12), false sinon
// @note LoRaWAN utilise uniquement SF7, SF9 et SF12
// ---------------------------------------------------------------------------*
bool isValidLoRaWanSF(uint8_t sf)
{
  return (sf == 7 || sf == 9 || sf == 12);
}

// ---------------------------------------------------------------------------*
// @brief Convertit et valide un Spreading Factor LoRaWAN depuis une chaîne
// @param sfString Chaîne contenant le SF (ex: "7", "9", "12")
// @param sfValue Pointeur vers la variable destination uint8_t
// @return bool True si conversion et validation réussies, false sinon
// @note Accepte uniquement les valeurs 7, 9 et 12
// ---------------------------------------------------------------------------*
bool validateLoRaWanSF(const char* sfString, uint8_t* sfValue)
{
  if (!sfString || !sfValue)
  {
    Serial.println("Erreur: parametres invalides");
    return false;
  }

  uint8_t tempSF = 0;

  // Convertir la chaîne en uint8_t
  if (!decimalStringToUint8(sfString, &tempSF))
  {
    Serial.println("Erreur: conversion SF impossible");
    return false;
  }

  // Vérifier que c'est un SF LoRaWAN valide
  if (!isValidLoRaWanSF(tempSF))
  {
    Serial.print("Erreur: SF");
    Serial.print(tempSF);
    Serial.println(" invalide. Valeurs autorisees: 7, 9, 12");
    return false;
  }

  *sfValue = tempSF;

  Serial.print("SF LoRaWAN valide: SF");
  Serial.println(*sfValue);

  return true;
}

// ---------------------------------------------------------------------------*
// @brief Convertit une chaîne hexadécimale en tableau d'octets
// @param source Chaîne de caractères contenant les valeurs hexadécimales
// @param destination Tableau d'octets où stocker le résultat
// @param len Nombre d'octets à convertir (longueur du tableau destination)
// @return void
// ---------------------------------------------------------------------------*
void convertByteArray(const char *source, uint8_t *destination, uint8_t len)
{
  for (uint8_t i = 0; i < len; i++)
  {
    // Extraire deux caractères hexadécimaux de la source
    char hexByte[3] = {source[i * 2], source[i * 2 + 1], '\0'};

    // Convertir en valeur numérique et stocker dans destination
    destination[i] = (uint8_t)strtol(hexByte, NULL, 16);
  }
}

// ---------------------------------------------------------------------------*
// @brief Convertit un tableau d'octets en chaîne hexadécimale
// @param source Tableau d'octets à convertir
// @param destination Chaîne de caractères où stocker le résultat
//        (doit contenir 2 * len + 1 caractères)
// @param len Nombre d'octets à convertir
// @return void
// ---------------------------------------------------------------------------*
void convertToHexString(const uint8_t *source, char *destination, uint8_t len)
{
  for (uint8_t i = 0; i < len; i++)
  {
    // Convertir chaque octet en deux caractères hexadécimaux
    snprintf(&destination[i * 2], 3, "%02X", source[i]);
  }
  // Terminer la chaîne avec un caractère nul
  destination[len * 2] = '\0';
}

// ---------------------------------------------------------------------------*
// @brief Affiche un tableau d'octets en hexadécimal sur le port série
// @param byteArray Tableau d'octets à afficher
// @param length Nombre d'octets à afficher
// @return void
// ---------------------------------------------------------------------------*
void printByteArray(const uint8_t* byteArray, uint8_t length)
{
  Serial.print("uint8_t array[");
  Serial.print(length);
  Serial.print("] = { ");

  for (uint8_t i = 0; i < length; i++)
  {
    Serial.print("0x");
    if (byteArray[i] < 0x10)
    {
      Serial.print("0");
    }
    Serial.print(byteArray[i], HEX);

    if (i < length - 1)
    {
      Serial.print(", ");
    }
  }

  Serial.println(" };");
}

// ---------------------------------------------------------------------------*
// @brief Affiche une chaîne hexadécimale sur le port série
// @param hexString Chaîne hexadécimale à afficher
// @return void
// ---------------------------------------------------------------------------*
void printHexString(const char* hexString)
{
  uint8_t len = strlen(hexString);

  Serial.print("char hexString[");
  Serial.print(len + 1);
  Serial.print("] = \"");
  Serial.print(hexString);
  Serial.println("\";");
}

// ===== FONCTION TestConvert() =====

/**
 * @brief Initialisation et test de conversion
 * @param void
 * @return void
 */
void TestConvert(void)
{
  Serial.println("========================================");
  Serial.println("Test conversion Hexa String -> Byte Array");
  Serial.println("========================================\n");

  // ===== TEST 1 : Conversion standard =====
  Serial.println("--- TEST 1 : Conversion standard ---");
  const char* hexString1 = "5048494C495050454C4F56454C4F56454C414F4F";
  uint8_t AppKey[17] = {0}; // 16 octets + 1 terminateur

  Serial.print("Chaine hexa source: ");
  Serial.println(hexString1);
  Serial.print("Longueur: ");
  Serial.print(strlen(hexString1));
  Serial.println(" caracteres\n");

  if (hexStringToByteArray(hexString1, AppKey, 17))
  {
    Serial.println("Resultat de la conversion:");
    printByteArray(AppKey, 16);
  }
  else
  {
    Serial.println("Echec de la conversion !");
  }

  // ===== TEST 2 : Clé AES-128 (32 caractères hexa = 16 octets) =====
  Serial.println("\n--- TEST 2 : Cle AES-128 ---");
  const char* hexString2 = "0123456789ABCDEF0123456789ABCDEF";
  uint8_t aesKey[17] = {0};

  Serial.print("Chaine hexa source: ");
  Serial.println(hexString2);

  if (hexStringToByteArray(hexString2, aesKey, 17))
  {
    Serial.println("Resultat de la conversion:");
    printByteArray(aesKey, 16);
  }

  // ===== TEST 3 : DevEUI LoRaWAN (16 caractères hexa = 8 octets) =====
  Serial.println("\n--- TEST 3 : DevEUI LoRaWAN ---");
  const char* hexString3 = "0004A30B001A2B3C";
  uint8_t devEUI[9] = {0};

  Serial.print("Chaine hexa source: ");
  Serial.println(hexString3);

  if (hexStringToByteArray(hexString3, devEUI, 9))
  {
    Serial.println("Resultat de la conversion:");
    printByteArray(devEUI, 8);
  }

  // ===== TEST 4 : Adresse MAC (12 caractères hexa = 6 octets) =====
  Serial.println("\n--- TEST 4 : Adresse MAC ---");
  const char* hexString4 = "AABBCCDDEEFF";
  uint8_t macAddr[7] = {0};

  Serial.print("Chaine hexa source: ");
  Serial.println(hexString4);

  if (hexStringToByteArray(hexString4, macAddr, 7))
  {
    Serial.println("Resultat de la conversion:");
    printByteArray(macAddr, 6);
  }

  // ===== TEST 5 : Valeur courte (4 caractères hexa = 2 octets) =====
  Serial.println("\n--- TEST 5 : Valeur courte ---");
  const char* hexString5 = "BEEF";
  uint8_t shortVal[3] = {0};

  Serial.print("Chaine hexa source: ");
  Serial.println(hexString5);

  if (hexStringToByteArray(hexString5, shortVal, 3))
  {
    Serial.println("Resultat de la conversion:");
    printByteArray(shortVal, 2);
  }

  // ===== TEST 6 : Test d'erreur - longueur impaire =====
  Serial.println("\n--- TEST 6 : Erreur longueur impaire ---");
  const char* hexString6 = "ABC"; // 3 caractères (impair)
  uint8_t errorTest1[2] = {0};

  Serial.print("Chaine hexa source: ");
  Serial.println(hexString6);

  if (!hexStringToByteArray(hexString6, errorTest1, 2))
  {
    Serial.println("Erreur detectee comme prevu !\n");
  }

  // ===== TEST 7 : Test d'erreur - caractère invalide =====
  Serial.println("--- TEST 7 : Erreur caractere invalide ---");
  const char* hexString7 = "12G4"; // 'G' n'est pas hexa
  uint8_t errorTest2[3] = {0};

  Serial.print("Chaine hexa source: ");
  Serial.println(hexString7);

  if (!hexStringToByteArray(hexString7, errorTest2, 3))
  {
    Serial.println("Erreur detectee comme prevu !\n");
  }

  // ===== TEST 8 : Utilisation pratique avec saisie hexa =====
  Serial.println("--- TEST 8 : Integration avec saisie hexa ---");
  Serial.println("Simulation d'une saisie utilisateur:");

  // Simuler une saisie utilisateur
  char userHexInput[41] = "0123456789ABCDEF0123456789ABCDEF"; // 32 caractères
  uint8_t userKey[17] = {0};

  Serial.print("Utilisateur a saisi: ");
  Serial.println(userHexInput);

  if (hexStringToByteArray(userHexInput, userKey, 17))
  {
    Serial.println("Cle prete a etre utilisee:");
    printByteArray(userKey, 16);

    Serial.println("\nUtilisation dans le code:");
    Serial.println("lorawan.setAppKey(userKey); // 16 octets sans le 0x00");
  }

  Serial.println("\n========================================");
  Serial.println("Tests termines !");
  Serial.println("========================================");

  // ===== TESTS DE CONVERSION INVERSE : Byte Array -> Hex String =====
  Serial.println("\n\n========================================");
  Serial.println("CONVERSION INVERSE: Byte Array -> Hex String");
  Serial.println("========================================\n");

  // ===== TEST 9 : Conversion inverse standard =====
  Serial.println("--- TEST 9 : Conversion inverse AppKey ---");
  uint8_t testAppKey[16] = {0x50, 0x48, 0x49, 0x4C, 0x49, 0x50, 0x50, 0x45,
                            0x4C, 0x4F, 0x56, 0x45, 0x4C, 0x41, 0x4F, 0x4F};
  char hexOutput1[41] = {0}; // 40 caractères + '\0'

  Serial.println("Tableau source:");
  printByteArray(testAppKey, 16);
  Serial.println();

  if (byteArrayToHexString(testAppKey, 16, hexOutput1, 41))
  {
    Serial.println("Resultat de la conversion:");
    printHexString(hexOutput1);
    Serial.print("Chaine obtenue: ");
    Serial.println(hexOutput1);
  }
  else
  {
    Serial.println("Echec de la conversion !");
  }

  // ===== TEST 10 : Conversion inverse clé AES-128 =====
  Serial.println("\n--- TEST 10 : Conversion inverse cle AES-128 ---");
  uint8_t testAesKey[16] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
                            0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
  char hexOutput2[41] = {0};

  Serial.println("Tableau source:");
  printByteArray(testAesKey, 16);
  Serial.println();

  if (byteArrayToHexString(testAesKey, 16, hexOutput2, 41))
  {
    Serial.println("Resultat de la conversion:");
    printHexString(hexOutput2);
  }

  // ===== TEST 11 : Conversion inverse DevEUI =====
  Serial.println("\n--- TEST 11 : Conversion inverse DevEUI ---");
  uint8_t testDevEUI[8] = {0x00, 0x04, 0xA3, 0x0B, 0x00, 0x1A, 0x2B, 0x3C};
  char hexOutput3[17] = {0}; // 16 caractères + '\0'

  Serial.println("Tableau source:");
  printByteArray(testDevEUI, 8);
  Serial.println();

  if (byteArrayToHexString(testDevEUI, 8, hexOutput3, 17))
  {
    Serial.println("Resultat de la conversion:");
    printHexString(hexOutput3);
  }

  // ===== TEST 12 : Conversion inverse adresse MAC =====
  Serial.println("\n--- TEST 12 : Conversion inverse MAC address ---");
  uint8_t testMac[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
  char hexOutput4[13] = {0}; // 12 caractères + '\0'

  Serial.println("Tableau source:");
  printByteArray(testMac, 6);
  Serial.println();

  if (byteArrayToHexString(testMac, 6, hexOutput4, 13))
  {
    Serial.println("Resultat de la conversion:");
    printHexString(hexOutput4);
  }

  // ===== TEST 13 : Conversion inverse valeur courte =====
  Serial.println("\n--- TEST 13 : Conversion inverse valeur courte ---");
  uint8_t testShort[2] = {0xBE, 0xEF};
  char hexOutput5[5] = {0}; // 4 caractères + '\0'

  Serial.println("Tableau source:");
  printByteArray(testShort, 2);
  Serial.println();

  if (byteArrayToHexString(testShort, 2, hexOutput5, 5))
  {
    Serial.println("Resultat de la conversion:");
    printHexString(hexOutput5);
  }

  // ===== TEST 14 : Test d'erreur - buffer trop petit =====
  Serial.println("\n--- TEST 14 : Erreur buffer trop petit ---");
  uint8_t testBigArray[16] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                              0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  char tooSmallBuffer[10] = {0}; // Trop petit pour 16 octets (besoin de 33)

  Serial.println("Tentative de conversion de 16 octets dans un buffer de 10 caracteres:");

  if (!byteArrayToHexString(testBigArray, 16, tooSmallBuffer, 10))
  {
    Serial.println("Erreur detectee comme prevu !\n");
  }

  // ===== TEST 15 : Conversion aller-retour (round-trip) =====
  Serial.println("--- TEST 15 : Conversion aller-retour ---");
  Serial.println("Test de coherence: Hexa -> Bytes -> Hexa");

  const char* originalHex = "DEADBEEFCAFEBABE";
  uint8_t intermediateBytes[9] = {0}; // 8 octets + '\0'
  char finalHex[17] = {0};

  Serial.print("Chaine hexa originale: ");
  Serial.println(originalHex);

  // Conversion 1 : Hex -> Bytes
  if (hexStringToByteArray(originalHex, intermediateBytes, 9))
  {
    Serial.println("\nApres conversion vers bytes:");
    printByteArray(intermediateBytes, 8);

    // Conversion 2 : Bytes -> Hex
    if (byteArrayToHexString(intermediateBytes, 8, finalHex, 17))
    {
      Serial.println("\nApres conversion retour vers hexa:");
      printHexString(finalHex);

      // Vérification
      if (strcmp(originalHex, finalHex) == 0)
      {
        Serial.println("\n*** TEST REUSSI: Les chaines sont identiques ! ***");
      }
      else
      {
        Serial.println("\n*** ERREUR: Les chaines different ! ***");
        Serial.print("Originale: ");
        Serial.println(originalHex);
        Serial.print("Finale:    ");
        Serial.println(finalHex);
      }
    }
  }

  Serial.println("\n========================================");
  Serial.println("Tous les tests termines !");
  Serial.println("========================================");

  // ===== TESTS DE CONVERSION DECIMALE : uint8_t <-> char buffer =====
  Serial.println("\n\n========================================");
  Serial.println("CONVERSIONS DECIMALES: uint8_t <-> char buffer");
  Serial.println("========================================\n");

  // ===== TEST 16 : uint8_t -> string (cas typiques) =====
  Serial.println("--- TEST 16 : uint8_t -> string (cas typiques) ---");
  char decBuffer[10] = {0};

  // Spreading Factor 7
  uint8_t sf7 = 7;
  if (uint8ToDecimalString(sf7, decBuffer, 10))
  {
    Serial.print("Resultat: SF");
    Serial.print(sf7);
    Serial.print(" = \"");
    Serial.print(decBuffer);
    Serial.println("\"");
  }

  // Spreading Factor 12
  uint8_t sf12 = 12;
  if (uint8ToDecimalString(sf12, decBuffer, 10))
  {
    Serial.print("Resultat: SF");
    Serial.print(sf12);
    Serial.print(" = \"");
    Serial.print(decBuffer);
    Serial.println("\"");
  }

  // Valeur maximale
  uint8_t maxVal = 255;
  if (uint8ToDecimalString(maxVal, decBuffer, 10))
  {
    Serial.print("Resultat: ");
    Serial.print(maxVal);
    Serial.print(" = \"");
    Serial.print(decBuffer);
    Serial.println("\"");
  }

  // Valeur minimale
  uint8_t minVal = 0;
  if (uint8ToDecimalString(minVal, decBuffer, 10))
  {
    Serial.print("Resultat: ");
    Serial.print(minVal);
    Serial.print(" = \"");
    Serial.print(decBuffer);
    Serial.println("\"");
  }

  // ===== TEST 17 : string -> uint8_t (cas valides) =====
  Serial.println("\n--- TEST 17 : string -> uint8_t (cas valides) ---");
  uint8_t resultValue = 0;

  // "7" -> 7
  if (decimalStringToUint8("7", &resultValue))
  {
    Serial.print("Spreading Factor = ");
    Serial.println(resultValue);
  }

  // "12" -> 12
  if (decimalStringToUint8("12", &resultValue))
  {
    Serial.print("Spreading Factor = ");
    Serial.println(resultValue);
  }

  // "255" -> 255
  if (decimalStringToUint8("255", &resultValue))
  {
    Serial.print("Valeur maximale = ");
    Serial.println(resultValue);
  }

  // "0" -> 0
  if (decimalStringToUint8("0", &resultValue))
  {
    Serial.print("Valeur minimale = ");
    Serial.println(resultValue);
  }

  // Avec espaces " 42 "
  if (decimalStringToUint8(" 42 ", &resultValue))
  {
    Serial.print("Avec espaces = ");
    Serial.println(resultValue);
  }

  // ===== TEST 18 : Erreurs de conversion string -> uint8_t =====
  Serial.println("\n--- TEST 18 : Erreurs de conversion ---");

  // Valeur trop grande
  Serial.println("Test: \"256\" (hors plage)");
  if (!decimalStringToUint8("256", &resultValue))
  {
    Serial.println("Erreur detectee comme prevu !\n");
  }

  // Valeur négative
  Serial.println("Test: \"-5\" (negatif)");
  if (!decimalStringToUint8("-5", &resultValue))
  {
    Serial.println("Erreur detectee comme prevu !\n");
  }

  // Caractère invalide
  Serial.println("Test: \"12A\" (caractere invalide)");
  if (!decimalStringToUint8("12A", &resultValue))
  {
    Serial.println("Erreur detectee comme prevu !\n");
  }

  // Chaîne vide
  Serial.println("Test: \"\" (chaine vide)");
  if (!decimalStringToUint8("", &resultValue))
  {
    Serial.println("Erreur detectee comme prevu !\n");
  }

  // ===== TEST 19 : Conversion aller-retour décimale =====
  Serial.println("--- TEST 19 : Conversion aller-retour decimale ---");
  Serial.println("Test de coherence: uint8 -> string -> uint8");

  uint8_t originalSF = 12;
  char sfBuffer[10] = {0};
  uint8_t finalSF = 0;

  Serial.print("Valeur originale: ");
  Serial.println(originalSF);

  // Conversion 1 : uint8 -> string
  if (uint8ToDecimalString(originalSF, sfBuffer, 10))
  {
    Serial.print("Apres conversion en string: \"");
    Serial.print(sfBuffer);
    Serial.println("\"");

    // Conversion 2 : string -> uint8
    if (decimalStringToUint8(sfBuffer, &finalSF))
    {
      Serial.print("Apres conversion retour: ");
      Serial.println(finalSF);

      // Vérification
      if (originalSF == finalSF)
      {
        Serial.println("\n*** TEST REUSSI: Les valeurs sont identiques ! ***");
      }
      else
      {
        Serial.println("\n*** ERREUR: Les valeurs different ! ***");
      }
    }
  }

  // ===== TEST 20 : Cas d'usage LoRaWAN Spreading Factor =====
  Serial.println("\n--- TEST 20 : Usage pratique LoRaWAN SF ---");
  Serial.println("Configuration Spreading Factor:\n");

  // Test des 3 valeurs valides
  const char* validSFs[] = {"7", "9", "12"};

  for (uint8_t i = 0; i < 3; i++)
  {
    uint8_t spreadingFactor = 0;
    Serial.print("Test SF: \"");
    Serial.print(validSFs[i]);
    Serial.println("\"");

    if (validateLoRaWanSF(validSFs[i], &spreadingFactor))
    {
      Serial.println("  => lorawan.setSpreadingFactor(spreadingFactor);\n");
    }
  }

  // Test des valeurs invalides
  Serial.println("Tests de valeurs invalides:");
  const char* invalidSFs[] = {"6", "8", "10", "11", "13"};

  for (uint8_t i = 0; i < 5; i++)
  {
    uint8_t spreadingFactor = 0;
    Serial.print("Test SF: \"");
    Serial.print(invalidSFs[i]);
    Serial.println("\"");

    if (!validateLoRaWanSF(invalidSFs[i], &spreadingFactor))
    {
      Serial.println("  => Rejete comme prevu\n");
    }
  }

  // ===== TEST 21 : Autres paramètres LoRaWAN =====
  Serial.println("\n--- TEST 21 : Autres parametres LoRaWAN ---");

  // Bandwidth
  uint8_t bandwidth = 125;
  char bwBuffer[10] = {0};
  uint8ToDecimalString(bandwidth, bwBuffer, 10);
  Serial.print("Bandwidth: ");
  Serial.print(bwBuffer);
  Serial.println(" kHz");

  // Coding Rate
  uint8_t codingRate = 5;
  char crBuffer[10] = {0};
  uint8ToDecimalString(codingRate, crBuffer, 10);
  Serial.print("Coding Rate: 4/");
  Serial.println(crBuffer);

  // TX Power
  uint8_t txPower = 14;
  char pwrBuffer[10] = {0};
  uint8ToDecimalString(txPower, pwrBuffer, 10);
  Serial.print("TX Power: ");
  Serial.print(pwrBuffer);
  Serial.println(" dBm");

  Serial.println("\n========================================");
  Serial.println("Tous les tests de conversion termines !");
  Serial.println("========================================");
}
