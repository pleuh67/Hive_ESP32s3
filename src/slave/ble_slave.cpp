// ble_slave.cpp — Serveur GATT BLE slave ESP32-S3
// Phase 2 : NimBLE 2.x, passkey statique, 3 caracteristiques en lecture
//
// Cycle de vie :
//   bleSlaveInit()  -> configure server + caracteristiques
//   bleSlaveStart() -> lance l'advertising (non bloquant)
//   bleSlaveIsComplete() -> surveille fin de session (loop ou attente)
//   bleSlaveStop()  -> libere NimBLE avant deep sleep

#include <Arduino.h>
#include <NimBLEDevice.h>
#include "ble_slave.h"
#include "config.h"
#include "credentials.h"

// ---------------------------------------------------------------------------
// Variables module
// ---------------------------------------------------------------------------

static NimBLECharacteristic* s_chrWeight    = nullptr;
static NimBLECharacteristic* s_chrVbat      = nullptr;
static NimBLECharacteristic* s_chrTimestamp = nullptr;

// Positionne a true des qu'une deconnexion est detectee (master a lu)
static volatile bool s_bleComplete = false;

// ---------------------------------------------------------------------------
// Callbacks serveur GATT
// ---------------------------------------------------------------------------

class SlaveServerCallbacks : public NimBLEServerCallbacks
{
  void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override
  {
    Serial.printf("[BLE] Master connecte : %s\n",
                  connInfo.getAddress().toString().c_str());
    // Arreter l'advertising pendant la connexion (un seul master a la fois)
    NimBLEDevice::getAdvertising()->stop();
  }

  void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo,
                    int reason) override
  {
    Serial.printf("[BLE] Deconnecte (reason=%d)\n", reason);
    s_bleComplete = true;
  }

  void onAuthenticationComplete(NimBLEConnInfo& connInfo) override
  {
    if (!connInfo.isAuthenticated())
    {
      Serial.println("[BLE] Auth echouee — deconnexion forcee");
      NimBLEDevice::getServer()->disconnect(connInfo.getConnHandle());
    }
    else
    {
      Serial.println("[BLE] Auth OK");
    }
  }
};

class SlaveCharCallbacks : public NimBLECharacteristicCallbacks
{
  void onRead(NimBLECharacteristic* pChr, NimBLEConnInfo& connInfo) override
  {
    Serial.printf("[BLE] Lecture %s par %s\n",
                  pChr->getUUID().toString().c_str(),
                  connInfo.getAddress().toString().c_str());
  }
};

// Instances statiques des callbacks (pas d'allocation dynamique)
static SlaveServerCallbacks s_serverCB;
static SlaveCharCallbacks   s_charCB;

// ---------------------------------------------------------------------------

void bleSlaveInit(int16_t weight_raw, uint8_t vbat_enc, uint32_t timestamp)
{
  s_bleComplete = false;

  NimBLEDevice::init("HiveSlv");

  // Securite : bond + MITM + Secure Connections, passkey fixe
  NimBLEDevice::setSecurityAuth(true, true, true);
  NimBLEDevice::setSecurityPasskey(BLE_PASSKEY);
  // Slave affiche le passkey (fixe) — master doit le saisir
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);

  // Serveur GATT
  NimBLEServer* server = NimBLEDevice::createServer();
  server->setCallbacks(&s_serverCB);

  // Service
  NimBLEService* svc = server->createService(BLE_SERVICE_UUID);

  // Caracteristique poids — int16_t (2 octets), lecture chiffree
  s_chrWeight = svc->createCharacteristic(
    BLE_CHAR_WEIGHT_UUID,
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::READ_ENC
  );
  s_chrWeight->setCallbacks(&s_charCB);
  s_chrWeight->setValue(reinterpret_cast<uint8_t*>(&weight_raw),
                        sizeof(weight_raw));

  // Caracteristique VBat — uint8_t (1 octet), lecture chiffree
  s_chrVbat = svc->createCharacteristic(
    BLE_CHAR_VBAT_UUID,
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::READ_ENC
  );
  s_chrVbat->setCallbacks(&s_charCB);
  s_chrVbat->setValue(&vbat_enc, sizeof(vbat_enc));

  // Caracteristique timestamp — uint32_t (4 octets), lecture chiffree
  s_chrTimestamp = svc->createCharacteristic(
    BLE_CHAR_TIME_UUID,
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::READ_ENC
  );
  s_chrTimestamp->setCallbacks(&s_charCB);
  s_chrTimestamp->setValue(reinterpret_cast<uint8_t*>(&timestamp),
                           sizeof(timestamp));

  svc->start();

  Serial.printf("[BLE] Slave init OK — poids=%d vbat=%u ts=%lu\n",
                (int)weight_raw, (unsigned)vbat_enc,
                (unsigned long)timestamp);
}

// ---------------------------------------------------------------------------

void bleSlaveStart(void)
{
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(BLE_SERVICE_UUID);
  adv->setName("HiveSlv");
  adv->setMinInterval(BLE_ADV_INTERVAL_MS / 0.625f); // unites de 0.625 ms
  adv->setMaxInterval(BLE_ADV_INTERVAL_MS / 0.625f);

  // NimBLE 2.x : duration en millisecondes (0 = indefini)
  adv->start(static_cast<uint32_t>(BLE_ADVERTISING_SEC) * 1000UL);

  Serial.printf("[BLE] Advertising lance (%d s)\n", BLE_ADVERTISING_SEC);
}

// ---------------------------------------------------------------------------

bool bleSlaveIsComplete(void)
{
  // Termine si : master deconnecte OU timeout advertising expire
  return s_bleComplete ||
         !NimBLEDevice::getAdvertising()->isAdvertising();
}

// ---------------------------------------------------------------------------

void bleSlaveStop(void)
{
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  if (adv->isAdvertising())
  {
    adv->stop();
  }

  NimBLEDevice::deinit(true); // true = liberation complete

  s_chrWeight    = nullptr;
  s_chrVbat      = nullptr;
  s_chrTimestamp = nullptr;
  s_bleComplete  = false;

  Serial.println("[BLE] Slave BLE arrete");
}
