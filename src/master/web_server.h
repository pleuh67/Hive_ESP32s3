// web_server.h — Serveur web master ESP32-S3
// Phase 4 : WiFi STA + ESPAsyncWebServer + LittleFS
// Phase 5 : Multi-ecrans (Donnees / LoRa / Calibration)
//
// Routes Donnees :
//   GET  /                       -> index.html (LittleFS)
//   GET  /api/data               -> JSON capteurs master + slaves
//   GET  /api/status             -> JSON etat systeme (WiFi, LoRa, uptime)
//
// Routes LoRa :
//   GET  /api/config/lora        -> JSON config LoRa (sf, deveui, appeui, joined)
//   POST /api/config/lora/sf     -> ?sf=9      — met a jour SpreadingFactor + save
//   POST /api/config/lora/keys   -> ?appeui=XX&appkey=XX — met a jour cles + save
//   POST /api/lora/join          -> declenche loraJoin() (deferred, flag)
//   POST /api/lora/send          -> declenche loraSendPayload() (deferred, flag)
//
// Routes Calibration :
//   GET  /api/config/cal         -> JSON calibration (noload, scaling, vbat/vsol scale)
//   GET  /api/hx711/raw          -> JSON lecture brute live {raw, weight_g}
//   POST /api/cal/tare           -> pose tare a vide (deferred, flag)
//   POST /api/cal/scale          -> ?ref_g=10500 — calibre echelle (deferred, flag)
//   POST /api/cal/vbat           -> ?ref_v=3.85  — ajuste VBatScale + save
//   POST /api/cal/vsol           -> ?ref_v=5.20  — ajuste VSolScale + save
//
// Gestion du deep sleep :
//   Si WiFi est connecte, le deep sleep est supprime pour garder
//   le serveur accessible. La boucle payload continue via alarme RTC.

#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <stdbool.h>

// Initialise le WiFi (STA), monte LittleFS et demarre le serveur.
// Bloquant jusqu'a 10 s pour la connexion WiFi.
// Retourne true si WiFi connecte et serveur demarre.
bool webServerInit(void);

// Retourne true si le WiFi est actuellement connecte.
bool webServerIsConnected(void);

// A appeler depuis loop() : traite les actions differees (join LoRa, envoi payload,
// tare HX711, calibration HX711) demandees via l'API POST sans bloquer le callback async.
void webServerProcess(void);

#endif // WEB_SERVER_H
