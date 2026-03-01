// display_manager.h — Affichage OLED SSD1309 via U8g2
// Phase 1 — common master + slave
// Remplace Adafruit SH1106/SSD1306 du POC_ATSAMD
// API compatible avec les appels de saisies_nb.cpp

#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <U8g2lib.h>
#include <stdint.h>

// ===== DIMENSIONS (coherent avec config.h) =====
// 128x64, 8 lignes de 8 px, 21 colonnes de 6 px

// ===== INITIALISATION =====

// Initialise l'ecran U8g2 SSD1309 via I2C
void displayInit(void);

// ===== PRIMITIVES BUFFER =====

// Efface le buffer (ne modifie pas l'ecran)
void displayClear(void);

// Envoie le buffer vers l'ecran
void displayFlush(void);

// ===== ECRITURE =====

// Ecrit du texte a une position (col en chars, row en lignes 0..7)
void displayText(uint8_t col, uint8_t row, const char* text);

// Ecrit du texte en inverse video (fond blanc, texte noir)
void displayTextInverse(uint8_t col, uint8_t row, const char* text);

// Efface une ligne entiere (row 0..7)
void displayClearRow(uint8_t row);

// ===== API HAUTE NIVEAU (compatible saisies_nb.cpp) =====

// Affiche un message de debug sur la derniere ligne, flush immediat
void OLEDDebugDisplay(const char* message);

// Affiche un message sur la ligne 7 (status bar)
// defilant et inverse non implmementes en Phase 1
void OLEDDisplayMessageL8(const char* message, bool defilant, bool inverse);

// Ecrit du texte avec l'API de saisies_nb.cpp :
// page (ignore en Phase 1), line (0..7), col (0..20), text
void OLEDDrawText(uint8_t page, uint8_t line, uint8_t col, const char* text);

// Efface une zone dans saisies_nb (page ignore, line 0..7, col, width)
void OLEDClearZone(uint8_t page, uint8_t line, uint8_t col, uint8_t width);

// ===== OBJET U8G2 GLOBAL =====
extern U8G2_SSD1309_128X64_NONAME2_F_HW_I2C u8g2;

#endif // DISPLAY_MANAGER_H
