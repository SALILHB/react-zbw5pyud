/* Mock de la bibliothèque DallasTemperature — banc de tests natif.
 * Les températures des sondes sont pilotées par mock_ds18b20[] :
 *   index 0 = T_sec (chambre de séchage), index 1 = T_cap (plaque capteur). */
#pragma once
#include <stdint.h>
#include "OneWire.h"

#define DEVICE_DISCONNECTED_C (-127.0f)

const uint8_t NB_SONDES_MOCK = 4;
extern float mock_ds18b20[NB_SONDES_MOCK];

class DallasTemperature {
public:
  explicit DallasTemperature(OneWire* bus);
  void    begin();
  void    setResolution(uint8_t bits);
  void    setWaitForConversion(bool attendre);
  void    requestTemperatures();
  float   getTempCByIndex(uint8_t index);
  uint8_t getDeviceCount();
};
