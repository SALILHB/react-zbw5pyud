/* Mock de la bibliothèque OneWire — banc de tests natif. */
#pragma once
#include <stdint.h>

class OneWire {
public:
  explicit OneWire(uint8_t) {}
};
