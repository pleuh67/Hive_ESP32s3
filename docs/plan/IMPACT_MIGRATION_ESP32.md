# Impact de la migration ATSAMD21 → ESP32-S3

## Contexte

Le projet migre du SODAQ Explorer (ATSAMD21) vers une architecture **ESP32-S3 Master/Slaves** avec BLE mesh, serveur web embarqué, et LoRaWAN via SX1262. Le backend reste **Orange Live Objects** (pas de TTN).

Référence : `docs/20260220_memo_ruches_connectees_ESP.md` (mémo technique Philippe, 20/02/2026)

## Décisions prises (session du 01/03/2026 avec Philippe)

| Question | Décision |
|---|---|
| Nombre de slaves prototype | **3** |
| Format payload | **Nouveau format V2** (24 octets, master + 3 slaves) |
| Décodeur backend | **Go sur Scalingo**, modifiable par Philippe & son pote |
| Interfaces utilisateur | **OLED + clavier + Web** (les deux conservées) |
| Organisation repo | **Un seul repo**, deux env PlatformIO `[env:master]` / `[env:slave]` |
| Cartes SODAQ | **Retirées** |
| Module LoRa | **E22-900M22S (SPI)** à commander — le E22-900T30D (UART) est incompatible RadioLib |
| BLE sécurité | **Pairing avec passkey statique** |
| ADR LoRaWAN | **Activé**, option forcer SF7/9/12 via menu |
| Cellules de charge | **200 kg partout** (erreur mémo pour 50 kg slaves) |
| Capteurs slaves | **Poids + VBat + timestamp** uniquement |
| BME280 | **Remplace DHT22** sur master uniquement |
| DS3231 + EEPROM | **Sur tous les noeuds** |
| OLED + clavier sur slaves | **Oui** (debug/maintenance, débranchable en prod) |
| Alimentation | **1× Li-Ion 18650** + solaire 6V + CN3791 MPPT |
| Pesons physiques | **Réutilisés** (mêmes coefficients Jauge[22]) |
| Credentials | **credentials.h gitignored** |
| Slave autonome sans master | **Non** |
| Timeline | **Chaîne complète** dès le départ |
| Portée BLE | **15 m** entre ruches (OK, BLE ~30m extérieur) |
| Intégration BEEP | **À évaluer** plus tard |

---

## 1. Impact sur le plan de refonte en 4 phases

### Phase 0 — Assainissement → TRANSFORMÉE en "Phase 0 — Migration de base"

**Ce qui reste pertinent :**
- Structuration en arborescence PlatformIO (src/, include/, data/, docs/) — le repo ESP32Web_4 a déjà cette structure
- Suppression du code mort (~430 lignes identifiées)
- Correction des bugs connus (String Arduino, memcpy payload, extern orphelins)
- Factorisation des 4 balances en tableau `HX711 scales[4]`

**Ce qui change :**
- Pas besoin de restructurer le repo ATSAMD — on part du repo ESP32Web_4 comme base
- Le .gitignore, platformio.ini existent déjà
- Les corrections de bugs se font PENDANT le portage, pas avant
- La réduction des globales se fait naturellement en réarchitecturant pour ESP32

**Ce qui disparaît :**
- Les questions sur Arduino IDE vs PlatformIO (PlatformIO, tranché)
- La migration .ino → .cpp (le repo ESP32 est déjà en .cpp)

### Phase 1 — Abstraction radio → TRANSFORMÉE en "Phase 1 — Couche communication multi-protocole"

**Ce qui reste pertinent :**
- Le concept de RadioManager avec interface abstraite
- Le format payload extensible versionné (header + données compactes)
- L'encodage compact des mesures (13 octets vs 36 octets en float)
- La file d'attente d'envoi en RAM

**Ce qui change radicalement :**
- 3 protocoles au lieu de 1 : **BLE** (slaves→master), **LoRaWAN** (master→cloud), **WiFi** (serveur web local)
- Le module radio change : **SX1262 + RadioLib** remplace RN2483A + Sodaq_RN2483
- Le backend LoRaWAN reste Orange Live Objects — le format payload 19 octets doit rester compatible ou le décodeur côté serveur doit être mis à jour
- Le payload doit maintenant transporter les données de N slaves (pas juste 1 noeud)

**Nouveau scope :**
- Service BLE GATT sur le master (réception données slaves)
- Client BLE sur les slaves (envoi périodique au master)
- Agrégation des données de tous les noeuds avant envoi LoRaWAN
- API REST + WebSocket sur le master

### Phase 2 — Fédération LoRa → REMPLACÉE par "Phase 2 — Réseau BLE Master/Slaves"

**Ce qui disparaît complètement :**
- Le protocole P2P LoRa via `mac pause` / `radio tx` / `radio rx`
- La synchronisation temporelle par slots LoRa
- Les commandes RN2483 en mode radio
- Le bilan énergétique collecteur LoRa

**Ce qui le remplace :**
- Topologie BLE mesh maître-esclave (natif ESP32-S3)
- Le master scanne et se connecte aux slaves en BLE périodiquement
- Les slaves sont en deep sleep entre les connexions BLE
- Beaucoup plus simple et fiable que le P2P LoRa bricolé sur RN2483A

**Avantage majeur :** BLE est natif sur l'ESP32-S3 — pas de module externe, pas de commandes AT, consommation ultra-basse en mode advertising.

### Phase 3 — Robustesse production → ENRICHIE

**Ce qui reste :**
- CI/CD GitHub Actions (compilation automatique)
- Watchdog matériel (ESP32 a un WDT intégré)
- Robustesse bus I2C (mêmes principes)
- Logs locaux et diagnostics

**Ce qui s'ajoute :**
- **OTA via WiFi** — mise à jour firmware à distance (énorme gain vs ATSAMD qui nécessite un accès physique)
- **NVS (Non-Volatile Storage)** — remplacement natif de l'EEPROM AT24C32 pour la config
- **Serveur web de diagnostic** — pages de status, logs, config accessibles via navigateur
- **FreeRTOS** — tâches séparées pour BLE, LoRa, Web, mesures (meilleure isolation)
- **SPIFFS/LittleFS** — système de fichiers pour logs, pages web, certificats

**Ce qui change :**
- `NVIC_SystemReset()` → `esp_restart()`
- `ArduinoLowPower` → `esp_sleep_*` (API ESP-IDF)
- Deep sleep ESP32 = redémarrage complet → état à sauvegarder en RTC RAM (`RTC_DATA_ATTR`)

---

## 2. Impact sur le code existant (POC_ATSAMD)

### Code RÉUTILISABLE tel quel (~40% du total)

| Fichier | Lignes | Contenu réutilisable |
|---|---|---|
| `struct.h` | ~300 | Types, structures, énumérations — 100% portable |
| `Convert.cpp` | ~200 | Conversions hex/byte — 100% portable |
| `Saisies_NB.cpp` | ~800 | Machines à états saisies — 100% portable |
| `Menus.cpp` | ~400 | Navigation menus — 95% portable |
| `24C32.cpp` | ~250 | Config EEPROM I2C — 95% portable (si AT24C32 conservé) |
| `DS3231.cpp` | ~200 | Pilote RTC — 90% portable |

### Code ADAPTABLE avec modifications ciblées (~30%)

| Fichier | Ce qui change |
|---|---|
| `define.h` | Pins, serials, includes bibliothèques |
| `var.h` | Supprimer `String`, extern orphelins, remapper objets |
| `Mesures.cpp` | Recalibrer ADC (12 bits ESP32 vs 10 bits SAMD), remplacer `TEMP_SENSOR` |
| `Handle.cpp` | Adapter appels sleep(), ajouter gestion BLE/WiFi |
| `MenusFonc.cpp` | Adapter appels LoRa indirects |
| `OLED.cpp` | Changer bibliothèque (Adafruit SH110X → U8g2 pour SSD1309) |
| `KEYPAD.cpp` | Recalibrer niveaux ADC ×4 |
| `LED_NB.cpp` | Remapper pins LED |
| `DHT22.cpp` | Supprimer si BME280 remplace le DHT22 |
| `setup.cpp` | `NVIC_SystemReset` → `esp_restart`, init LoRa différent |
| `POC_ATSAMD.ino` | Devenir `main.cpp`, adapter interrupts |

### Code à RÉÉCRIRE (~30%)

| Fichier | Raison |
|---|---|
| `RN2483A.cpp` | Module radio complètement différent (SX1262 + RadioLib vs RN2483A + Sodaq) |
| `Power.cpp` | API deep sleep totalement différente (ESP-IDF vs ArduinoLowPower) |
| `ISR.cpp` | `IRAM_ATTR`, supprimer Serial/I2C de l'ISR |

### Données métier CRITIQUES à préserver

| Donnée | Fichier | Importance |
|---|---|---|
| `Jauge[22][4]` | var.h | Coefficients d'étalonnage des 22 pesons physiques |
| `Peson[10][4]` | var.h | Matrice carte → pesons connectés |
| `buildLoraPayload()` format 19 octets | RN2483A.cpp | Compatible décodeur Orange Live Objects |
| `ConfigGenerale_t` | struct.h | Structure de config persistante avec magic number + CRC |
| `HWEUI_List[]`, `AppKey_List[]` | var.h | Identifiants LoRaWAN des cartes physiques |
| Tables `VBatScale_List[]`, `VSolScale_List[]` | var.h | Calibration ADC par carte |

---

## 3. Nouveau code à créer

### 3.1 Architecture logicielle cible (Master)

```
ESP32-S3 Master — Architecture FreeRTOS
├── Tâche Mesures (Core 0)
│   ├── HX711 lecture (1 cellule 200 kg sur master)
│   ├── BME280 (T°/HR/Pression) — remplace DHT22
│   ├── BH1750 (Luminosité lux) — remplace LDR analogique
│   ├── INA219 (Courant/tension solaire)
│   └── ADC (VBat via diviseur résistif)
│
├── Tâche BLE (Core 0)
│   ├── Scan et connexion aux slaves
│   ├── Lecture caractéristiques GATT (poids, temp, vbat)
│   └── Agrégation données dans structure partagée
│
├── Tâche LoRaWAN (Core 1)
│   ├── RadioLib + SX1262 (SPI)
│   ├── OTAA vers Orange Live Objects
│   ├── Construction payload agrégé (master + N slaves)
│   └── File d'attente avec retry
│
├── Tâche Web (Core 1)
│   ├── ESPAsyncWebServer (port 80)
│   ├── API REST : GET /api/data, GET /api/config, POST /api/config
│   ├── WebSocket : données temps réel
│   └── Pages SPIFFS (HTML/CSS/JS)
│
├── Tâche OLED + Menus (Core 0)
│   ├── U8g2 + SSD1309 2,42"
│   ├── Navigation menus (réutilisation Menus.cpp / Saisies_NB.cpp)
│   └── Écrans info (réutilisation OLED.cpp adapté)
│
└── Tâche Énergie (Core 0)
    ├── Gestion deep sleep / light sleep
    ├── Réveil sur alarme RTC (DS3231 → GPIO interrupt)
    └── Mode adaptatif selon VBat
```

### 3.2 Architecture logicielle cible (Slave)

```
ESP32-S3 Slave — Ultra-low-power
├── setup()
│   ├── Init HX711 (1 cellule 50 kg)
│   ├── Init ADC (VBat diviseur résistif)
│   └── Lecture mesures
│
├── BLE Advertising
│   ├── Profile GATT custom
│   ├── Caractéristiques : poids, température (interne), VBat
│   └── Attente connexion master (timeout)
│
└── Deep sleep
    ├── Réveil sur timer RTC ou timer ESP32
    ├── Conso < 20 µA
    └── Redémarrage complet à chaque cycle
```

### 3.3 Fichiers à créer (structure adoptée)

```
src/
├── common/                          ← code partagé master ET slave
│   ├── hx711_manager.cpp/h          ← pesée HX711
│   ├── rtc_manager.cpp/h            ← DS3231 + alarmes + ISR
│   ├── eeprom_manager.cpp/h         ← AT24C32 config persistante
│   ├── display_manager.cpp/h        ← U8g2 + SSD1309
│   ├── keypad.cpp/h                 ← clavier analogique 5 touches
│   ├── power_manager.cpp/h          ← deep sleep ESP32
│   ├── convert.cpp/h                ← conversions hex/byte
│   ├── saisies_nb.cpp/h             ← saisies non-bloquantes
│   └── menus_common.cpp/h           ← calibration, affichage simple
├── master/
│   ├── main.cpp                     ← setup() + loop() master ✅ CRÉÉ
│   ├── ble_master.cpp/h             ← scan, connect, read slaves
│   ├── lora_manager.cpp/h           ← RadioLib + SX1262
│   ├── sensor_manager.cpp/h         ← BME280, BH1750, INA219
│   ├── web_server.cpp/h             ← ESPAsyncWebServer + API REST
│   └── menus_master.cpp/h           ← menus spécifiques master
└── slave/
    ├── main.cpp                     ← setup() + deep sleep ✅ CRÉÉ
    ├── ble_slave.cpp/h              ← GATT server, advertising
    └── menus_slave.cpp/h            ← menus calibration slave

include/
├── config.h                         ← constantes, pins
├── types.h                          ← structures de données
├── credentials.h                    ← AppKey, WiFi — .gitignore ✅ CRÉÉ
└── credentials_example.h            ← template sans secrets ✅ CRÉÉ
```

---

## 4. Changements matériels et leurs impacts logiciels

| Ancien (ATSAMD) | Nouveau (ESP32-S3) | Impact code |
|---|---|---|
| RN2483A (UART, AT commands) | SX1262 Ebyte E22 (SPI) | Réécriture complète couche LoRa, RadioLib au lieu de Sodaq_RN2483 |
| DHT22 (1-wire GPIO) | BME280 (I2C) | Nouveau driver, gain : pression atmosphérique en plus |
| LDR analogique (ADC) | BH1750 (I2C) | Nouveau driver, gain : mesure en lux calibrée |
| — | INA219 (I2C) | Nouveau driver pour mesure courant solaire |
| OLED SH1106 1,3" (Adafruit) | OLED SSD1309 2,42" (U8g2) | Changement bibliothèque affichage |
| 4× HX711 par carte | 1× HX711 par noeud | Simplification, mais N noeuds |
| LiFePO4 3,2V | Li-Ion 18650 3,7V | Recalibration ADC, seuils batterie différents |
| Pas de WiFi | WiFi STA | Nouveau : serveur web, OTA |
| Pas de BLE | BLE master/slave | Nouveau : réseau inter-noeuds |
| ArduinoLowPower (SAMD) | esp_sleep (ESP-IDF) | Réécriture power management |

---

## 5. Questions — TOUTES RÉSOLUES (01/03/2026)

Toutes les questions ont été tranchées avec Philippe. Voir le tableau de décisions en haut de ce document.

### Points d'attention découverts pendant l'analyse

- **Module LoRa E22-900T30D (UART) commandé par erreur** — incompatible RadioLib (qui nécessite SPI). Commander un **E22-900M22S** (SPI, EU868). Le E22-900T30D a aussi un problème de puissance (30 dBm > limite EU868 de 14 dBm).
- **Erreur mémo : cellules 50 kg slaves** — c'est 200 kg partout.
- **OLED + clavier sur tous les noeuds** (pas prévu dans le mémo original qui disait "slaves ultra-low-power BLE seulement") — pour debug/maintenance, débranchable en prod.
