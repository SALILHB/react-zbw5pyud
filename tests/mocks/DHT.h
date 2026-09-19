/* Mock de la bibliothèque DHT — banc de tests natif.
 * Les mesures sont indexées par la broche du capteur. */
#pragma once
#include <stdint.h>
#include "Arduino.h"

#define DHT11 11
#define DHT21 21
#define DHT22 22

extern float mock_dht_temp[NB_BROCHES_MOCK];
extern float mock_dht_hum[NB_BROCHES_MOCK];

class DHT {
public:
  DHT(uint8_t pin, uint8_t type);
  void  begin();
  float readTemperature();
  float readHumidity();
private:
  uint8_t pin_;
};
