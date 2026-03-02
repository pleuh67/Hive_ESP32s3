# Ruches Connectees ESP32-S3

Firmware embarque C++ (Arduino/PlatformIO) pour la surveillance de ruches apicoles.
Architecture **Master/Slaves** sur **ESP32-S3-DevKitC-1** avec communication BLE.

## Architecture

```
1 Master ESP32-S3 N16R8              3 Slaves ESP32-S3
WiFi + BLE + LoRaWAN + Web           BLE + HX711 + OLED debug
BME280 + BH1750 + INA219             Deep sleep entre cycles
OLED + clavier + menus               Poids + VBat + timestamp
           |                                |
           +------- BLE GATT (15 m) --------+
           |
     LoRaWAN SX1262 (E22-900M22S)
           |
   Orange Live Objects
           |
   Decodeur Go (Scalingo)
           |
   Prometheus -> Grafana
```

## Build

```bash
pio run -e master           # Compiler master
pio run -e slave            # Compiler slave
pio run -e master -t upload # Flasher master
pio run -e slave -t upload  # Flasher slave
pio run -e master -t uploadfs  # Upload fichiers web (LittleFS)
pio device monitor -b 115200   # Moniteur serie
```

## Materiel

### Master

| Composant | Interface | Adresse/Pins |
|-----------|-----------|-------------|
| SSD1309 OLED 2,42" | I2C (SDA=8, SCL=9) | 0x3C |
| DS3231 RTC | I2C | 0x68 |
| AT24C32 EEPROM | I2C | 0x57 |
| BME280 | I2C | 0x76 |
| BH1750 | I2C | 0x23 |
| INA219 | I2C | 0x40 |
| HX711 | GPIO | SCK=4, DOUT=5 |
| SX1262 E22-900M22S | SPI | SCK=12, MISO=13, MOSI=11, NSS=10 |
| Clavier analogique 5 touches | ADC | GPIO1 |
| VBat | ADC | GPIO2 |
| VSol | ADC | GPIO3 |

### Slave

| Composant | Interface | Pins |
|-----------|-----------|------|
| SSD1309 OLED 2,42" | I2C (SDA=8, SCL=9) | 0x3C |
| DS3231 RTC | I2C | 0x68 |
| AT24C32 EEPROM | I2C | 0x57 |
| HX711 | GPIO | SCK=4, DOUT=5 |
| Clavier analogique 5 touches | ADC | GPIO1 |
| VBat | ADC | GPIO2 |

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

## Structure du repo

```
src/
+-- common/          # Code partage master + slave
+-- master/          # Master uniquement (LoRa, BLE client, Web, capteurs)
+-- slave/           # Slave uniquement (BLE server, deep sleep)
include/             # config.h, types.h, credentials.h
data-master/         # Fichiers web (LittleFS)
docs/                # Documentation
```

## Librairies

| Lib | Version | Usage |
|-----|---------|-------|
| NimBLE-Arduino | ^2.1.0 | BLE GATT master/slave |
| HX711 | ^0.7.5 | Cellules de charge |
| RTClib | ^2.1.4 | DS3231 RTC |
| U8g2 | ^2.35.19 | OLED SSD1309 |
| RadioLib | ^7.4.0 | LoRaWAN SX1262 |
| ESPAsyncWebServer | ^3.10.0 | Serveur web |
| Adafruit BME280 | ^2.2.4 | Meteo |
| BH1750 | ^1.3.0 | Luminosite |
| Adafruit INA219 | ^1.2.1 | Courant/tension solaire |

## Credentials

Copier `include/credentials_example.h` vers `include/credentials.h` et remplir :
- `WIFI_SSID` / `WIFI_PASSWORD`
- `LORA_DEV_EUI`, `LORA_JOIN_EUI`, `LORA_APP_KEY`, `LORA_NWK_KEY`
- `BLE_PASSKEY`

`credentials.h` est dans `.gitignore` — ne jamais commiter les secrets.
