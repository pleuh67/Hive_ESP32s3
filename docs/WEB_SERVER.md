# Documentation — Serveur web (web_server.cpp + index.html)

## 1. Architecture générale

```
Navigateur (PC/smartphone)
        |
        |  HTTP/TCP  (WiFi LAN, port 80)
        |
  ESP32-S3 Master
  ┌─────────────────────────────────────────────────────┐
  │  WiFi STA  →  IP locale (ex: 192.168.1.42)         │
  │                                                      │
  │  ESPAsyncWebServer (port 80)                        │
  │  ┌──────────────────┐   ┌────────────────────────┐  │
  │  │ Routes API       │   │ Fichiers statiques     │  │
  │  │ /api/data  GET   │   │ LittleFS /             │  │
  │  │ /api/lora/join   │   │  └── index.html        │  │
  │  │ /api/cal/tare    │   │       (HTML+CSS+JS)    │  │
  │  │ ...              │   └────────────────────────┘  │
  │  └──────────────────┘                               │
  └─────────────────────────────────────────────────────┘
```

**Trois composants distincts :**

| Composant | Rôle |
|-----------|------|
| `WiFi` (lib Arduino) | Connexion au réseau local (mode Station = client WiFi) |
| `ESPAsyncWebServer` | Répond aux requêtes HTTP sans bloquer le CPU |
| `LittleFS` | Système de fichiers dans la flash — stocke index.html |

---

## 2. Le mode "Station" WiFi (STA)

```cpp
WiFi.mode(WIFI_STA);          // Mode client (l'ESP se connecte au routeur)
WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
```

**STA vs AP :**
- **STA** (Station) : l'ESP rejoint un réseau WiFi existant. Il obtient une IP du routeur
  (ex: `192.168.1.42`). Tout appareil du réseau peut y accéder.
- **AP** (Access Point) : l'ESP crée son propre réseau. Pas utilisé ici.

**L'IP est affichée au démarrage :**
```
[WiFi] Connecte : 192.168.1.42
```
C'est cette IP qu'on tape dans le navigateur : `http://192.168.1.42`

**Timeout 10 secondes :**
```cpp
while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000UL)
{
  delay(200);  // Attente active, acceptable car c'est dans setup()
}
```
Si le routeur ne répond pas en 10s, `webServerInit()` retourne `false` et le serveur
est désactivé. Le reste du firmware continue normalement.

---

## 3. LittleFS — le système de fichiers dans la flash

La flash du master (16 Mo) est partitionnée : une partie pour le firmware, une partie
pour LittleFS (un petit système de fichiers).

```
Flash 16 Mo
├── Firmware (firmware.bin)   ~1.2 Mo
└── LittleFS partition        reste
    └── index.html            ~8 Ko
```

**Déploiement de index.html :**
```bash
pio run -e master -t uploadfs
```
Cette commande lit `data-master/`, crée une image LittleFS, et la flashe
séparément du firmware. À faire après chaque modification de index.html.

**Montage au démarrage :**
```cpp
LittleFS.begin(false)  // false = ne pas reformater si montage OK
LittleFS.begin(true)   // true = reformater si échec (perd les fichiers !)
```

**Servir index.html :**
```cpp
s_server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
// "GET /"         → renvoie index.html
// "GET /style.css" → renvoie style.css si présent dans LittleFS
```

---

## 4. ESPAsyncWebServer — pourquoi "async" ?

### Serveur synchrone classique (pas utilisé ici)
```
Requête arrive → traitement → réponse → BLOQUE pendant tout ce temps
```
Pendant qu'on traite une requête, rien d'autre ne peut se passer.

### Serveur asynchrone (notre cas)
```
Requête arrive → callback déclenché → réponse immédiate → retour à loop()
```
Le traitement de la requête est délégué à une **tâche FreeRTOS** dédiée (AsyncTCP).
Le callback (handler) s'exécute dans cette tâche, pas dans `loop()`.

**Conséquence critique :** les handlers **ne doivent pas bloquer** (pas de delay,
pas de I2C long, pas de LoRa TX). Si un handler bloque, il bloque la tâche réseau
et le WiFi peut devenir instable.

---

## 5. Le pattern "deferred action" (actions différées)

Certaines opérations sont bloquantes :
- `hx711Tare()` : ~250 ms (10 lectures HX711)
- `hx711CalcScale()` : ~250 ms (10 lectures HX711)
- `loraJoin()` : jusqu'à 30 s (échange LoRaWAN OTAA)
- `loraSendPayload()` : 2-5 s (TX + fenêtres RX)

On **ne peut pas** les appeler directement dans un handler async.

**Solution : le drapeau (flag) booléen**

```
Navigateur          Handler async           loop() (tâche principale)
    |                     |                        |
    |-- POST /api/cal/tare ->|                      |
    |                     |  s_tareRequested=true   |
    |<-- 202 "demande" ---|                        |
    |                                   |           |
    |                          webServerProcess()   |
    |                                   |-- s_tareRequested==true
    |                                   |-- hx711Tare()  (~250ms, OK ici)
    |                                   |-- E24C32saveConfig()
    |                                   |-- s_tareRequested=false
```

```cpp
// Dans le handler (tâche async — immédiat) :
static void handleCalTare(AsyncWebServerRequest* request)
{
  s_tareRequested = true;           // Juste positionner le flag
  request->send(202, ...);          // Répondre immédiatement
}

// Dans loop() via webServerProcess() :
if (s_tareRequested)
{
  s_tareRequested = false;
  hx711Tare();          // Exécuté dans loop() → peut bloquer sans risque
  E24C32saveConfig();
}
```

**Code HTTP 202 :** "Accepted" — signifie "requête reçue, traitement en cours".
Différent de 200 "OK" qui signifie "traitement terminé".

**Exceptions — actions directes dans le handler :**
Les ajustements VBat/VSol n'utilisent pas de flag car ils sont non-bloquants :
```cpp
// Calcul arithmétique pur + 1 appel I2C rapide (~5ms) : acceptable en async
config.materiel.VBatScale = config.materiel.VBatScale * ref_v / measured;
E24C32saveConfig();  // ~5ms avec EEPROM présente
```

---

## 6. Construction JSON manuelle (sans ArduinoJson)

Contrainte du projet : pas d'allocation dynamique → pas d'ArduinoJson.
On construit la chaîne JSON à la main avec `snprintf` dans un buffer statique.

### Pourquoi `static char buf[]` ?

```cpp
static void handleApiData(AsyncWebServerRequest* request)
{
  static char buf[640];   // ← static : alloué une seule fois, reste en RAM
  buildJsonData(buf, sizeof(buf));
  request->send(200, "application/json", buf);
  // request->send() copie buf immédiatement → pas de problème de durée de vie
}
```

**`static` local** = alloué dans la section BSS (données statiques), pas sur la pile.
Sans `static`, `buf` serait sur la pile de la tâche AsyncTCP — risque de stack overflow
avec un buffer de 640 octets.

### Construction incrémentale du JSON (tableau slaves)

```cpp
int pos = snprintf(buf, bufLen, "{\"ts\":%lu,\"master\":{...},\"slaves\":[", ...);
//                              ^-- pos pointe maintenant après le '['

for (uint8_t i = 0; i < NUM_SLAVES; i++)
{
  if (i > 0) { buf[pos++] = ','; }   // Séparateur entre éléments

  pos += snprintf(buf + pos,         // buf+pos = écrire à la suite
                  bufLen - pos,      // espace restant dans le buffer
                  "{\"id\":%u,...}", i, ...);
}

snprintf(buf + pos, bufLen - pos, "],\"lora_ok\":...}");  // Fermeture
```

**Pourquoi `bufLen - pos` comme taille ?**
`snprintf` écrit au maximum N-1 caractères + le null terminal. En passant l'espace
restant, on évite le dépassement de buffer même si les données sont volumineuses.

### Tailles des buffers — calcul

| Route | Buffer | Justification |
|-------|--------|---------------|
| `/api/data` | 640 | Master (~200) + 3 slaves (~80×3) + LoRa (~50) + marge |
| `/api/status` | 300 | Version + IP + RSSI + boot + uptime (~150) + marge |
| `/api/config/lora` | 256 | SF + 2×16 hex EUI + statut (~120) + marge |
| `/api/config/cal` | 300 | 7 floats + clés (~200) + marge |
| `/api/hx711/raw` | 80 | 2 floats (~40) + marge |
| Messages erreur/ok | 64-96 | Courts messages texte |

---

## 7. Les routes HTTP — référence complète

### Convention : GET vs POST

| Méthode | Usage | Paramètres |
|---------|-------|------------|
| **GET** | Lire des données, sans effet de bord | Dans l'URL : `?key=val` |
| **POST** | Modifier/déclencher une action | Dans l'URL aussi (ici), ou dans le body |

Dans ce projet, les POST passent leurs paramètres en **query string** (dans l'URL)
même pour les POST, par simplicité. C'est inhabituel mais fonctionnel.

### Extraction des paramètres côté C++

```cpp
// Vérifier qu'un paramètre existe
if (!request->hasParam("sf"))
{
  request->send(400, "application/json", "{\"ok\":false,...}");
  return;
}

// Lire sa valeur (retourne un String Arduino, on prend .c_str() immédiatement)
const char* sfStr = request->getParam("sf")->value().c_str();
uint8_t sf = (uint8_t)atoi(sfStr);   // Conversion string → entier
float ref  = atof(sfStr);            // Conversion string → flottant
```

**Attention :** `.c_str()` retourne un pointeur valide seulement le temps de vie
de l'objet `AsyncWebParameter`. On le lit immédiatement, on ne le stocke pas.

### Codes de réponse HTTP utilisés

| Code | Signification | Quand |
|------|---------------|-------|
| **200** | OK — traitement terminé | Action directe réussie (ex: save SF) |
| **202** | Accepted — traitement en cours | Action différée via flag (tare, join...) |
| **400** | Bad Request — erreur client | Paramètre manquant ou invalide |
| **404** | Not Found | Route inexistante |

---

## 8. CORS — pourquoi cet en-tête ?

```cpp
DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
```

**CORS** (Cross-Origin Resource Sharing) : mécanisme de sécurité du navigateur.
Par défaut, un navigateur refuse qu'une page web d'un domaine appelle une API
d'un autre domaine. Ici, index.html est servi par `192.168.1.42` et appelle
`192.168.1.42/api/...` — même domaine, pas de problème CORS normalement.

Cet en-tête est utile si on teste l'API depuis un outil externe (Postman, curl,
ou un autre navigateur avec une page de développement locale).

`"*"` = n'importe quelle origine peut faire des requêtes → acceptable pour un
réseau local fermé. À restreindre si exposition sur Internet.

---

## 9. Le JavaScript — concepts utilisés

### fetch() — appel HTTP depuis le navigateur

```javascript
fetch('/api/data')                        // Envoie GET /api/data
  .then(function(r) { return r.json(); }) // Parse la réponse JSON
  .then(function(d) {                     // d = objet JS parsé
    // Utiliser d.master.temp, d.slaves[0].weight_g, etc.
  })
  .catch(function(e) {                    // Erreur réseau
    console.error(e);
  });
```

**Chaîne de `.then()`** : chaque `.then()` reçoit le résultat du précédent.
`r.json()` est lui-même une Promise — d'où le deuxième `.then()`.

**Pour les POST :**
```javascript
fetch('/api/lora/join', {method: 'POST'})
  .then(function(r) { return r.json(); })
  .then(function(d) { /* d.ok, d.message */ });
```

**Pas d'await/async** : on utilise la syntaxe `.then()` (ES5) pour une meilleure
compatibilité avec tous les navigateurs, y compris anciens.

### Manipulation du DOM

Le DOM (Document Object Model) est la représentation en mémoire de la page HTML.
JavaScript peut lire et modifier n'importe quel élément via son `id`.

```javascript
// Lire la valeur d'un input
var sf = document.getElementById('lora-sf').value;  // "9"

// Écrire dans un span
document.getElementById('lora-joined-status').textContent = 'Joint';

// Changer une classe CSS (active/inactive)
document.getElementById('tab-lora').classList.add('active');
document.getElementById('tab-data').classList.remove('active');

// Montrer/cacher un div
document.getElementById('pane-lora').style.display = '';       // visible
document.getElementById('pane-data').style.display = 'none';   // caché
```

### Timers JavaScript

```javascript
// setInterval : répète indéfiniment toutes les N ms
var timer = setInterval(function() {
  cd--;
  if (cd <= 0) refresh();
}, 1000);  // toutes les 1000 ms = 1s

// clearInterval : arrêter un timer
clearInterval(timer);

// setTimeout : exécuter UNE FOIS après N ms
setTimeout(function() {
  updateCalLive();   // Appelé 1200ms après la tare pour voir le résultat
}, 1200);
```

**Pourquoi le délai après tare ?**
```javascript
apiPost('/api/cal/tare', function(d) {
  if (d.ok) setTimeout(function() { updateCalLive(); loadCalConfig(); }, 1200);
});
```
Le 202 arrive immédiatement, mais `hx711Tare()` est exécutée dans `loop()`
lors du prochain passage (~quelques ms). On attend 1200ms pour être sûr que
la tare est effective ET que le prochain appel HX711 a eu lieu.

### La fonction `apiPost()` — helper générique

```javascript
function apiPost(url, cb) {
  fetch(url, {method:'POST'})
    .then(function(r) { return r.json(); })
    .then(function(d) {
      notify(d.message || (d.ok ? 'OK' : 'Erreur'), !d.ok);  // Affiche notification
      if (cb) cb(d);   // Appelle le callback si fourni (peut être null)
    })
    .catch(function(e) { notify('Erreur reseau: ' + e, true); });
}
```

Tous les POST répondent `{"ok": true/false, "message": "..."}` — ce helper
gère automatiquement l'affichage de la notification.

---

## 10. Navigation par onglets — mécanisme CSS + JS

### HTML : trois panes, un seul visible

```html
<div id="pane-data">          <!-- visible par défaut -->
<div id="pane-lora" style="display:none">   <!-- caché -->
<div id="pane-cal"  style="display:none">   <!-- caché -->
```

### CSS : l'onglet actif

```css
nav button { color:#666; border-bottom: 3px solid transparent; }
nav button.active { color:#e8a000; border-bottom-color:#e8a000; }
```
Le soulignement orange apparaît quand la classe `active` est présente.

### JS : showTab()

```javascript
function showTab(name) {
  var tabs = ['data', 'lora', 'cal'];

  // 1. Tout cacher, tout désactiver
  for (var i = 0; i < tabs.length; i++) {
    document.getElementById('pane-' + tabs[i]).style.display = 'none';
    document.getElementById('tab-'  + tabs[i]).classList.remove('active');
  }

  // 2. Montrer seulement le pane demandé
  document.getElementById('pane-' + name).style.display = '';
  document.getElementById('tab-'  + name).classList.add('active');

  // 3. Arrêter le polling si on quitte l'onglet calibration
  if (activeTab === 'cal' && name !== 'cal') stopCalLive();
  activeTab = name;

  // 4. Charger les données de l'onglet entrant
  if (name === 'lora') loadLoraConfig();
  if (name === 'cal') { loadCalConfig(); startCalLive(); }
}
```

---

## 11. Le polling live HX711 (onglet Calibration)

```javascript
function startCalLive() {
  updateCalLive();                          // Une lecture immédiate
  calLiveTimer = setInterval(updateCalLive, 2000);  // Puis toutes les 2s
}

function stopCalLive() {
  if (calLiveTimer) { clearInterval(calLiveTimer); calLiveTimer = null; }
}

function updateCalLive() {
  fetch('/api/hx711/raw')
    .then(function(r) { return r.json(); })
    .then(function(d) {
      document.getElementById('cal-live').textContent =
        (d.weight_g / 1000).toFixed(3) + ' kg';
    })
    .catch(function() {});  // Silencieux si hors ligne
}
```

**Côté ESP32 (`handleApiHx711Raw`) :**
```cpp
if (scale.is_ready())     // Non bloquant : vérifie si DOUT == LOW
{
  raw_val = (float)scale.read();   // Lit 1 échantillon (~12ms à 80Hz)
  weight_g = (raw_val - tare) / echelle;
}
// Sinon : retourne la dernière valeur calculée par loop()
```

`scale.is_ready()` retourne `true` quand le HX711 a une conversion disponible
(broche DOUT passe à LOW). `scale.read()` lit les 24 bits immédiatement.

---

## 12. Calibration VBat/VSol — la formule

```cpp
// Avant : config.materiel.VBatScale est le facteur de conversion ADC → Volts
// measured = ADC_raw * VBatScale → donne la tension mesurée

// Si le multimètre lit ref_v=3.85V mais le firmware affiche measured=3.72V :
//   → le facteur est sous-évalué de (3.85/3.72)
//   → nouveau facteur = ancien × (3.85 / 3.72)

config.materiel.VBatScale = config.materiel.VBatScale * ref_v / measured;
```

Autrement dit : `new_scale = old_scale × (valeur_réelle / valeur_mesurée)`.

La mesure `measured` vient de `HiveSensor_Data.Bat_Voltage` — la dernière valeur
calculée par le loop principal, pas une lecture fraîche. C'est suffisant pour
une calibration one-shot à la mise en service.

---

## 13. Sécurité — AppKey masqué

```html
<input type="password" id="lora-appkey" ...>
<!-- type=password : le navigateur masque les caractères saisis (••••) -->
```

```cpp
// En GET /api/config/lora : AppKey n'est PAS retourné
// (pas de champ appkey_hex dans buildJsonConfigLora)
// L'utilisateur peut saisir une nouvelle clé sans voir l'ancienne
```

**Limitation :** l'AppKey transite en clair dans l'URL lors du POST
(`/api/config/lora/keys?appkey=AABBCC...`). Acceptable pour un réseau local
fermé. Sur un réseau non sécurisé, il faudrait utiliser HTTPS (non supporté
nativement par ESPAsyncWebServer sans certificats).

---

## 14. Tester l'API depuis le terminal (curl)

```bash
# Lire les données capteurs
curl http://192.168.1.42/api/data

# Lire la config LoRa
curl http://192.168.1.42/api/config/lora

# Changer SF à 10
curl -X POST "http://192.168.1.42/api/config/lora/sf?sf=10"

# Déclencher une tare
curl -X POST http://192.168.1.42/api/cal/tare

# Calibrer avec 10.5 kg de référence
curl -X POST "http://192.168.1.42/api/cal/scale?ref_g=10500"

# Ajuster VBat (multimètre indique 3.85V)
curl -X POST "http://192.168.1.42/api/cal/vbat?ref_v=3.85"

# Ajuster VSol
curl -X POST "http://192.168.1.42/api/cal/vsol?ref_v=5.20"
```

---

## 15. Séquence d'initialisation complète

```
setup()
  │
  ├─ webServerInit()
  │   ├─ WiFi.begin(SSID, PWD)           bloquant jusqu'à 10s
  │   ├─ LittleFS.begin()                lecture partition flash
  │   ├─ s_server.on("/api/data", ...)   enregistrement des routes
  │   ├─ s_server.on(...)                (12 routes au total)
  │   ├─ s_server.serveStatic(...)       service de index.html
  │   └─ s_server.begin()                démarrage de la tâche AsyncTCP
  │
loop()
  │
  ├─ webServerProcess()   ← appelé à chaque tour de boucle
  │   ├─ si s_joinRequested  → loraJoin()
  │   ├─ si s_sendRequested  → loraSendPayload()
  │   ├─ si s_tareRequested  → hx711Tare() + saveConfig()
  │   └─ si s_calibRequested → hx711CalcScale() + saveConfig()
  │
  └─ ... (BLE, LoRa, capteurs, menus)
```

---

## 16. Flux complet d'une requête de calibration

Exemple : l'utilisateur clique "Tare à vide" dans le navigateur.

```
1. Navigateur
   doTare() → fetch('/api/cal/tare', {method:'POST'})

2. TCP/IP — paquet HTTP POST → ESP32

3. AsyncTCP task (FreeRTOS, priorité 10)
   handleCalTare() déclenché
   → s_tareRequested = true      (écriture atomique bool, thread-safe)
   → request->send(202, ...)     réponse immédiate

4. TCP/IP — paquet HTTP 202 → Navigateur

5. Navigateur
   .then(d) → notify("Tare demandee", false)  notification verte 4s
   setTimeout(updateCalLive + loadCalConfig, 1200)

6. loop() (FreeRTOS, priorité 1)
   webServerProcess()
   → s_tareRequested == true
   → s_tareRequested = false
   → hx711Tare()          10 lectures HX711 ≈ 250ms
   → E24C32saveConfig()   écriture EEPROM ≈ 5ms

7. t+1200ms : Navigateur
   updateCalLive() → GET /api/hx711/raw → poids ≈ 0.000 kg affiché
   loadCalConfig() → GET /api/config/cal → noload mis à jour
```
