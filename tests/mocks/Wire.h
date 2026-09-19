/* Mock de la bibliothèque Wire (I2C) — banc de tests natif. */
#pragma once
#include <stdint.h>

class TwoWire {
public:
  void begin() {}
  void setClock(uint32_t) {}
};

extern TwoWire Wire;
