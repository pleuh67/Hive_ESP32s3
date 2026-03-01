// ble_master.cpp — Client GATT BLE master ESP32-S3
// Phase 2 : NimBLE 2.x, scan passif, lecture 3 caracteristiques par slave
//
// Cycle :
//   1. Init NimBLE (client, securite passkey)
//   2. Scan BLE_SCAN_SEC secondes pour decouvrir les slaves (service UUID)
//   3. Pour chaque slave : connexion + auth + lecture weight/vbat/timestamp
//   4. Remplissage slaveReadings[] + deinit NimBLE

#include <Arduino.h>
#include <NimBLEDevice.h>
#include "ble_master.h"
#include "config.h"
#include "credentials.h"
#include "types.h"

// ---------------------------------------------------------------------------
// Stockage des adresses trouvees pendant le scan
// NimBLEAddress stocke le type (public/random) + les 6 octets MAC
// ---------------------------------------------------------------------------

static NimBLEAddress s_foundAddresses[NUM_SLAVES];
static uint8_t       s_foundCount = 0;

// ---------------------------------------------------------------------------
// Callback scan : accumule les NimBLEAddress des slaves avec notre service UUID
// ---------------------------------------------------------------------------

class MasterScanCallbacks : public NimBLEScanCallbacks
{
  void onResult(const NimBLEAdvertisedDevice* device) override
  {
    if (s_foundCount >= NUM_SLAVES)
    {
      return; // Deja assez de slaves
    }

    if (!device->isAdvertisingService(NimBLEUUID(BLE_SERVICE_UUID)))
    {
      return; // Pas notre service
    }

    NimBLEAddress addr = device->getAddress();

    // Verifier que cet appareil n'est pas deja dans la liste
    for (uint8_t i = 0; i < s_foundCount; i++)
    {
      if (s_foundAddresses[i] == addr)
      {
        return; // Deja connu
      }
    }

    s_foundAddresses[s_foundCount] = addr;
    s_foundCount++;
    Serial.printf("[BLE] Slave detecte #%u : %s\n",
                  (unsigned)s_foundCount,
                  addr.toString().c_str());

    // Arreter le scan si tous les slaves sont trouves
    if (s_foundCount >= NUM_SLAVES)
    {
      NimBLEDevice::getScan()->stop();
    }
  }

  void onScanEnd(const NimBLEScanResults& results, int reason) override
  {
    Serial.printf("[BLE] Scan termine — %u slave(s) trouve(s) (reason=%d)\n",
                  (unsigned)s_foundCount, reason);
  }
};

// ---------------------------------------------------------------------------
// Callbacks client : connexion / deconnexion / securite
// ---------------------------------------------------------------------------

class MasterClientCallbacks : public NimBLEClientCallbacks
{
  void onConnect(NimBLEClient* pClient) override
  {
    Serial.printf("[BLE] Connecte a %s\n",
                  pClient->getPeerAddress().toString().c_str());
  }

  void onDisconnect(NimBLEClient* pClient, int reason) override
  {
    Serial.printf("[BLE] Deconnecte de %s (reason=%d)\n",
                  pClient->getPeerAddress().toString().c_str(), reason);
  }

  void onAuthenticationComplete(NimBLEConnInfo& connInfo) override
  {
    if (!connInfo.isAuthenticated())
    {
      Serial.println("[BLE] Auth echouee");
    }
    else
    {
      Serial.println("[BLE] Auth OK");
    }
  }

  // Fournit le passkey quand le slave le demande (NimBLE 2.x)
  void onPassKeyEntry(NimBLEConnInfo& connInfo) override
  {
    Serial.printf("[BLE] PassKey envoye : %06u\n", (unsigned)BLE_PASSKEY);
    NimBLEDevice::injectPassKey(connInfo, BLE_PASSKEY);
  }
};

// Instances statiques (pas d'allocation dynamique)
static MasterScanCallbacks   s_scanCB;
static MasterClientCallbacks s_clientCB;

// ---------------------------------------------------------------------------
// Lit les 3 caracteristiques d'un slave connecte
// Retourne true si toutes les lectures ont reussi
// ---------------------------------------------------------------------------

static bool readSlaveCharacteristics(NimBLEClient* client,
                                     SlaveReading_t* reading)
{
  NimBLERemoteService* svc = client->getService(BLE_SERVICE_UUID);
  if (!svc)
  {
    Serial.println("[BLE] Service introuvable sur ce slave");
    return false;
  }

  // --- Poids (int16_t, 2 octets) ---
  NimBLERemoteCharacteristic* chrW =
    svc->getCharacteristic(BLE_CHAR_WEIGHT_UUID);
  if (!chrW)
  {
    Serial.println("[BLE] Caracteristique poids introuvable");
    return false;
  }
  NimBLEAttValue valW = chrW->readValue();
  if (valW.size() < sizeof(int16_t))
  {
    Serial.println("[BLE] Reponse poids trop courte");
    return false;
  }
  memcpy(&reading->weight, valW.data(), sizeof(int16_t));

  // --- VBat (uint8_t, 1 octet) ---
  NimBLERemoteCharacteristic* chrV =
    svc->getCharacteristic(BLE_CHAR_VBAT_UUID);
  if (!chrV)
  {
    Serial.println("[BLE] Caracteristique VBat introuvable");
    return false;
  }
  NimBLEAttValue valV = chrV->readValue();
  if (valV.size() < sizeof(uint8_t))
  {
    Serial.println("[BLE] Reponse VBat trop courte");
    return false;
  }
  reading->vbat = valV.data()[0];

  // --- Timestamp (uint32_t, 4 octets) ---
  NimBLERemoteCharacteristic* chrT =
    svc->getCharacteristic(BLE_CHAR_TIME_UUID);
  if (!chrT)
  {
    Serial.println("[BLE] Caracteristique timestamp introuvable");
    return false;
  }
  NimBLEAttValue valT = chrT->readValue();
  if (valT.size() < sizeof(uint32_t))
  {
    Serial.println("[BLE] Reponse timestamp trop courte");
    return false;
  }
  memcpy(&reading->timestamp, valT.data(), sizeof(uint32_t));

  Serial.printf("[BLE] Slave lu : poids=%dg vbat=%u.%uV ts=%lu\n",
                (int)reading->weight * 10,
                (unsigned)(reading->vbat / 10 + 2),
                (unsigned)(reading->vbat % 10),
                (unsigned long)reading->timestamp);
  return true;
}

// ---------------------------------------------------------------------------

void bleMasterCollect(SlaveReading_t* readings, uint8_t count)
{
  // Reinitialiser les resultats
  for (uint8_t i = 0; i < count; i++)
  {
    readings[i].valid      = false;
    readings[i].weight     = PAYLOAD_INVALID_WEIGHT;
    readings[i].vbat       = PAYLOAD_INVALID_VBAT;
    readings[i].timestamp  = 0;
    readings[i].address[0] = '\0';
  }
  s_foundCount = 0;

  // --- Init NimBLE client ---
  NimBLEDevice::init("HiveMst");
  NimBLEDevice::setSecurityAuth(true, true, true);
  NimBLEDevice::setSecurityPasskey(BLE_PASSKEY);
  // Master saisit le passkey affiche par le slave
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_KEYBOARD_ONLY);

  // --- Scan BLE passif ---
  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->setScanCallbacks(&s_scanCB, false); // false = signaler les doublons (on filtre manuellement)
  scan->setActiveScan(false);               // Scan passif pour economie
  scan->setInterval(160);                   // 100 ms (unites de 0.625 ms)
  scan->setWindow(80);                      // 50 ms

  // NimBLE 2.x : duration en millisecondes
  Serial.printf("[BLE] Scan %d s...\n", BLE_SCAN_SEC);
  scan->start(static_cast<uint32_t>(BLE_SCAN_SEC) * 1000UL, false);

  // Attente fin du scan (start() est non bloquant en 2.x)
  while (scan->isScanning())
  {
    delay(100);
  }

  if (s_foundCount == 0)
  {
    Serial.println("[BLE] Aucun slave trouve");
    NimBLEDevice::deinit(true);
    return;
  }

  // --- Connexion a chaque slave trouve ---
  uint8_t slaveIdx = 0;
  for (uint8_t i = 0; i < s_foundCount && slaveIdx < count; i++)
  {
    Serial.printf("[BLE] Connexion slave %u : %s\n",
                  (unsigned)i,
                  s_foundAddresses[i].toString().c_str());

    snprintf(readings[slaveIdx].address,
             sizeof(readings[slaveIdx].address),
             "%s", s_foundAddresses[i].toString().c_str());

    NimBLEClient* client = NimBLEDevice::createClient();
    client->setClientCallbacks(&s_clientCB, false);
    client->setConnectTimeout(BLE_CONNECT_TIMEOUT_MS / 1000);

    bool connected = false;
    for (uint8_t retry = 0; retry < BLE_MAX_RETRIES && !connected; retry++)
    {
      connected = client->connect(s_foundAddresses[i]);
      if (!connected)
      {
        Serial.printf("[BLE] Echec connexion (tentative %u/%u)\n",
                      (unsigned)(retry + 1), (unsigned)BLE_MAX_RETRIES);
      }
    }

    if (connected)
    {
      readings[slaveIdx].valid =
        readSlaveCharacteristics(client, &readings[slaveIdx]);
      client->disconnect();
      if (readings[slaveIdx].valid)
      {
        slaveIdx++;
      }
    }
    else
    {
      Serial.printf("[BLE] Slave %s injoignable\n",
                    s_foundAddresses[i].toString().c_str());
    }

    NimBLEDevice::deleteClient(client);
  }

  Serial.printf("[BLE] Collecte terminee : %u/%u slave(s) valide(s)\n",
                (unsigned)slaveIdx, (unsigned)count);

  NimBLEDevice::deinit(true);
}
