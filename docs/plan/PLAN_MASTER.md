# Plan de travail — Ruches Connectees ESP32-S3

## MISE A JOUR 02/03/2026 — Phases 0 a 4 terminees

Toutes les decisions architecturales ont ete tranchees avec Philippe le 01/03/2026.
Voir `docs/plan/IMPACT_MIGRATION_ESP32.md` pour le detail des decisions.

## Architecture cible

```
1 Master ESP32-S3 (N16R8)          3 Slaves ESP32-S3
WiFi + BLE + LoRaWAN + Web         BLE + HX711 + OLED debug
BME280 + BH1750 + INA219           Deep sleep entre cycles
OLED + clavier + menus             Poids + VBat + timestamp
           |                              |
           +--- BLE GATT (15m) -----------+
           |
     LoRaWAN (SX1262)
           |
   Orange Live Objects
           |
   Decodeur Go (Scalingo)
           |
   Prometheus -> Grafana
```

## Etat d'avancement (02/03/2026)

```
Phase 0 [DONE]        Phase 1 [DONE]        Phase 2 [DONE]
MIGRATION DE BASE      CAPTEURS + OLED       BLE MASTER/SLAVES

[x] Structure PIO      [x] HX711 manager     [x] Slave GATT server
[x] types.h porte      [x] DS3231 + EEPROM   [x] Master GATT client
[x] config.h           [x] OLED (U8g2)       [x] Pairing passkey
[x] convert.cpp        [x] Clavier ADC       [x] Cycle 3 slaves
[x] saisies_nb.cpp     [x] BME280/BH1750     [x] Deep sleep slaves
[x] eeprom_manager     [x] INA219            [x] Donnees SlaveReading_t
[x] rtc_manager        [x] Menus calibration
[x] power_manager

Phase 3 [DONE]        Phase 4 [DONE]
LORAWAN               WEB + SERVEUR

[x] RadioLib SX1262    [x] ESPAsyncWebServer
[x] OTAA Orange LO     [x] WiFi STA + LittleFS
[x] Payload V2 24B     [x] API REST /api/data
[x] Session RTC RAM    [x] API REST /api/status
[x] ADR + forcer SF    [x] POST /api/lora/join
                       [x] Dashboard HTML
```

**Build master** : RAM 17.4% / Flash 34.0%
**Build slave**  : RAM 10.2% / Flash 17.8%

## Phase 0 — Migration de base [DONE]

- [x] Structure repo (`src/common/`, `src/master/`, `src/slave/`, `include/`)
- [x] `platformio.ini` multi-env (master + slave compilent)
- [x] `.gitignore` (credentials, .pio, binaires)
- [x] `credentials.h` + `credentials_example.h`
- [x] `include/types.h` (porte depuis struct.h + SlaveReading_t)
- [x] `include/config.h` — constantes et pins ESP32-S3
- [x] `src/common/convert.cpp/h`
- [x] `src/common/saisies_nb.cpp/h`
- [x] `src/common/eeprom_manager.cpp/h`
- [x] `src/common/rtc_manager.cpp/h`
- [x] Compilation des deux envs sans erreur

## Phase 1 — Capteurs + OLED + menus [DONE]

- [x] `src/common/hx711_manager.cpp/h`
- [x] `src/common/display_manager.cpp/h` — U8g2 + SSD1309
- [x] `src/common/keypad.cpp/h` — clavier ADC 12 bits
- [x] `src/common/power_manager.cpp/h` — esp_sleep
- [x] `src/common/menus_common.cpp/h`
- [x] `src/master/sensor_manager.cpp/h` — BME280, BH1750, INA219
- [x] `src/master/menus_master.cpp/h`
- [x] `src/slave/menus_slave.cpp/h`

## Phase 2 — BLE Master/Slaves [DONE]

- [x] `src/slave/ble_slave.cpp/h` — GATT server, 3 caracteristiques
- [x] `src/master/ble_master.cpp/h` — scan, connect, read, disconnect
- [x] Pairing passkey statique (NimBLE 2.x)
- [x] Encodage : weight = poids_g/10 (int16_t), vbat = (V-2.0)*10 (uint8_t)
- [x] SlaveReading_t slaveReadings[3] rempli par bleMasterCollect

## Phase 3 — LoRaWAN [DONE]

- [x] `src/master/lora_manager.cpp/h` — RadioLib 7.x + SX1262 via SPI
- [x] OTAA join vers Orange Live Objects
- [x] Construction payload V2 (24 octets)
- [x] Persistance session LoRaWAN en RTC RAM (getBufferSession/setBufferSession)
- [x] ADR active, option forcer SF via menu
- [x] `radio.setDio2AsRfSwitch(true)` pour E22-900M22S

## Phase 4 — Serveur web [DONE]

- [x] `src/master/web_server.cpp/h` — ESPAsyncWebServer + WiFi STA
- [x] LittleFS pour fichiers statiques (`data-master/`)
- [x] API REST : `/api/data`, `/api/status`, `/api/lora/join`
- [x] Dashboard HTML (`data-master/index.html`)
- [x] CORS : `Access-Control-Allow-Origin: *`
- [x] Deep sleep desactive si WiFi connecte
- [x] `webServerProcess()` pour actions differees (join LoRa)

## Phases suivantes (a definir)

- [ ] OTA WiFi (ArduinoOTA ou AsyncElegantOTA)
- [ ] Watchdog ESP32
- [ ] Tests terrain : portee BLE entre ruches, duty cycle LoRa
- [ ] Calibration des cellules de charge 200 kg sur les vrais pesons
- [ ] Validation payload cote serveur (decodeur Go + Grafana)
- [ ] Integration BEEP (a evaluer — voir `docs/ANALYSE_BEEP.md`)

## Portabilite du code ATSAMD (migration terminee)

| Categorie | % | Statut |
|-----------|---|--------|
| Reutilisable tel quel | ~40% | Porte dans src/common/ |
| Adaptable | ~30% | Adapte et integre |
| A reecrire | ~30% | Reecrit (LoRa, Power, ISR) |

Les 26 fichiers ATSAMD originaux ont ete supprimes le 02/03/2026
apres verification que tout le code utile etait porte.

## Documentation associee

| Fichier | Description |
|---------|-------------|
| `CLAUDE.md` | Contexte projet pour Claude Code |
| `docs/plan/IMPACT_MIGRATION_ESP32.md` | Analyse d'impact + decisions |
| `docs/20260220_memo_ruches_connectees_ESP.md` | Memo technique ESP32 (Philippe) |
| `docs/ANALYSE_BEEP.md` | Etude plateforme BEEP |
| `docs/plan/ANALYSE_REPO.md` | Audit code ATSAMD (historique) |
