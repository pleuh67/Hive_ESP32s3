// web_server.cpp — Serveur web master ESP32-S3
// Phase 4 : WiFi STA + ESPAsyncWebServer + LittleFS
//
// Architecture :
//   - WiFi STA, connexion au setup (timeout 10 s)
//   - ESPAsyncWebServer sur port 80 (async, non bloquant)
//   - LittleFS pour servir index.html
//   - JSON construit manuellement avec snprintf (pas d'ArduinoJson)
//   - Actions bloquantes (loraJoin) differees via flag -> traitees dans loop()

#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include "web_server.h"
#include "lora_manager.h"
#include "config.h"
#include "credentials.h"
#include "types.h"

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
static bool           s_joinRequested = false; // flag pour action differee

// ---------------------------------------------------------------------------
// Construction JSON (snprintf dans buffer statique)
// Le buffer est rempli et passe a request->send() qui le copie immediatement.
// ---------------------------------------------------------------------------

// Serialize les donnees capteurs master + slaves en JSON
// buf : destination (600 octets recommandes)
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

// POST /api/lora/join — loraJoin() est bloquant, on utilise un flag
static void handleLoraJoin(AsyncWebServerRequest* request)
{
  s_joinRequested = true;
  request->send(202, "application/json",
                "{\"status\":\"join_requested\"}");
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
      Serial.println("[FS] LittleFS irrécuperable");
      return false;
    }
  }
  Serial.printf("[FS] LittleFS OK — libre: %lu Ko\n",
                (unsigned long)LittleFS.totalBytes() / 1024UL);

  // 3. Routes API
  s_server.on("/api/data",   HTTP_GET,  handleApiData);
  s_server.on("/api/status", HTTP_GET,  handleApiStatus);
  s_server.on("/api/lora/join", HTTP_POST, handleLoraJoin);

  // 4. Fichiers statiques depuis LittleFS (index.html par defaut)
  s_server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

  // 5. 404
  s_server.onNotFound([](AsyncWebServerRequest* req)
  {
    req->send(404, "text/plain", "Non trouve");
  });

  // 6. En-tetes CORS pour l'API (acces depuis navigateur local)
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
}
