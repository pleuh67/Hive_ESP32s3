// config.h — Constantes, pins et macros pour ESP32-S3
// Porte depuis define.h + var.h (POC_ATSAMD)
// Les pins GPIO sont a ajuster selon le cablage reel

#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

// ===== IDENTIFICATION PROJET =====
#define PROJECT_NAME    "Ruches Connectees ESP32-S3"
#define VERSION         "2.0.0-dev"

// ===== NOMBRE DE NOEUDS =====
#define NUM_SLAVES      3

// ===== MACROS UTILITAIRES =====
#define NIBBLE_TO_HEX_CHAR(i) ((i <= 9) ? ('0' + i) : ('A' - 10 + i))
#define HIGH_NIBBLE(i) ((i >> 4) & 0x0F)
#define LOW_NIBBLE(i) (i & 0x0F)

// ===== TIMING =====
#define WAKEUP_INTERVAL_MIN     15      // Intervalle reveil en minutes
#define INTERVAL_1SEC           1000    // 1 seconde en ms
#define TIMEOUT_SAISIE          20000   // Timeout saisies ecrans (ms)
#define BLE_ADVERTISING_SEC     30      // Duree advertising BLE (slave)
#define BLE_CONNECT_TIMEOUT_MS  5000    // Timeout connexion BLE (master)
#define BLE_MAX_RETRIES         3       // Tentatives connexion par slave
#define DEFAULT_SF              9       // Spreading Factor par defaut

// ===== DUREES LED (ms) =====
#define RED_LED_DURATION        100
#define GREEN_LED_DURATION      100
#define BLUE_LED_DURATION       100
#define BUILTIN_LED_DURATION    100

// ===== I2C ADRESSES =====
#define DS3231_ADDRESS          0x68
#define EEPROM_ADDRESS          0x57
#define OLED_ADDRESS            0x3C
#ifndef BME280_ADDRESS
#define BME280_ADDRESS          0x76    // Master uniquement
#endif
#define BH1750_ADDRESS          0x23    // Master uniquement
#ifndef INA219_ADDRESS
#define INA219_ADDRESS          0x40    // Master uniquement
#endif

// ===== EEPROM CONFIGURATION =====
#define CONFIG_VERSION          200     // Version 2.0.0
#define CONFIG_EEPROM_START     0x0000
#define CONFIG_MAGIC_NUMBER     0xFF04  // Incremente vs ATSAMD (0xFF03)

// ===== PINS GPIO ESP32-S3 =====
// TODO: ajuster selon le cablage reel du prototype

// I2C (partage : RTC, EEPROM, OLED, BME280, BH1750, INA219)
#define PIN_I2C_SDA             8
#define PIN_I2C_SCL             9

// HX711 (1 cellule par noeud)
#define PIN_HX711_SCK           4
#define PIN_HX711_DOUT          5

// RTC alarme (interrupt)
#define PIN_RTC_INT             6

// Clavier analogique 5 touches
#define PIN_KBD_ANA             1       // ADC1_CH0

// ADC mesures analogiques
#define PIN_VBAT_ADC            2       // ADC1_CH1
#define PIN_VSOL_ADC            3       // ADC1_CH2 (master uniquement)

// LED integree
#define PIN_LED_BUILTIN         48      // LED RGB neopixel sur DevKitC-1

#ifdef IS_MASTER
// SPI pour SX1262 (LoRa) — master uniquement
#define PIN_LORA_SCK            12
#define PIN_LORA_MISO           13
#define PIN_LORA_MOSI           11
#define PIN_LORA_NSS            10
#define PIN_LORA_DIO1           14
#define PIN_LORA_BUSY           15
#define PIN_LORA_RESET          16
#endif

// ===== CLAVIER ANALOGIQUE =====
#define NB_KEYS                 5
#define KBD_TOLERANCE           80      // Tolerance ADC 12 bits (vs 20 sur 10 bits)
#define DEBOUNCE_COUNT          5
#define DEBOUNCE_DELAY_MS       1

// Niveaux ADC 12 bits pour ESP32-S3 (a calibrer sur le prototype)
// ATSAMD 10 bits : {10, 149, 332, 501, 735}
// ESP32 12 bits  : x4 approximatif, a affiner avec esp_adc_cal
static const uint16_t KBD_LEVELS[NB_KEYS] = {40, 596, 1328, 2004, 2940};

// Alias touches selon usage
#define MOINS       KEY_3
#define PLUS        KEY_2
#define LEFT        KEY_1
#define RIGHT       KEY_4
#define VALIDE      KEY_5
#define UP          KEY_2
#define DOWN        KEY_3
#define ANNULER_4   KEY_4

// ===== OLED CONFIGURATION =====
// SSD1309 2,42" via U8g2
#define SCREEN_WIDTH            128
#define SCREEN_HEIGHT           64
#define MAX_LIGNES              8
#define TAILLE_LIGNE            8
#define OLED_Max_Col            21      // 128 / 6 pixels par caractere
#define OLED_Col                6       // Largeur caractere en pixels

// ===== MENU CONFIGURATION =====
#define MAX_MENU_DEPTH          5

// Nombre d'options par menu (a ajuster apres portage des menus)
#define M0_ITEM                 5       // Menu Demarrage
#define M01_ITEM                7       // Menu config. Systeme
#define M02_ITEM                8       // Menu config. LoRa
#define M03_ITEM                5       // Menu Calib. Tensions
#define M033_ITEM               5
#define M04_ITEM                7       // Menu Calib. Balances
#define M04x_ITEM               5

// ===== LISTES =====
#define LIST_SF                 4
#define LIST_RUCHERS            12

// ===== PAYLOAD LORAWAN V2 =====
#define PAYLOAD_VERSION         0x02
#define PAYLOAD_SIZE_V2         24      // Master + 3 slaves
#define HEXPAYLOAD_SIZE_V2      48
#define PAYLOAD_INVALID_WEIGHT  0x7FFF  // Slave absent
#define PAYLOAD_INVALID_VBAT    0xFF    // Slave absent

// ===== HX711 =====
#define HX711_NB_LECTURES       10      // Nombre de lectures pour moyenne
#define HX711_AVR_SETUP         10      // Moyenne au setup
#define HX711_AVR_CYCLE         3       // Moyenne en cycle mesure

// ===== MACROS PESONS =====
// Acces aux coefficients de calibration via les tables Jauge/Peson
#define pesonTare(num)    Jauge[Peson[config.materiel.Num_Carte][num]][0]
#define pesonScale(num)   Jauge[Peson[config.materiel.Num_Carte][num]][1]
#define TareTemp(num)     Jauge[Peson[config.materiel.Num_Carte][num]][2]
#define CompTemp(num)     Jauge[Peson[config.materiel.Num_Carte][num]][3]
#define pesonNum(num)     Peson[config.materiel.Num_Carte][num]

// Calcul poids en g et kg
#define poidsBal_g(num)   HiveSensor_Data.HX711Weight[num]
#define poidsBal_kg(num)  abs((Contrainte_List[num]-pesonTare(num))/pesonScale(num))

// ===== BUFFERS =====
#define SERIALBUFLEN            256
#define OLEDBUFLEN              (128 * 21)

// ===== MACROS DE LOG =====
#define LOG_ERROR(msg)   Serial.print("[ERROR] "); Serial.println(msg)
#define LOG_WARNING(msg) Serial.print("[WARN]  "); Serial.println(msg)
#define LOG_INFO(msg)    Serial.print("[INFO]  "); Serial.println(msg)
#define LOG_DEBUG(msg)   Serial.print("[DEBUG] "); Serial.println(msg)

// ===== VALEURS ERREUR =====
#define TEMP_ERR                99

// ===== BLE CONFIGURATION =====
#define BLE_SERVICE_UUID        "4843-5256-4348-4553" // "HCRVCHES" en hex
#define BLE_CHAR_WEIGHT_UUID    "4843-5256-0001-0000"
#define BLE_CHAR_VBAT_UUID      "4843-5256-0002-0000"
#define BLE_CHAR_TIME_UUID      "4843-5256-0003-0000"
#define BLE_ADV_INTERVAL_MS     1280    // Advertising lent pour economie

// ===== DONNEES DE CALIBRATION =====
// Jauges de contrainte J00 a J21
// Format : {Tare, Echelle, TareTemp, CompTemp}
// Ces valeurs proviennent des pesons physiques calibres sur les prototypes ATSAMD
// Elles sont reutilisees sur ESP32 (memes cellules de charge)

static const float Jauge[22][4] = {
  {0, 0, 0, 0},                          // J00 : pas de peson
  {178666, 108.5, 20, 0},                // J01 : 20 kg
  {30250, 21.2, 20, 0},                  // J02
  {-21000, -23208.92, 20, 0},            // J03
  {31000, 32000, 20, 0},                 // J04
  {41000, 42000, 20, 0},                 // J05
  {-142358, 20048, 19.7, 0},             // J06 : BAL_A 200 kg
  {61000, 62000, 20, 0},                 // J07 : MS 200 kg
  {-35751, -22785.07079, 20, 0},         // J08 : SL proto1 200 kg
  {-28026, -22990.56199, 20, 0},         // J09 : MS proto1 200 kg
  {374942, 1145.58, 20, 0},              // J10 : 2 kg
  {4798647, 1053.71, 20, 0},             // J11 : 2 kg
  {179568, 1056.40, 20, 0},              // J12 : 2 kg
  {20369, 19.93, 20, -2},                // J13 : MS proto1 Master 200 kg
  {120895, 103286.1433, 20, 1},          // J14 : 2/20 kg
  {139983, 20.46, 17.1, 0.048341},       // J15 : SFX proto1 SLC 200 kg
  {-4031212, 1117, 20, 1},               // J16 : proto1 2 kg
  {150625, 104371.5144, 20, 1},          // J17 : 2/20 kg
  {34134.50, 103.77, 20, 0},             // J18 : proto1 20 kg
  {191991, 109204.6332, 20, 1},          // J19 : proto1 20 kg + DHT22
  {22005.70, 97.49, 20, 0},              // J20
  {-21641, 106727.5847, 20, 1}           // J21 : 2/20 kg
};

// Matrice carte -> pesons connectes (A, B, C, D)
// Index = Num_Carte, valeur = numero de jauge dans Jauge[]
static const int Peson[10][4] = {
  {0, 0, 0, 0},          // 0 : Module LoRa pas lu
  {0, 0, 0, 17},         // 1 : 0004A30B0020300A carte HS
  {13, 8, 9, 0},         // 2 : 0004A30B0024BF45
  {6, 9, 3, 8},          // 3 : 0004A30B00EEEE01 Loess
  {0, 18, 0, 0},         // 4 : 0004A30B00EEA5D5 Verger
  {19, 21, 14, 17},      // 5 : 0004A30B00F547CF Cave
  {6, 0, 0, 0},          // 6
  {7, 0, 0, 0},          // 7
  {8, 0, 0, 0},          // 8
  {9, 0, 0, 0}           // 9
};

// Calibration ADC par carte (pont diviseur VBat)
static const float VBatScale_List[10] = {
  0.0032252, 0.0032252, 0.0032252, 0.0032252, 0.0032252,
  0.003222,  0.0032252, 0.0032252, 0.0032252, 0.0032252
};

// Calibration ADC par carte (pont diviseur VSol)
static const float VSolScale_List[10] = {
  0.0032555, 0.0032555, 0.0032555, 0.0032555, 0.0032555,
  0.0032555, 0.0032555, 0.0032555, 0.0032555, 0.0032555
};

#endif // CONFIG_H
