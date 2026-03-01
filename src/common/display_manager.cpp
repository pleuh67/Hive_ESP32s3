// display_manager.cpp — Affichage OLED SSD1309 via U8g2
// Phase 1 — common master + slave
// Reecriture complete (l'original ATSAMD utilisait Adafruit SH1106/SSD1306)
//
// Convention d'affichage :
//   128 x 64 pixels, police 6x8 px
//   8 lignes (row 0..7), 21 colonnes max (col 0..20)
//   Coordonnee Y = baseline du texte = (row+1)*8 - 1

#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include "display_manager.h"
#include "config.h"

// ---------------------------------------------------------------------------
// Objet U8g2 — SSD1309 128x64, I2C materiel, full buffer (F)
// U8G2_R0 = pas de rotation
// reset = U8X8_PIN_NONE (pas de broche reset dediee)
// ---------------------------------------------------------------------------
U8G2_SSD1309_128X64_NONAME2_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// Hauteur d'une ligne en pixels (police 6x8 -> 8 px par ligne)
static const uint8_t LINE_HEIGHT = 8;

// Largeur d'un caractere en pixels
static const uint8_t CHAR_WIDTH  = 6;

// ---------------------------------------------------------------------------
// @brief Convertit une ligne logique (0..7) en coordonnee Y baseline U8g2
// @param row Ligne logique (0 = haut, 7 = bas)
// @return uint8_t Coordonnee Y (baseline)
// ---------------------------------------------------------------------------
static uint8_t rowToY(uint8_t row)
{
  return (uint8_t)((row + 1) * LINE_HEIGHT - 1);
}

// ---------------------------------------------------------------------------
// @brief Initialise l'ecran OLED SSD1309 via U8g2
// @param void
// @return void
// ---------------------------------------------------------------------------
void displayInit(void)
{
  u8g2.begin();
  u8g2.setFont(u8g2_font_5x8_tf);
  u8g2.clearBuffer();
  u8g2.sendBuffer();
  LOG_INFO("OLED SSD1309 U8g2 OK");
}

// ---------------------------------------------------------------------------
// @brief Efface le buffer interne (ne modifie pas l'ecran)
// @param void
// @return void
// ---------------------------------------------------------------------------
void displayClear(void)
{
  u8g2.clearBuffer();
}

// ---------------------------------------------------------------------------
// @brief Envoie le buffer vers l'ecran
// @param void
// @return void
// ---------------------------------------------------------------------------
void displayFlush(void)
{
  u8g2.sendBuffer();
}

// ---------------------------------------------------------------------------
// @brief Ecrit du texte a une position donnee (normal)
// @param col Colonne en caracteres (0..20)
// @param row Ligne logique (0..7)
// @param text Texte a afficher (char[])
// @return void
// ---------------------------------------------------------------------------
void displayText(uint8_t col, uint8_t row, const char* text)
{
  if (!text) return;
  u8g2.setDrawColor(1);
  u8g2.setFontMode(0);
  u8g2.drawStr((int8_t)(col * CHAR_WIDTH), (int8_t)rowToY(row), text);
}

// ---------------------------------------------------------------------------
// @brief Ecrit du texte en inverse video (fond blanc, texte noir)
// @param col Colonne en caracteres (0..20)
// @param row Ligne logique (0..7)
// @param text Texte a afficher
// @return void
// ---------------------------------------------------------------------------
void displayTextInverse(uint8_t col, uint8_t row, const char* text)
{
  if (!text) return;
  uint8_t x = (uint8_t)(col * CHAR_WIDTH);
  uint8_t y = rowToY(row);
  uint8_t w = (uint8_t)(strlen(text) * CHAR_WIDTH);

  // Rectangle de fond blanc
  u8g2.setDrawColor(1);
  u8g2.drawBox(x, (uint8_t)(y - LINE_HEIGHT + 1), w, LINE_HEIGHT);

  // Texte en noir sur fond blanc
  u8g2.setDrawColor(0);
  u8g2.setFontMode(1);
  u8g2.drawStr((int8_t)x, (int8_t)y, text);

  // Retour mode normal
  u8g2.setDrawColor(1);
  u8g2.setFontMode(0);
}

// ---------------------------------------------------------------------------
// @brief Efface une ligne entiere du buffer
// @param row Ligne logique (0..7)
// @return void
// ---------------------------------------------------------------------------
void displayClearRow(uint8_t row)
{
  uint8_t y = (uint8_t)(row * LINE_HEIGHT);
  u8g2.setDrawColor(0);
  u8g2.drawBox(0, y, SCREEN_WIDTH, LINE_HEIGHT);
  u8g2.setDrawColor(1);
}

// ---------------------------------------------------------------------------
// @brief Affiche un message sur la ligne 7 (debug), flush immediat
// @param message Texte a afficher (tronque a 21 chars)
// @return void
// ---------------------------------------------------------------------------
void OLEDDebugDisplay(const char* message)
{
  if (!message) return;
  char buf[22];
  snprintf(buf, sizeof(buf), "%-21s", message);

  displayClearRow(7);
  displayText(0, 7, buf);
  displayFlush();

  Serial.print("[OLED] ");
  Serial.println(message);
}

// ---------------------------------------------------------------------------
// @brief Affiche un message sur la ligne 7 (status bar)
// @param message Texte (tronque a 21 chars)
// @param defilant Non implemente en Phase 1
// @param inverse  True = inverse video
// @return void
// ---------------------------------------------------------------------------
void OLEDDisplayMessageL8(const char* message, bool defilant, bool inverse)
{
  if (!message) return;
  char buf[22];
  snprintf(buf, sizeof(buf), "%-21s", message);

  displayClearRow(7);
  if (inverse)
  {
    displayTextInverse(0, 7, buf);
  }
  else
  {
    displayText(0, 7, buf);
  }
  displayFlush();
}

// ---------------------------------------------------------------------------
// @brief API compatible saisies_nb.cpp : ecrit du texte
// @param page Ignore en Phase 1 (prevu pour multi-page)
// @param line Ligne (0..7)
// @param col  Colonne (0..20)
// @param text Texte
// @return void
// ---------------------------------------------------------------------------
void OLEDDrawText(uint8_t page, uint8_t line, uint8_t col, const char* text)
{
  (void)page; // non utilise en Phase 1
  if (!text || line >= MAX_LIGNES) return;
  displayText(col, line, text);
}

// ---------------------------------------------------------------------------
// @brief Efface une zone dans le buffer (compatible saisies_nb.cpp)
// @param page  Ignore en Phase 1
// @param line  Ligne (0..7)
// @param col   Colonne de depart
// @param width Largeur en caracteres
// @return void
// ---------------------------------------------------------------------------
void OLEDClearZone(uint8_t page, uint8_t line, uint8_t col, uint8_t width)
{
  (void)page;
  if (line >= MAX_LIGNES || width == 0) return;

  uint8_t x = (uint8_t)(col * CHAR_WIDTH);
  uint8_t y = (uint8_t)(line * LINE_HEIGHT);
  uint8_t w = (uint8_t)(width * CHAR_WIDTH);

  u8g2.setDrawColor(0);
  u8g2.drawBox(x, y, w, LINE_HEIGHT);
  u8g2.setDrawColor(1);
}
