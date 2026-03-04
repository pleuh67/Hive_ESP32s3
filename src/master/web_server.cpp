// web_server.cpp — Serveur web master ESP32-S3
// Phase 4 : WiFi STA + ESPAsyncWebServer + LittleFS
// Phase 5 : Multi-ecrans (Donnees / LoRa / Calibration)
//
// Architecture :
//   - WiFi STA, connexion au setup (timeout 10 s)
//   - ESPAsyncWebServer sur port 80 (async, non bloquant)
//   - LittleFS pour servir index.html
//   - JSON construit manuellement avec snprintf (pas d'ArduinoJson)
//   - Actions bloquantes differees via flags -> traitees dans loop()

#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include "web_server.h"
#include "lora_manager.h"
#include "config.h"
#include "credentials.h"
#include "types.h"
#include "common/hx711_manager.h"
#include "common/eeprom_manager.h"
#include "common/convert.h"

// ---------------------------------------------------------------------------
// Acces aux donnees globales (definies dans main.cpp)
// ---------------------------------------------------------------------------

extern HiveSensor_Data_t HiveSensor_Data;
extern SlaveReading_t    slaveReadings[NUM_SLAVES];
extern ConfigGenerale_t  config;

// bootCount est en RTC RAM dans power_manager
extern uint32_t bootCount;

// ---------------------------------------------------------------------------
// Instances statiques
// ---------------------------------------------------------------------------

static AsyncWebServer s_server(80);
static bool  s_joinRequested  = false; // flag : loraJoin() en attente
static bool  s_sendRequested  = false; // flag : loraSendPayload() en attente
static bool  s_tareRequested  = false; // flag : hx711Tare() en attente
static bool  s_calibRequested = false; // flag : hx711CalcScale() en attente
static float s_calibRefGrams  = 0.0f; // poids reference pour calibration

// ---------------------------------------------------------------------------
// Construction JSON (snprintf dans buffer statique)
// Le buffer est rempli et passe a request->send() qui le copie immediatement.
// ---------------------------------------------------------------------------

// Serialize les donnees capteurs master + slaves en JSON
// buf : destination (640 octets recommandes)
static void buildJsonData(char* buf, size_t bufLen)
{
  // Poids master en grammes (HX711Weight est en grammes)
  long weightG = (long)HiveSensor_Data.HX711Weight[0];

  int pos = snprintf(buf, bufLen,
    "{\"ts\":%lu,"
    "\"master\":{"
      "\"temp\":%.1f,"
      "\"hum\":%.1f,"
      "\"pres\":%.1f,"
      "\"lux\":%.0f,"
      "\"weight_g\":%ld,"
      "\"vbat\":%.2f,"
      "\"vsol\":%.2f,"
      "\"isol\":%.1f,"
      "\"boot\":%lu"
    "},"
    "\"slaves\":[",
    (unsigned long)millis() / 1000UL,
    HiveSensor_Data.DHT_Temp,
    HiveSensor_Data.DHT_Hum,
    HiveSensor_Data.Pressure,
    HiveSensor_Data.Brightness,
    weightG,
    HiveSensor_Data.Bat_Voltage,
    HiveSensor_Data.Solar_Voltage,
    HiveSensor_Data.SolarCurrent,
    (unsigned long)bootCount
  );

  for (uint8_t i = 0; i < NUM_SLAVES && pos < (int)bufLen - 80; i++)
  {
    if (i > 0) { buf[pos++] = ','; }

    if (slaveReadings[i].valid)
    {
      // weight_g = weight (0.01 kg) * 10 = grammes
      long slaveWeightG = (long)slaveReadings[i].weight * 10;
      // vbat_v = vbat_enc / 10.0 + 2.0 (encode dans ble_slave)
      float slaveVbat = slaveReadings[i].vbat / 10.0f + 2.0f;
      pos += snprintf(buf + pos, bufLen - (size_t)pos,
        "{\"id\":%u,\"valid\":true,\"weight_g\":%ld,\"vbat\":%.2f,\"ts\":%lu}",
        (unsigned)i, slaveWeightG, slaveVbat,
        (unsigned long)slaveReadings[i].timestamp
      );
    }
    else
    {
      pos += snprintf(buf + pos, bufLen - (size_t)pos,
        "{\"id\":%u,\"valid\":false}", (unsigned)i
      );
    }
  }

  char loraStatus[32];
  loraGetStatus(loraStatus, sizeof(loraStatus));
  snprintf(buf + pos, bufLen - (size_t)pos,
    "],\"lora_ok\":%s,\"lora_status\":\"%s\"}",
    loraIsJoined() ? "true" : "false",
    loraStatus
  );
}

// Serialize l'etat systeme en JSON
static void buildJsonStatus(char* buf, size_t bufLen)
{
  char ip[16]        = "0.0.0.0";
  char loraStatus[32];
  loraGetStatus(loraStatus, sizeof(loraStatus));

  if (WiFi.status() == WL_CONNECTED)
  {
    WiFi.localIP().toString().toCharArray(ip, sizeof(ip));
  }

  snprintf(buf, bufLen,
    "{\"version\":\"%s\","
    "\"ip\":\"%s\","
    "\"rssi\":%d,"
    "\"boot\":%lu,"
    "\"lora_joined\":%s,"
    "\"lora_status\":\"%s\","
    "\"uptime\":%lu}",
    VERSION,
    ip,
    (int)WiFi.RSSI(),
    (unsigned long)bootCount,
    loraIsJoined() ? "true" : "false",
    loraStatus,
    (unsigned long)millis() / 1000UL
  );
}

// Serialize la configuration LoRa en JSON (AppKey non retourne)
static void buildJsonConfigLora(char* buf, size_t bufLen)
{
  char deveui_hex[17] = "????????????????";
  char appeui_hex[17] = "????????????????";
  char loraStatus[32];
  loraGetStatus(loraStatus, sizeof(loraStatus));

  byteArrayToHexString(config.materiel.DevEUI,   8, deveui_hex, sizeof(deveui_hex));
  byteArrayToHexString(config.applicatif.AppEUI,  8, appeui_hex, sizeof(appeui_hex));

  snprintf(buf, bufLen,
    "{\"sf\":%u,"
    "\"deveui_hex\":\"%s\","
    "\"appeui_hex\":\"%s\","
    "\"joined\":%s,"
    "\"lora_status\":\"%s\"}",
    (unsigned)config.applicatif.SpreadingFactor,
    deveui_hex,
    appeui_hex,
    loraIsJoined() ? "true" : "false",
    loraStatus
  );
}

// Serialize les donnees de calibration en JSON
static void buildJsonConfigCal(char* buf, size_t bufLen)
{
  snprintf(buf, bufLen,
    "{\"noload\":%.0f,"
    "\"scaling\":%.4f,"
    "\"vbat_scale\":%.8f,"
    "\"vsol_scale\":%.8f,"
    "\"weight_g\":%.3f,"
    "\"vbat\":%.3f,"
    "\"vsol\":%.3f}",
    config.materiel.HX711NoloadValue_0,
    config.materiel.HX711Scaling_0,
    config.materiel.VBatScale,
    config.materiel.VSolScale,
    HiveSensor_Data.HX711Weight[0],
    HiveSensor_Data.Bat_Voltage,
    HiveSensor_Data.Solar_Voltage
  );
}

// ---------------------------------------------------------------------------
// Handlers de routes
// ---------------------------------------------------------------------------

static void handleApiData(AsyncWebServerRequest* request)
{
  static char buf[640];
  buildJsonData(buf, sizeof(buf));
  request->send(200, "application/json", buf);
}

static void handleApiStatus(AsyncWebServerRequest* request)
{
  static char buf[300];
  buildJsonStatus(buf, sizeof(buf));
  request->send(200, "application/json", buf);
}

// GET /api/config/lora — configuration LoRa (AppKey non retourne)
static void handleApiConfigLora(AsyncWebServerRequest* request)
{
  static char buf[256];
  buildJsonConfigLora(buf, sizeof(buf));
  request->send(200, "application/json", buf);
}

// POST /api/config/lora/sf?sf=9 — met a jour le Spreading Factor
static void handleApiConfigLoraSf(AsyncWebServerRequest* request)
{
  if (!request->hasParam("sf"))
  {
    request->send(400, "application/json",
      "{\"ok\":false,\"message\":\"Param sf manquant\"}");
    return;
  }

  uint8_t sf = (uint8_t)atoi(request->getParam("sf")->value().c_str());
  if (sf < 7 || sf > 12)
  {
    request->send(400, "application/json",
      "{\"ok\":false,\"message\":\"SF invalide (7-12)\"}");
    return;
  }

  config.applicatif.SpreadingFactor = sf;
  E24C32saveConfig();

  static char buf[64];
  snprintf(buf, sizeof(buf),
    "{\"ok\":true,\"message\":\"SF=%u sauvegarde\"}", (unsigned)sf);
  request->send(200, "application/json", buf);
}

// POST /api/config/lora/keys?appeui=XX&appkey=XX — met a jour AppEUI + AppKey
static void handleApiConfigLoraKeys(AsyncWebServerRequest* request)
{
  bool changed = false;

  if (request->hasParam("appeui"))
  {
    const char* s = request->getParam("appeui")->value().c_str();
    if (strlen(s) != 16)
    {
      request->send(400, "application/json",
        "{\"ok\":false,\"message\":\"AppEUI : 16 caracteres hex attendus\"}");
      return;
    }
    if (!hexStringToByteArray(s, config.applicatif.AppEUI, 8))
    {
      request->send(400, "application/json",
        "{\"ok\":false,\"message\":\"AppEUI : caracteres hex invalides\"}");
      return;
    }
    changed = true;
  }

  if (request->hasParam("appkey"))
  {
    const char* s = request->getParam("appkey")->value().c_str();
    if (strlen(s) != 32)
    {
      request->send(400, "application/json",
        "{\"ok\":false,\"message\":\"AppKey : 32 caracteres hex attendus\"}");
      return;
    }
    if (!hexStringToByteArray(s, config.applicatif.AppKey, 16))
    {
      request->send(400, "application/json",
        "{\"ok\":false,\"message\":\"AppKey : caracteres hex invalides\"}");
      return;
    }
    changed = true;
  }

  if (!changed)
  {
    request->send(400, "application/json",
      "{\"ok\":false,\"message\":\"Aucun parametre fourni\"}");
    return;
  }

  E24C32saveConfig();
  request->send(200, "application/json",
    "{\"ok\":true,\"message\":\"Cles LoRa sauvegardees\"}");
}

// POST /api/lora/join — loraJoin() est bloquant, on utilise un flag
static void handleLoraJoin(AsyncWebServerRequest* request)
{
  s_joinRequested = true;
  request->send(202, "application/json",
    "{\"ok\":true,\"message\":\"Join LoRa demande\"}");
}

// POST /api/lora/send — loraSendPayload() est bloquant, on utilise un flag
static void handleLoraSend(AsyncWebServerRequest* request)
{
  if (!loraIsJoined())
  {
    request->send(400, "application/json",
      "{\"ok\":false,\"message\":\"LoRa non joint — faire Join d abord\"}");
    return;
  }
  s_sendRequested = true;
  request->send(202, "application/json",
    "{\"ok\":true,\"message\":\"Envoi payload demande\"}");
}

// GET /api/config/cal — valeurs calibration courantes
static void handleApiConfigCal(AsyncWebServerRequest* request)
{
  static char buf[300];
  buildJsonConfigCal(buf, sizeof(buf));
  request->send(200, "application/json", buf);
}

// GET /api/hx711/raw — lecture brute en direct (pour calibration live)
// Lit 1 echantillon si HX711 pret (non bloquant si pas pret)
static void handleApiHx711Raw(AsyncWebServerRequest* request)
{
  static char buf[80];
  float raw_val  = 0.0f;
  float weight_g = HiveSensor_Data.HX711Weight[0]; // valeur precompilee

  if (scale.is_ready())
  {
    raw_val = (float)scale.read();
    float tare    = config.materiel.HX711NoloadValue_0;
    float echelle = config.materiel.HX711Scaling_0;
    if (echelle != 0.0f)
    {
      weight_g = (raw_val - tare) / echelle;
    }
  }

  snprintf(buf, sizeof(buf),
    "{\"raw\":%.0f,\"weight_g\":%.3f}",
    raw_val, weight_g);
  request->send(200, "application/json", buf);
}

// POST /api/cal/tare — pose la tare a vide (deferred : bloquant ~250 ms)
static void handleCalTare(AsyncWebServerRequest* request)
{
  s_tareRequested = true;
  request->send(202, "application/json",
    "{\"ok\":true,\"message\":\"Tare demandee\"}");
}

// POST /api/cal/scale?ref_g=10500 — calibre l'echelle (deferred)
static void handleCalScale(AsyncWebServerRequest* request)
{
  if (!request->hasParam("ref_g"))
  {
    request->send(400, "application/json",
      "{\"ok\":false,\"message\":\"Param ref_g manquant\"}");
    return;
  }

  float ref_g = atof(request->getParam("ref_g")->value().c_str());
  if (ref_g <= 0.0f)
  {
    request->send(400, "application/json",
      "{\"ok\":false,\"message\":\"ref_g doit etre > 0\"}");
    return;
  }

  s_calibRefGrams  = ref_g;
  s_calibRequested = true;
  request->send(202, "application/json",
    "{\"ok\":true,\"message\":\"Calibration demandee\"}");
}

// POST /api/cal/vbat?ref_v=3.85 — ajuste VBatScale par valeur multimetre
static void handleCalVbat(AsyncWebServerRequest* request)
{
  if (!request->hasParam("ref_v"))
  {
    request->send(400, "application/json",
      "{\"ok\":false,\"message\":\"Param ref_v manquant\"}");
    return;
  }

  float ref_v    = atof(request->getParam("ref_v")->value().c_str());
  float measured = HiveSensor_Data.Bat_Voltage;

  if (ref_v <= 0.0f || measured <= 0.1f)
  {
    request->send(400, "application/json",
      "{\"ok\":false,\"message\":\"Valeurs invalides (ref>0, mesure>0.1V)\"}");
    return;
  }

  config.materiel.VBatScale = config.materiel.VBatScale * ref_v / measured;
  E24C32saveConfig();

  static char buf[96];
  snprintf(buf, sizeof(buf),
    "{\"ok\":true,\"message\":\"VBatScale=%.8f\"}",
    config.materiel.VBatScale);
  request->send(200, "application/json", buf);
}

// POST /api/cal/vsol?ref_v=5.20 — ajuste VSolScale par valeur multimetre
static void handleCalVsol(AsyncWebServerRequest* request)
{
  if (!request->hasParam("ref_v"))
  {
    request->send(400, "application/json",
      "{\"ok\":false,\"message\":\"Param ref_v manquant\"}");
    return;
  }

  float ref_v    = atof(request->getParam("ref_v")->value().c_str());
  float measured = HiveSensor_Data.Solar_Voltage;

  if (ref_v <= 0.0f || measured <= 0.1f)
  {
    request->send(400, "application/json",
      "{\"ok\":false,\"message\":\"Valeurs invalides (ref>0, mesure>0.1V)\"}");
    return;
  }

  config.materiel.VSolScale = config.materiel.VSolScale * ref_v / measured;
  E24C32saveConfig();

  static char buf[96];
  snprintf(buf, sizeof(buf),
    "{\"ok\":true,\"message\":\"VSolScale=%.8f\"}",
    config.materiel.VSolScale);
  request->send(200, "application/json", buf);
}

// ---------------------------------------------------------------------------

bool webServerInit(void)
{
  // 1. Connexion WiFi STA (timeout 10 s)
  Serial.printf("[WiFi] Connexion a %s...\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000UL)
  {
    delay(200);
  }

  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("[WiFi] Connexion echouee — serveur web desactive");
    return false;
  }

  Serial.printf("[WiFi] Connecte : %s\n", WiFi.localIP().toString().c_str());

  // 2. LittleFS
  if (!LittleFS.begin(false))
  {
    Serial.println("[FS] LittleFS echec — reformatage...");
    if (!LittleFS.begin(true))
    {
      Serial.println("[FS] LittleFS irrecuperable");
      return false;
    }
  }
  Serial.printf("[FS] LittleFS OK — libre: %lu Ko\n",
                (unsigned long)LittleFS.totalBytes() / 1024UL);

  // 3. Routes API — Donnees
  s_server.on("/api/data",   HTTP_GET,  handleApiData);
  s_server.on("/api/status", HTTP_GET,  handleApiStatus);

  // 4. Routes API — LoRa
  s_server.on("/api/config/lora",      HTTP_GET,  handleApiConfigLora);
  s_server.on("/api/config/lora/sf",   HTTP_POST, handleApiConfigLoraSf);
  s_server.on("/api/config/lora/keys", HTTP_POST, handleApiConfigLoraKeys);
  s_server.on("/api/lora/join",        HTTP_POST, handleLoraJoin);
  s_server.on("/api/lora/send",        HTTP_POST, handleLoraSend);

  // 5. Routes API — Calibration
  s_server.on("/api/config/cal", HTTP_GET,  handleApiConfigCal);
  s_server.on("/api/hx711/raw",  HTTP_GET,  handleApiHx711Raw);
  s_server.on("/api/cal/tare",   HTTP_POST, handleCalTare);
  s_server.on("/api/cal/scale",  HTTP_POST, handleCalScale);
  s_server.on("/api/cal/vbat",   HTTP_POST, handleCalVbat);
  s_server.on("/api/cal/vsol",   HTTP_POST, handleCalVsol);

  // 6. Fichiers statiques depuis LittleFS (index.html par defaut)
  s_server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

  // 7. 404
  s_server.onNotFound([](AsyncWebServerRequest* req)
  {
    req->send(404, "text/plain", "Non trouve");
  });

  // 8. En-tetes CORS pour l'API (acces depuis navigateur local)
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");

  s_server.begin();
  Serial.println("[Web] Serveur demarre sur port 80");
  return true;
}

// ---------------------------------------------------------------------------

bool webServerIsConnected(void)
{
  return WiFi.status() == WL_CONNECTED;
}

// ---------------------------------------------------------------------------
// Traite les actions differees depuis loop()
// ---------------------------------------------------------------------------

void webServerProcess(void)
{
  if (s_joinRequested)
  {
    s_joinRequested = false;
    Serial.println("[Web] Join LoRa demande via API...");
    loraJoin();
  }

  if (s_sendRequested)
  {
    s_sendRequested = false;
    Serial.println("[Web] Envoi payload demande via API...");
    loraSendPayload(&HiveSensor_Data, slaveReadings, NUM_SLAVES);
  }

  if (s_tareRequested)
  {
    s_tareRequested = false;
    Serial.println("[Web] Tare demandee via API...");
    hx711Tare();
    E24C32saveConfig();
  }

  if (s_calibRequested)
  {
    s_calibRequested = false;
    Serial.printf("[Web] Calibration demandee via API (ref=%.1f g)...\n",
                  s_calibRefGrams);
    hx711CalcScale(s_calibRefGrams);
    E24C32saveConfig();
  }
}
