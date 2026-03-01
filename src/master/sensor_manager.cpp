// sensor_manager.cpp — Capteurs I2C master : BME280, BH1750, INA219
// Phase 1 — master uniquement
// Nouveau : remplace DHT22 + LDR + getTemperature() du POC_ATSAMD
//
// BME280  : temperature + humidite + pression (remplace DHT22 + analogique)
// BH1750  : luminosite en lux (remplace LDR analogique)
// INA219  : courant du panneau solaire en mA (nouveau)
// VBat    : tension batterie via ADC (diviseur resistif)
// VSol    : tension solaire via ADC (diviseur resistif)

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include <BH1750.h>
#include <Adafruit_INA219.h>
#include "sensor_manager.h"
#include "types.h"
#include "config.h"

// ===== OBJETS CAPTEURS =====
static Adafruit_BME280 bme;
static BH1750          bh1750;
static Adafruit_INA219 ina219(INA219_ADDRESS);

// ===== FLAGS DE PRESENCE =====
static bool bmeOK    = false;
static bool bh1750OK = false;
static bool ina219OK = false;

// ===== VARIABLES EXTERNES =====
extern HiveSensor_Data_t HiveSensor_Data;
extern ConfigGenerale_t  config;

// ---------------------------------------------------------------------------
// @brief Initialise tous les capteurs du master
// @param void
// @return bool false si au moins un capteur est absent
// ---------------------------------------------------------------------------
bool sensorsInit(void)
{
  bool ok = true;

  // BME280 (T/HR/Pression)
  bmeOK = bme.begin(BME280_ADDRESS);
  if (bmeOK)
  {
    // Mode "indoor navigation" : sur-echantillonnage x1, filtre x16
    bme.setSampling(Adafruit_BME280::MODE_FORCED,
                    Adafruit_BME280::SAMPLING_X1,
                    Adafruit_BME280::SAMPLING_X1,
                    Adafruit_BME280::SAMPLING_X1,
                    Adafruit_BME280::FILTER_X16,
                    Adafruit_BME280::STANDBY_MS_0_5);
    LOG_INFO("BME280 OK");
  }
  else
  {
    LOG_ERROR("BME280 introuvable (addr 0x76)");
    ok = false;
  }

  // BH1750 (Luminosite)
  bh1750OK = bh1750.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, BH1750_ADDRESS);
  if (bh1750OK)
  {
    LOG_INFO("BH1750 OK");
  }
  else
  {
    LOG_ERROR("BH1750 introuvable (addr 0x23)");
    ok = false;
  }

  // INA219 (Courant solaire)
  ina219OK = ina219.begin();
  if (ina219OK)
  {
    LOG_INFO("INA219 OK");
  }
  else
  {
    LOG_ERROR("INA219 introuvable (addr 0x40)");
    ok = false;
  }

  return ok;
}

// ---------------------------------------------------------------------------
// @brief Lit le BME280 : temperature, humidite, pression
// @param void
// @return bool true si lecture reussie
// ---------------------------------------------------------------------------
bool sensorReadBME280(void)
{
  if (!bmeOK) return false;

  bme.takeForcedMeasurement();

  float t = bme.readTemperature();
  float h = bme.readHumidity();
  float p = bme.readPressure() / 100.0f; // hPa

  if (isnan(t) || isnan(h) || isnan(p))
  {
    LOG_ERROR("BME280 lecture NaN");
    return false;
  }

  HiveSensor_Data.DHT_Temp = t;
  HiveSensor_Data.DHT_Hum  = h;
  HiveSensor_Data.Pressure = p;
  return true;
}

// ---------------------------------------------------------------------------
// @brief Lit le BH1750 : luminosite en lux
// @param void
// @return bool true si lecture reussie
// ---------------------------------------------------------------------------
bool sensorReadBH1750(void)
{
  if (!bh1750OK) return false;

  float lux = bh1750.readLightLevel();
  if (lux < 0.0f)
  {
    LOG_ERROR("BH1750 lecture erreur");
    return false;
  }

  HiveSensor_Data.Brightness = lux;
  return true;
}

// ---------------------------------------------------------------------------
// @brief Lit l'INA219 : courant du panneau solaire en mA
// @param void
// @return bool true si lecture reussie
// ---------------------------------------------------------------------------
bool sensorReadINA219(void)
{
  if (!ina219OK) return false;

  float mA = ina219.getCurrent_mA();
  if (isnan(mA))
  {
    LOG_ERROR("INA219 lecture NaN");
    return false;
  }

  HiveSensor_Data.SolarCurrent = mA;
  return true;
}

// ---------------------------------------------------------------------------
// @brief Lit la tension batterie via ADC (diviseur resistif)
// @param void
// @return bool true
// ---------------------------------------------------------------------------
bool sensorReadVBat(void)
{
  // Moyenne de 8 lectures pour reduire le bruit ADC
  uint32_t sum = 0;
  for (uint8_t i = 0; i < 8; i++)
  {
    sum += (uint32_t)analogRead(PIN_VBAT_ADC);
  }
  float adcVal = (float)(sum / 8);

  // Conversion avec le coefficient de calibration par carte
  uint8_t carte = config.materiel.Num_Carte;
  if (carte >= 10) carte = 0;
  HiveSensor_Data.Bat_Voltage = adcVal * VBatScale_List[carte];
  return true;
}

// ---------------------------------------------------------------------------
// @brief Lit la tension solaire via ADC (diviseur resistif) — master only
// @param void
// @return bool true
// ---------------------------------------------------------------------------
bool sensorReadVSol(void)
{
  uint32_t sum = 0;
  for (uint8_t i = 0; i < 8; i++)
  {
    sum += (uint32_t)analogRead(PIN_VSOL_ADC);
  }
  float adcVal = (float)(sum / 8);

  uint8_t carte = config.materiel.Num_Carte;
  if (carte >= 10) carte = 0;
  HiveSensor_Data.Solar_Voltage = adcVal * VSolScale_List[carte];
  return true;
}

// ---------------------------------------------------------------------------
// @brief Lit tous les capteurs du master
// @param void
// @return bool false si au moins une lecture a echoue
// ---------------------------------------------------------------------------
bool sensorsReadAll(void)
{
  bool ok = true;
  ok &= sensorReadBME280();
  ok &= sensorReadBH1750();
  ok &= sensorReadINA219();
  ok &= sensorReadVBat();
  ok &= sensorReadVSol();
  return ok;
}

// ---------------------------------------------------------------------------
// @brief Affiche toutes les valeurs capteurs sur Serial
// @param void
// @return void
// ---------------------------------------------------------------------------
void sensorsPrintAll(void)
{
  char buf[80];
  snprintf(buf, sizeof(buf),
           "T=%.1fC HR=%.0f%% P=%.0fhPa Lux=%.0f I=%.0fmA Vb=%.2fV Vs=%.2fV",
           HiveSensor_Data.DHT_Temp,
           HiveSensor_Data.DHT_Hum,
           HiveSensor_Data.Pressure,
           HiveSensor_Data.Brightness,
           HiveSensor_Data.SolarCurrent,
           HiveSensor_Data.Bat_Voltage,
           HiveSensor_Data.Solar_Voltage);
  Serial.println(buf);
}
