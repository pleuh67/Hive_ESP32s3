# CLAUDE.md — Ruches Connectees ESP32-S3

## Identite du projet

Firmware embarque C++ (Arduino/PlatformIO) pour **ESP32-S3-DevKitC-1** destine a la surveillance de ruches apicoles. Architecture **Master/Slaves** avec communication BLE entre noeuds :

- **1 Master** : collecte poids (HX711), meteo (BME280), luminosite (BH1750), energie (INA219, ADC), agrege les donnees des slaves via BLE, transmet via **LoRaWAN** (SX1262 + RadioLib) vers **Orange Live Objects**, et expose un serveur web local (ESPAsyncWebServer).
- **3 Slaves** : chacun mesure le poids (HX711), la tension batterie (ADC), l'horodatage (DS3231), puis transmet au master via BLE avant de passer en deep sleep.

**Migration depuis** : SODAQ Explorer (ATSAMD21) — le code metier portable (~40%) est porte depuis le repo `pleuh67/POC_ATSAMD`.

## Infrastructure backend (existante)

- **Orange Live Objects** : reception des trames LoRaWAN (OTAA)
- **Decodeur Go** : sur Scalingo, decode le payload V2 (24 octets) — maintenu par Philippe et son pote
- **Prometheus + Grafana** : visualisation — operationnel, hors perimetre firmware
- **BEEP** (beep.nl) : integration a evaluer plus tard — voir `docs/ANALYSE_BEEP.md`

## Contraintes fondamentales

- **RAM** : 512 Ko SRAM + 8 Mo PSRAM (master N16R8) — plus large que ATSAMD mais toujours pas de fragmentation heap
- **Flash** : 16 Mo (master), 4 Mo (slave)
- **Consommation** : objectif < 20 uA en deep sleep, budget 1x Li-Ion 18650 + solaire 6V + CN3791 MPPT
- **Fiabilite** : exterieur sans supervision pendant des semaines
- **Temps reel** : loop() ne doit jamais bloquer (pas de `delay()` en production)
- **LoRaWAN** : payload 24 octets max, duty cycle 1%, EU868
- **BLE** : NimBLE avec pairing passkey statique, portee ~15 m entre ruches
- **Pas de `String` Arduino** — utiliser `char[]` + `snprintf`
- **Pas d'allocation dynamique** (`new`, `malloc`) — memoire statique uniquement

## Architecture materielle

### Master (ESP32-S3-DevKitC-1 N16R8)

```
ESP32-S3 Master
+-- I2C bus
|   +-- DS3231 RTC + EEPROM AT24C32 (0x68 / 0x57)
|   +-- OLED SSD1309 2,42" (0x3C) — U8g2
|   +-- BME280 (0x76) — temperature, humidite, pression
|   +-- BH1750 (0x23) — luminosite en lux
|   +-- INA219 (0x40) — courant/tension solaire
+-- SPI bus
|   +-- SX1262 E22-868M22S (LoRaWAN) — RadioLib
+-- GPIO
|   +-- HX711 x1 (cellule 200 kg)
|   +-- Alarme RTC (interrupt FALLING)
+-- ADC
|   +-- VBat (pont diviseur)
|   +-- VSol (pont diviseur)
|   +-- Clavier analogique 5 touches
+-- BLE : GATT client (scan + lecture 3 slaves)
+-- WiFi STA : serveur web (ESPAsyncWebServer)
+-- Alimentation : Li-Ion 18650 + solaire 6V + CN3791 MPPT
```

### Slave (ESP32-S3)

```
ESP32-S3 Slave
+-- I2C bus
|   +-- DS3231 RTC + EEPROM AT24C32
|   +-- OLED SSD1309 2,42" (debug/maintenance, debranchable)
+-- GPIO
|   +-- HX711 x1 (cellule 200 kg)
|   +-- Clavier analogique 5 touches (debug/maintenance)
+-- ADC
|   +-- VBat (pont diviseur)
+-- BLE : GATT server (advertising + envoi donnees au master)
+-- Alimentation : Li-Ion 18650 + solaire 6V + CN3791 MPPT
```

## Toolchain

- **IDE** : VS Code + PlatformIO (ou Cursor + PlatformIO CLI)
- **Build master** : `pio run -e master`
- **Build slave** : `pio run -e slave`
- **Upload master** : `pio run -e master -t upload`
- **Upload slave** : `pio run -e slave -t upload`
- **Monitor serie** : `pio device monitor -b 115200`
- **Upload fichiers web** : `pio run -e master -t uploadfs`

## Organisation du code

```
src/
+-- common/                    <- code partage master ET slave
|   +-- hx711_manager.cpp/h    <- pesee HX711
|   +-- rtc_manager.cpp/h      <- DS3231 + alarmes + ISR
|   +-- eeprom_manager.cpp/h   <- AT24C32 config persistante
|   +-- display_manager.cpp/h  <- U8g2 + SSD1309
|   +-- keypad.cpp/h           <- clavier analogique 5 touches
|   +-- power_manager.cpp/h    <- deep sleep ESP32
|   +-- convert.cpp/h          <- conversions hex/byte
|   +-- saisies_nb.cpp/h       <- saisies non-bloquantes
|   +-- menus_common.cpp/h     <- calibration, affichage simple
+-- master/
|   +-- main.cpp               <- setup() + loop() master
|   +-- ble_master.cpp/h       <- scan, connect, read slaves
|   +-- lora_manager.cpp/h     <- RadioLib + SX1262
|   +-- sensor_manager.cpp/h   <- BME280, BH1750, INA219
|   +-- web_server.cpp/h       <- ESPAsyncWebServer + API REST
|   +-- menus_master.cpp/h     <- menus specifiques master
+-- slave/
    +-- main.cpp               <- setup() + deep sleep cycle
    +-- ble_slave.cpp/h        <- GATT server, advertising
    +-- menus_slave.cpp/h      <- menus calibration slave

include/
+-- config.h                   <- constantes, pins, macros
+-- types.h                    <- structures de donnees
+-- credentials.h              <- AppKey, WiFi, BLE passkey — .gitignore !
+-- credentials_example.h      <- template sans secrets

data-master/                   <- fichiers web (HTML/CSS/JS) pour LittleFS
docs/                          <- documentation
```

**Selection des fichiers par environnement :**
- `[env:master]` : `build_src_filter = -<*> +<common/**/*> +<master/**/*>`
- `[env:slave]` : `build_src_filter = -<*> +<common/**/*> +<slave/**/*>`
- Flags : `-DIS_MASTER=1` ou `-DIS_SLAVE=1`

## Conventions de code

- **Style d'accolades** : alignement vertical (Allman)
- **Indentation** : 2 espaces
- **Noms** : fonctions et variables en camelCase, `#define` en MAJUSCULES
- **Commentaires** : en francais avec accents
- **Textes affiches (OLED/serie)** : francais SANS accents
- **Debug** : `Serial.printf()` (plus de `debugSerial`/`SerialUSB`)
- **Types** : toujours `uint8_t`, `int16_t`, `float` — jamais `int` seul
- **ISR** : `IRAM_ATTR`, flag only (pas de Serial ni I2C dans les ISR)
- **Deep sleep** : sauvegarder l'etat en RTC RAM (`RTC_DATA_ATTR`)

## Structures de donnees critiques

```cpp
// Mesures capteurs — rempli par acquisition, lu par build payload
HiveSensor_Data_t HiveSensor_Data;

// Configuration persistante (EEPROM AT24C32)
// NE JAMAIS modifier ConfigGenerale_t sans incrementer CONFIG_VERSION
ConfigGenerale_t config; // .magicNumber, .applicatif, .materiel, .checksum

// Donnees des slaves recues via BLE
SlaveReading_t slaveReadings[3]; // address, weight, vbat, timestamp, valid

// Coefficients d'etalonnage des pesons physiques
Jauge[22][4]; // tare, echelle, tempTare, compTemp
Peson[10][4]; // matrice carte -> pesons connectes
```

## Regles pour Claude Code

### Ce qu'il FAUT faire

- Tester la compilation des DEUX environnements : `pio run -e master && pio run -e slave`
- Utiliser `snprintf` (jamais `sprintf`)
- Marquer `volatile` toute variable partagee ISR <-> loop
- Marquer `IRAM_ATTR` toutes les fonctions d'interruption
- Mettre les secrets dans `credentials.h` (gitignore)
- Documenter les fonctions en francais

### Ce qu'il NE FAUT PAS faire

- **Pas de `String` Arduino** — `char[]` + `snprintf`
- **Pas de `delay()` dans loop()** — non-bloquant uniquement
- **Pas d'allocation dynamique** (`new`, `malloc`, `std::vector`)
- **Pas de Serial dans les ISR** — flag only
- **Ne pas modifier `ConfigGenerale_t`** sans incrementer `CONFIG_VERSION`
- **Ne pas committer `credentials.h`** — secrets LoRa, WiFi, BLE

### Workflow de modification

1. Modifier le code
2. Compiler les deux envs : `pio run -e master && pio run -e slave`
3. Tester sur carte physique si possible
4. Commit avec message descriptif en francais

## Bibliotheques cibles

### Partagees (master + slave)

| Lib | Version | Usage |
|-----|---------|-------|
| NimBLE-Arduino | ^2.1.0 (h2zero) | BLE GATT client/server |
| HX711 | ^0.7.5 (bogde) | Cellules de charge |
| RTClib | ^2.1.4 (Adafruit) | DS3231 RTC |
| U8g2 | ^2.35.19 (olikraus) | OLED SSD1309 2,42" |

### Master uniquement

| Lib | Version | Usage |
|-----|---------|-------|
| RadioLib | ^7.4.0 (jgromes) | LoRaWAN SX1262 |
| ESPAsyncWebServer | ^3.10.0 (esp32async) | Serveur web |
| AsyncTCP | ^3.4.10 (esp32async) | TCP async pour web |
| Adafruit BME280 | ^2.2.4 | Temperature/humidite/pression |
| BH1750 | ^1.3.0 (claws) | Luminosite lux |
| Adafruit INA219 | ^1.2.1 | Courant/tension solaire |

## Payload LoRaWAN V2 (24 octets)

```
[0]    Version (0x02)          [1]    NodeCount
[2]    Flags                   [3-4]  Poids master (x0.01 kg)
[5-6]  Temperature (x0.1 C)   [7]    Humidite (x0.5 %)
[8-9]  Pression (x0.1 hPa)    [10-11] Luminosite (lux)
[12]   VBat master (x0.1V)    [13]   VSol (x0.1V)
[14]   ISol (x10 mA)          [15-16] Poids slave 1
[17]   VBat slave 1            [18-19] Poids slave 2
[20]   VBat slave 2            [21-22] Poids slave 3
[23]   VBat slave 3
```

Slave absent : poids = 0x7FFF, VBat = 0xFF

## Decisions architecturales (01/03/2026)

| Decision | Choix |
|----------|-------|
| Nombre de slaves | 3 |
| Backend LoRaWAN | Orange Live Objects (pas TTN) |
| Module LoRa | E22-868M22S (SPI) — le E22-900T30D (UART) est incompatible |
| BLE | NimBLE avec pairing passkey statique |
| ADR LoRaWAN | Active, option forcer SF via menu |
| Cellules de charge | 200 kg partout |
| Capteurs slaves | Poids + VBat + timestamp uniquement |
| BME280 | Remplace DHT22 sur master |
| OLED + clavier slaves | Oui (debug/maintenance, debranchable) |
| Alimentation | 1x Li-Ion 18650 + solaire 6V + CN3791 |
| Credentials | credentials.h gitignore |
| Organisation repo | Un seul repo, deux env PlatformIO |
| Cartes SODAQ | Retirees |
| Integration BEEP | A evaluer plus tard |

## Etat d'avancement (02/03/2026)

- **Phase 0** — Migration de base : DONE
- **Phase 1** — Capteurs + OLED + menus : DONE
- **Phase 2** — BLE Master/Slaves : DONE
- **Phase 3** — LoRaWAN OTAA + payload V2 : DONE
- **Phase 4** — Serveur web WiFi + dashboard : DONE

Build master : RAM 17.4% / Flash 34.0%
Build slave  : RAM 10.2% / Flash 17.8%

### Prochaines etapes

- Tests terrain : portee BLE, duty cycle LoRa, deep sleep < 20 uA
- Calibration des cellules de charge 200 kg
- Validation payload cote serveur (decodeur Go + Grafana)
- OTA WiFi, watchdog ESP32
- Integration BEEP (a evaluer)

## Documentation

| Fichier | Description |
|---------|-------------|
| `docs/plan/PLAN_MASTER.md` | Vue d'ensemble des phases |
| `docs/plan/IMPACT_MIGRATION_ESP32.md` | Analyse d'impact migration |
| `docs/20260220_memo_ruches_connectees_ESP.md` | Memo technique ESP32 (Philippe) |
| `docs/ANALYSE_BEEP.md` | Etude plateforme BEEP |
| `docs/plan/ANALYSE_REPO.md` | Audit du code source ATSAMD |

## Portage ATSAMD termine (02/03/2026)

Les 26 fichiers ATSAMD originaux ont ete supprimes apres portage complet.
L'historique git conserve la trace (commit ee37fd6).

| Fichier ATSAMD (supprime) | Porte dans |
|---------------------------|-----------|
| struct.h | include/types.h |
| Convert.cpp | src/common/convert.cpp |
| Saisies_NB.cpp | src/common/saisies_nb.cpp |
| 24C32.cpp | src/common/eeprom_manager.cpp |
| DS3231.cpp + ISR.cpp | src/common/rtc_manager.cpp |
| OLED.cpp | src/common/display_manager.cpp |
| KEYPAD.cpp | src/common/keypad.cpp |
| Mesures.cpp | src/common/hx711_manager.cpp |
| Menus.cpp + MenusFonc.cpp + Handle.cpp | src/common/menus_common.cpp |
| RN2483A.cpp | src/master/lora_manager.cpp |
| Power.cpp | src/common/power_manager.cpp |
| setup.cpp | src/master/main.cpp + src/slave/main.cpp |
