**Réseau de Balances Connectées**

pour Ruches Apicoles --- Mémo Technique Préliminaire

*ESP32-S3 \| BLE \| LoRaWAN \| Serveur Web embarqué*

> **ERRATA (01/03/2026)** :
> - Backend = **Orange Live Objects** (pas TTN comme mentionné plus bas)
> - Cellules de charge = **200 kg partout** (pas 50 kg pour les slaves)
> - Module LoRa = **E22-900M22S (SPI)** requis — le E22-900T30D (UART) est incompatible RadioLib
> - OLED + clavier analogique sur **tous les noeuds** (debug/maintenance)
> - Librairie BLE = **NimBLE-Arduino** (pas la lib BLE native ESP32)

**1. Contexte et Objectifs**

Ce projet vise à construire un réseau de balances connectées pour le suivi pondéral de ruches apicoles. Les données (poids, température, batterie) sont collectées localement via BLE par un nœud maître qui les transmet à un serveur distant via LoRaWAN.

**2. Architecture Générale**

**Le réseau comprend un nœud maître (Master) et 1 à 8 nœuds esclaves (Slaves) :**

• Master : ESP32-S3 avec Wi-Fi/BLE/LoRaWAN, serveur web embarqué, OLED SSD1309, RTC DS3231, BME280, BH1750, HX711 + cellule 200 kg, alimentation solaire.

• Slaves : ESP32-S3 BLE seulement, HX711 + cellule 50 kg, alimentation solaire, ultra-low-power.

**3. Spécifications Techniques**

**3.1 Topologie réseau**

BLE mesh maître-esclave (1 master ↔ jusqu\'à 8 slaves), LoRaWAN Class A vers The Things Network (TTN), serveur web embarqué sur ESP32-S3 (Wi-Fi).

**3.2 Fréquence de mesure**

Mesure toutes les 15 minutes (configurable), transmission LoRaWAN toutes les heures (ou sur seuil de variation de poids).

**3.3 Autonomie**

Batterie Li-Ion 18650 2 500 mAh + panneau solaire 6 V 2--5 W. Objectif : autonomie \> 7 jours sans soleil.

**4. Protocoles de Communication**

**4.1 BLE (Bluetooth Low Energy)**

Profile GATT custom. UUID service ruche, caractéristiques poids / température / tension batterie. Connexion périodique (réduit consommation slaves).

**4.2 LoRaWAN**

Classe A, SF7--SF12 auto-adaptatif (ADR), fréquence EU868, payload CayenneLPP ou binaire optimisé. Librairie : jgromes/RadioLib ≥ 6.6.0.

**4.3 Wi-Fi (Master seulement)**

STA mode vers routeur local. Serveur HTTP/WebSocket pour visualisation données en temps réel et configuration.

**5. Alimentation et Gestion Énergie**

Chaque nœud est alimenté par une batterie Li-Ion 18650 chargée par panneau solaire via un régulateur MPPT CN3791. Le master dispose d\'un INA219 pour la mesure précise du courant solaire. Les slaves utilisent un diviseur résistif sur ADC pour la tension batterie.

Modes de fonctionnement : actif (mesure + BLE), sommeil léger (BLE en attente), deep sleep (ESP32-S3 \< 20 µA). Le master reste en sommeil léger pour maintenir la connectivité BLE et Wi-Fi.

**6. Format des Données**

**6.1 Payload LoRaWAN**

Format binaire compact (12 octets/nœud) : poids (uint16 × 0,1 kg), température (int16 × 0,1 °C), tension (uint8 × 0,1 V), flags (bit-field).

**6.2 API REST embarquée**

GET /api/data → JSON avec tous les nœuds. GET /api/config → configuration système. POST /api/config → mise à jour paramètres.

**7. Développement et Outils**

IDE : Arduino IDE 2.x ou PlatformIO. Board package : esp32 ≥ 3.0.0 (Espressif). Librairies principales : jgromes/RadioLib, adafruit/RTClib, adafruit/Adafruit_BME280, claws/BH1750, bogde/HX711, olikraus/U8g2.

**ANNEXE A --- Catalogue des Modules Hardware**

*Références commerciales AliExpress/Ebyte · Prix indicatifs 2025 · Liens cliquables*

**A.1 Microcontrôleur --- ESP32-S3**

  ------------------------------------ ----------------------------------------------------------------- --------------------- ---------------------------------------------------------------------------------------------------------------
  **Module**                           **Caractéristiques**                                              **Prix AliExpress**   **Lien**

  ESP32-S3-DevKitC-1 (⭐ RECOMMANDÉ)   16 Mo Flash, 8 Mo PSRAM, 36 GPIO, USB-C, Espressif officiel       8--12 €               🔗 [[Recherche AliExpress]{.underline}](https://www.aliexpress.com/w/wholesale-ESP32-S3-DevKitC-1-N16R8.html)

  ESP32-S3 Zero (Waveshare)            Format 25×18 mm ultra-compact --- pour slaves en boîtier étroit   5--8 €                🔗 [[Recherche AliExpress]{.underline}](https://www.aliexpress.com/w/wholesale-ESP32-S3-Zero.html)

  ESP32-S3-WROOM-1 (nu)                Module SMD pour PCB custom, 4--6 €                                4--6 €                🔗 [[Recherche AliExpress]{.underline}](https://www.aliexpress.com/w/wholesale-ESP32-S3-WROOM-1.html)
  ------------------------------------ ----------------------------------------------------------------- --------------------- ---------------------------------------------------------------------------------------------------------------

> *⚠ Choisir impérativement la version N16R8 (16 Mo Flash + 8 Mo PSRAM) pour le master --- indispensable pour serveur web + BLE + LoRa simultanés. Version N8R2 suffisante pour les slaves.*

**A.2 Module LoRa --- SX1262 868 MHz (Europe)**

  ----------------------------------- -------------------------- --------------- ---------- ------------------------------------------------------------------------------------------------------------
  **Module**                          **Interface**              **Puissance**   **Prix**   **Lien**

  Ebyte E22-868T22S (⭐ RECOMMANDÉ)   SPI + UART, IPEX U.FL      22 dBm, 5 km    8--12 €    🔗 [[AliExpress E22-900T22S]{.underline}](https://www.aliexpress.com/i/4000548865041.html)

  Ebyte E22-900M22S                   SPI, SMD sans connecteur   22 dBm, 5 km    7--10 €    🔗 [[Ebyte Official Store]{.underline}](https://ebyteiot.com/collections/lora-module/sx1262)

  Waveshare SX1262 HAT                SPI, bien documenté        22 dBm          10--15 €   🔗 [[Recherche AliExpress]{.underline}](https://www.aliexpress.com/w/wholesale-Waveshare-SX1262-LoRa.html)
  ----------------------------------- -------------------------- --------------- ---------- ------------------------------------------------------------------------------------------------------------

> *✅ Librairie : jgromes/RadioLib ≥ 6.6.0 --- LoRaWAN Class A, persistance session NVS, SF/BW/CR configurables. Éviter les modules SX1276 (génération précédente, moins sensibles).*
>
> *💡 Antenne 868 MHz + câble IPEX→SMA :*

🔗 [[Antenne 868 MHz AliExpress]{.underline}](https://www.aliexpress.com/w/wholesale-antenna-868MHz-LoRa-IPEX.html) (2--5 €)

**A.3 RTC --- DS3231**

  ------------------------------------- --------------------------------------------------- ---------- ---------------------------------------------------------------------------------------------------------------
  **Module**                            **Caractéristiques**                                **Prix**   **Lien**

  GY-DS3231 + AT24C32 (⭐ RECOMMANDÉ)   I²C (0x68 + 0x57), AT24C32 EEPROM 32 Kbit, ±2 ppm   1--3 €     🔗 [[AliExpress DS3231 AT24C32]{.underline}](https://www.aliexpress.com/item/2037934408.html)

  Adafruit DS3231 #3013                 Sans EEPROM, STEMMA QT, qualité certifiée           10--15 €   🔗 [[Recherche AliExpress Adafruit]{.underline}](https://www.aliexpress.com/w/wholesale-Adafruit-DS3231.html)
  ------------------------------------- --------------------------------------------------- ---------- ---------------------------------------------------------------------------------------------------------------

> *⚠ Vérifier que la résistance de charge R5 est absente sur les modules génériques si pile CR2032 non rechargeable (risque de détériorer la pile). Librairie : adafruit/RTClib ≥ 2.1.1.*

**A.4 Amplificateur cellule de charge --- HX711**

  -------------------------------- --------------------------------------- ---------- ---------------------------------------------------------------------------------------------------------
  **Module**                       **Caractéristiques**                    **Prix**   **Lien**

  HX711 générique (⭐ SUFFISANT)   24 bits, 2 fils CLK/DAT, gain 64/128×   0,5--2 €   🔗 [[AliExpress HX711 module]{.underline}](https://www.aliexpress.com/item/32661679886.html)

  SparkFun HX711 #SEN-13879        Meilleure qualité PCB, blindage         10--15 €   🔗 [[Recherche AliExpress SparkFun]{.underline}](https://www.aliexpress.com/item/3256804378615060.html)
  -------------------------------- --------------------------------------- ---------- ---------------------------------------------------------------------------------------------------------

> *⚠ Module générique à 0,5 € suffisant. Attention aux clones avec oscillateur 40 Hz au lieu de 10 Hz --- si lectures instables, tester en forçant la fréquence à 10 Hz dans le code. Librairie : bogde/HX711 ≥ 0.7.5.*

**A.5 Cellule de charge --- Capacité 200 kg (Master)**

  ------------------------------- ------------------------------------------- ----------------- ---------- -----------------------------------------------------------------------------------------------------------------
  **Référence**                   **Type**                                    **Sensibilité**   **Prix**   **Lien**

  CZL601 200 kg (⭐ RECOMMANDÉ)   Poutre parallèle, aluminium anodisé, IP65   2 mV/V, 4 fils    15--30 €   🔗 [[AliExpress CZL601 200kg]{.underline}](https://www.aliexpress.com/item/1005006343424988.html)

  CZL601 100 kg                   Idem, capacité réduite                      2 mV/V, 4 fils    10--20 €   🔗 [[AliExpress CZL601 100kg]{.underline}](https://www.aliexpress.com/item/1005003199094753.html)

  Bosche H30A 200 kg              Colonne simple appui, précision ±0,02%      2 mV/V            30--60 €   🔗 [[Recherche AliExpress H30A]{.underline}](https://www.aliexpress.com/w/wholesale-Bosche-H30A-load-cell.html)
  ------------------------------- ------------------------------------------- ----------------- ---------- -----------------------------------------------------------------------------------------------------------------

> *💡 La CZL601 est une cellule parallèle à point central --- un seul point d\'appui, pas de problème d\'équilibrage. Convient parfaitement pour plateau de ruche 300×350 mm. Protection IP65 native.*

**A.6 Cellule de charge --- Capacité 50 kg (Slaves)**

  --------------------------------- ----------------------------------------- --------------- ------------------------------------------------------------------------------------------------------------------------
  **Référence**                     **Type**                                  **Prix**        **Lien**

  CZL601 50 kg (⭐ RECOMMANDÉ)      Poutre parallèle aluminium IP65, 2 mV/V   8--15 €         🔗 [[AliExpress CZL601 50kg]{.underline}](https://www.aliexpress.com/w/wholesale-CZL601-50kg-load-cell.html)

  4× barres 50 kg pont Wheatstone   Câblage complexe, plateau requis          12--32 € (×4)   🔗 [[Recherche AliExpress load cell 50kg]{.underline}](https://www.aliexpress.com/w/wholesale-50kg-load-cell-bar.html)
  --------------------------------- ----------------------------------------- --------------- ------------------------------------------------------------------------------------------------------------------------

**A.7 Capteur T°/Humidité/Pression --- BME280 (Master)**

  --------------------------------- ------------------------------------------------------------------ ---------- ----------------------------------------------------------------------------------------------------------------------
  **Module**                        **Caractéristiques**                                               **Prix**   **Lien**

  GY-BME280 3,3 V (⭐ RECOMMANDÉ)   I²C (0x76/0x77) + SPI, T° −40/+85°C, HR 0--100%, P 300--1100 hPa   2--4 €     🔗 [[AliExpress GY-BME280]{.underline}](https://www.aliexpress.com/item/32772903134.html)

  Adafruit BME280 #2652             STEMMA QT, qualité garantie                                        10--15 €   🔗 [[Recherche AliExpress Adafruit BME280]{.underline}](https://www.aliexpress.com/w/wholesale-Adafruit-BME280.html)
  --------------------------------- ------------------------------------------------------------------ ---------- ----------------------------------------------------------------------------------------------------------------------

> *⚠ Adresse par défaut 0x76 (SDO à GND). Mettre SDO à VCC pour obtenir 0x77 si conflit. Préférer la version 3,3 V directe I²C sans régulateur intégré. Librairie : adafruit/Adafruit_BME280_Library.*

**A.8 Capteur Luminosité --- BH1750 (Master)**

  ------------------------------ ------------------------------------------- ---------- ---------------------------------------------------------------------------------------------
  **Module**                     **Plage**                                   **Prix**   **Lien**

  GY-30 BH1750 (⭐ RECOMMANDÉ)   1--65 535 lux, 16 bits, I²C (0x23/0x5C)     1--3 €     🔗 [[AliExpress GY-30 BH1750]{.underline}](https://www.aliexpress.com/item/1872367675.html)

  GY-302 BH1750                  Pull-up embarqués, légèrement plus fiable   1--3 €     🔗 [[AliExpress GY-302]{.underline}](https://www.aliexpress.com/item/32765542002.html)
  ------------------------------ ------------------------------------------- ---------- ---------------------------------------------------------------------------------------------

> *💡 Adresse 0x23 par défaut, 0x5C si pin ADDR à VCC. Librairie : claws/BH1750 --- readLightLevel(), compatible ESP32 natif. Mode one-shot pour économiser l\'énergie entre mesures.*

**A.9 Surveillance Batterie / Courant --- INA219**

  ------------------------------ ----------------------------------------------- ---------- -----------------------------------------------------------------------------------------------
  **Module**                     **Usage**                                       **Prix**   **Lien**

  INA219 générique (⭐ MASTER)   I²C (0x40--0x43), 0--26 V DC, ±3,2 A, 12 bits   1--3 €     🔗 [[AliExpress INA219 GY-219]{.underline}](https://www.aliexpress.com/item/32375566240.html)

  Diviseur résistif ADC          100 kΩ / 47 kΩ → ADC ESP32 (suffisant slaves)   \< 0,5 €   *Composants discrets --- pas de lien nécessaire*
  ------------------------------ ----------------------------------------------- ---------- -----------------------------------------------------------------------------------------------

> *💡 INA219 recommandé uniquement pour le master (mesure courant panneau solaire + tension précise). Pour les 3 slaves, le diviseur résistif avec ADC ESP32-S3 + correction non-linéarité esp_adc_cal est suffisant et moins coûteux. Librairie : adafruit/Adafruit_INA219.*

**A.10 Chargeur Solaire MPPT --- CN3791**

  ------------------------------------------ --------------------------------- ----------------- ---------- ------------------------------------------------------------------------------------------------------------------------------------
  **Module**                                 **Tension entrée**                **Courant max**   **Prix**   **Lien**

  CN3791 MPPT 6 V (petits panneaux 1--3 W)   6 V nominal                       2 A               2--4 €     🔗 [[AliExpress CN3791 6V]{.underline}](https://www.aliexpress.com/item/4000309853682.html)

  CN3791 MPPT 12 V (⭐ STANDARD)             12 V nominal                      2 A               2--4 €     🔗 [[AliExpress CN3791 module]{.underline}](https://www.aliexpress.com/item/1005003482888458.html)

  Waveshare Solar Manager                    6--24 V, USB-C, indicateurs LED   2 A               12--18 €   🔗 [[Recherche AliExpress Waveshare Solar]{.underline}](https://www.aliexpress.com/w/wholesale-Waveshare-Solar-Power-Manager.html)
  ------------------------------------------ --------------------------------- ----------------- ---------- ------------------------------------------------------------------------------------------------------------------------------------

> *💡 CN3791 6 V pour panneaux 1--3 W / 6 V. CN3791 12 V pour panneaux standard 5--10 W / 12 V. Broches CHRG (en charge) et DONE (charge terminée) lisibles en GPIO --- aucune librairie nécessaire. Charge CC/CV, protection surcharge intégrée.*

**A.11 Afficheur OLED --- SSD1309 2,42\" 128×64 (Master)**

  ------------------------------------- --------------------------------- ----------------------- ---------- -----------------------------------------------------------------------------------------------------------------
  **Module**                            **Résolution**                    **Interface**           **Prix**   **Lien**

  OLED 2,42\" SSD1309 (⭐ RECOMMANDÉ)   128×64 pixels, blanc              I²C + SPI (7 broches)   10--15 €   🔗 [[AliExpress SSD1309 2.42\"]{.underline}](https://www.aliexpress.com/item/32911459164.html)

  OLED 2,42\" SSD1309 (variante)        128×64 pixels, blanc/bleu         I²C (4 broches)         10--15 €   🔗 [[AliExpress SSD1309 128x64]{.underline}](https://www.aliexpress.com/item/1005003091769556.html)

  OLED 0,96\" SSD1306 (référence)       128×64 pixels --- ancien format   I²C (4 broches)         1--3 €     🔗 [[AliExpress SSD1306 0.96\"]{.underline}](https://www.aliexpress.com/w/wholesale-0.96-OLED-SSD1306-I2C.html)
  ------------------------------------- --------------------------------- ----------------------- ---------- -----------------------------------------------------------------------------------------------------------------

> *✅ Le SSD1309 est compatible avec les librairies SSD1306 (même jeu d\'instructions OLED). Utiliser olikraus/U8g2 avec le driver u8g2_SSD1309_128X64_NONAME_F\_\... pour ESP32-S3.*
>
> *⚠ Vérifier la pinout : certains modules ont GND/VCC inversés (GND-VCC-SCL-SDA vs VCC-GND-SCL-SDA). Angle de vision \> 160°, contraste \> 10 000:1, consommation 42 mA (full brightness), \< 10 µA (sleep mode).*

**A.12 Synthèse --- Adresses I²C sur le Bus (Master)**

  ---------------------- -------------------- ------------------------------- ---------------
  **Module**             **Adresse défaut**   **Alternative**                 **Conflit ?**

  DS3231 RTC             0x68                 Fixe (non configurable)         Non ✅

  AT24C32 EEPROM         0x57                 0x50--0x56 (jumpers A0/A1/A2)   Non ✅

  BME280                 0x76                 0x77 (SDO → VCC)                Non ✅

  BH1750                 0x23                 0x5C (ADDR → VCC)               Non ✅

  INA219                 0x40                 0x41/0x42/0x43 (A0/A1)          Non ✅

  OLED SSD1309           0x3C                 0x3D (jumper)                   Non ✅
  ---------------------- -------------------- ------------------------------- ---------------

> *✅ 6 devices sur le bus --- aucun conflit avec la configuration par défaut. Fréquence recommandée : 400 kHz (Fast Mode) --- compatible tous modules. ESP32-S3 supporte jusqu\'à 127 adresses I²C.*

**A.13 Boîtier, Étanchéité et Visserie**

**A.13.1 Boîtier étanche IP65/IP67**

  ----------------------------------------------- ----------------------------------------------- -------- ---------- -------------------------------------------------------------------------------------------------------------------
  **Référence**                                   **Dimensions**                                  **IP**   **Prix**   **Lien**

  Boîtier ABS étanche 100×68×50 mm                Compact --- slaves (électronique seule)         IP67     2--4 €     🔗 [[AliExpress boîtier IP67 ABS]{.underline}](https://fr.aliexpress.com/item/1005004153488855.html)

  Boîtier ABS étanche 150×100×70 mm (⭐ MASTER)   Master + batterie 18650                         IP67     4--8 €     🔗 [[AliExpress boîtier IP67 150x100]{.underline}](https://fr.aliexpress.com/item/4000601561680.html)

  Boîtier ABS étanche 200×150×130 mm              Grand format --- master + panneau sur flanc     IP67     8--15 €    🔗 [[AliExpress boîtier IP67 200x150]{.underline}](https://fr.aliexpress.com/item/32794767366.html)

  Boîtier ABS étanche (couvercle transparent)     Visibilité afficheur OLED depuis l\'extérieur   IP65     5--10 €    🔗 [[AliExpress boîtier couvercle transparent]{.underline}](https://fr.aliexpress.com/item/1005005859929902.html)
  ----------------------------------------------- ----------------------------------------------- -------- ---------- -------------------------------------------------------------------------------------------------------------------

> *💡 Choisir boîtier avec plaque de montage intérieure pour fixer PCB/fond de panier. Vérifier la cote de perçage avant commande (passe-câble, fixations).*

**A.13.2 Presse-étoupes et étanchéité câble**

  ------------------------------------------------- --------------------------------------- ----------------- -------------------- -----------------------------------------------------------------------------------------------------------
  **Référence**                                     **Usage**                               **Câble admis**   **Prix**             **Lien**

  Presse-étoupe PG7 nylon noir (⭐ CÂBLE CAPTEUR)   Câbles 4 fils capteur (HX711, DS3231)   Ø 3,5--7 mm       0,10--0,20 € / pce   🔗 [[AliExpress PG7 lot 50 pcs]{.underline}](https://fr.aliexpress.com/item/32828234650.html)

  Presse-étoupe PG9 nylon noir (⭐ ALIMENTATION)    Câble solaire, alimentation             Ø 4--8 mm         0,15--0,25 € / pce   🔗 [[AliExpress PG9 câble gland]{.underline}](https://www.aliexpress.com/item/1005001727362086.html)

  Presse-étoupe PG11                                Câbles USB ou faisceaux multi-fils      Ø 5--10 mm        0,20--0,30 € / pce   🔗 [[AliExpress PG7-PG11 assortiment]{.underline}](https://www.aliexpress.com/item/1005001727362086.html)

  Lot assortiment PG7→PG16 50 pcs + boîte           Kit complet pour prototype              Ø 3,5--16 mm      3--6 € le lot        🔗 [[AliExpress lot presse-étoupes 50 pcs]{.underline}](https://fr.aliexpress.com/item/32828234650.html)
  ------------------------------------------------- --------------------------------------- ----------------- -------------------- -----------------------------------------------------------------------------------------------------------

> *⚠ Utiliser du silicone RTV ou un joint torique sous les couvercles de boîtiers vissés. Pour l\'afficheur OLED, découper une fenêtre + colle silicone transparent sur plaque PMMA 3 mm.*

**A.13.3 Visserie inox M3/M4**

  ----------------------------------------------------- ------------------------------------------- -------------- ---------- ---------------------------------------------------------------------------------------------------------------------------
  **Référence**                                         **Usage**                                   **Quantité**   **Prix**   **Lien**

  Vis CHC M3 inox 304 assortiment 5--20 mm              Fixation PCB, distanciers, fond de panier   100 pcs        3--6 €     🔗 [[AliExpress vis M3 inox CHC]{.underline}](https://fr.aliexpress.com/item/32852995477.html)

  Vis CHC M4 inox 304 assortiment 6--25 mm              Fixation boîtier, cornières, platines       50 pcs         3--5 €     🔗 [[AliExpress vis M4 inox 304]{.underline}](https://fr.aliexpress.com/item/32951316307.html)

  Écrous M3 + M4 inox + rondelles inox assorties        Contre-écrous, assemblages boîtier          100 pcs        3--5 €     🔗 [[AliExpress écrous rondelles inox assortiment]{.underline}](https://fr.aliexpress.com/w/wholesale-visserie-inox.html)

  Entretoise laiton M3 assortiment 5--20 mm (kit PCB)   Fixation ESP32 + modules dans boîtier       50 pcs         2--4 €     🔗 [[AliExpress entretoises laiton M3]{.underline}](https://www.aliexpress.com/w/wholesale-brass-standoff-M3-PCB.html)
  ----------------------------------------------------- ------------------------------------------- -------------- ---------- ---------------------------------------------------------------------------------------------------------------------------

> *💡 Préférer l\'inox 304 (A2) ou 316 (A4) pour résistance à l\'humidité extérieure. Éviter la visserie zinguée qui rouille rapidement en environnement apicole (propolis acide, humidité).*

**A.14 Récapitulatif Budget Approvisionnement (prototype)**

**Estimation 1 master + 3 slaves --- modules génériques AliExpress :**

  ----------------------------------------------- ------------ ---------------- ---------------- -------------------------------------------------------------------------------------------------------------
  **Composant**                                   **Qté**      **Prix unit.**   **Sous-total**   **Lien AliExpress**

  ESP32-S3-DevKitC-1 N16R8                        4 (1M+3S)    8--12 €          32--48 €         🔗 [[ESP32-S3 N16R8]{.underline}](https://www.aliexpress.com/w/wholesale-ESP32-S3-DevKitC-1-N16R8.html)

  Ebyte E22-868T22S (SX1262)                      1            8--12 €          8--12 €          🔗 [[E22-900T22S]{.underline}](https://www.aliexpress.com/i/4000548865041.html)

  Antenne 868 MHz + câble IPEX→SMA                1            2--5 €           2--5 €           🔗 [[Antenne 868 MHz]{.underline}](https://www.aliexpress.com/w/wholesale-antenna-868MHz-LoRa-IPEX.html)

  DS3231 + AT24C32 module                         4            1--2 €           4--8 €           🔗 [[DS3231 AT24C32]{.underline}](https://www.aliexpress.com/item/2037934408.html)

  HX711 module breakout                           4            0,5--1 €         2--4 €           🔗 [[HX711]{.underline}](https://www.aliexpress.com/item/32661679886.html)

  Cellule CZL601 200 kg (master)                  1            15--30 €         15--30 €         🔗 [[CZL601 200kg]{.underline}](https://www.aliexpress.com/item/1005006343424988.html)

  Cellule CZL601 50 kg (slaves)                   3            8--15 €          24--45 €         🔗 [[CZL601 50kg]{.underline}](https://www.aliexpress.com/w/wholesale-CZL601-50kg-load-cell.html)

  BME280 module I²C                               1 (master)   2--4 €           2--4 €           🔗 [[GY-BME280]{.underline}](https://www.aliexpress.com/item/32772903134.html)

  BH1750 GY-30 module                             1 (master)   1--2 €           1--2 €           🔗 [[GY-30 BH1750]{.underline}](https://www.aliexpress.com/item/1872367675.html)

  INA219 breakout                                 4            1--2 €           4--8 €           🔗 [[INA219 GY-219]{.underline}](https://www.aliexpress.com/item/32375566240.html)

  CN3791 MPPT chargeur                            4            2--3 €           8--12 €          🔗 [[CN3791 MPPT]{.underline}](https://www.aliexpress.com/item/1005003482888458.html)

  OLED 2,42\" SSD1309 (master)                    1            10--15 €         10--15 €         🔗 [[SSD1309 2.42\"]{.underline}](https://www.aliexpress.com/item/32911459164.html)

  Batterie Li-Ion 18650 2 500 mAh                 4--8         3--6 €           12--48 €         🔗 [[18650 Li-Ion batterie]{.underline}](https://www.aliexpress.com/w/wholesale-18650-battery-2500mAh.html)

  Panneau solaire 6 V 2--5 W                      4            3--8 €           12--32 €         🔗 [[Panneau solaire 6V 5W]{.underline}](https://www.aliexpress.com/w/wholesale-solar-panel-6V-5W.html)

  Boîtiers étanches IP67 ABS                      4            4--10 €          16--40 €         🔗 [[Boîtier ABS IP67]{.underline}](https://fr.aliexpress.com/item/4000601561680.html)

  Presse-étoupes PG7/PG9 + visserie inox          ---          ---              10--20 €         🔗 [[Presse-étoupes + vis inox]{.underline}](https://fr.aliexpress.com/item/32828234650.html)

  Divers (fils, connecteurs JST, entrées câble)   ---          ---              15--25 €         *Divers --- commander selon besoin*
  ----------------------------------------------- ------------ ---------------- ---------------- -------------------------------------------------------------------------------------------------------------

**TOTAL ESTIMATIF : 178--408 €** *(électronique + mécanique, hors panneaux et boîtiers haut de gamme)*

> *⚠ Délais AliExpress : 3--6 semaines depuis la Chine. Prévoir les composants critiques (ESP32-S3, Ebyte E22) également sur Mouser/Farnell pour disponibilité rapide en développement. Les prix sont indicatifs (janvier 2025).*
>
> *💡 Pour les achats en volume (\>10 unités), négocier avec les vendeurs AliExpress ou commander directement sur Alibaba.com --- réductions 20--40% possibles.*
