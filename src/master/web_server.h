// web_server.h — Serveur web master ESP32-S3
// Phase 4 : WiFi STA + ESPAsyncWebServer + LittleFS
//
// Routes :
//   GET  /                  -> index.html (LittleFS)
//   GET  /api/data          -> JSON capteurs master + slaves
//   GET  /api/status        -> JSON etat systeme (WiFi, LoRa, uptime)
//   POST /api/lora/join     -> declenche loraJoin() (async, flag)
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

// A appeler depuis loop() : traite les actions differees (join LoRa demande
// via l'API POST sans bloquer le callback async).
void webServerProcess(void);

#endif // WEB_SERVER_H
