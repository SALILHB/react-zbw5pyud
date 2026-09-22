/* Mock C++ du noyau Arduino — banc de tests natif (§11.4).
 * Reproduit uniquement ce que le programme utilise réellement. */
#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <string>
#include <vector>

#define HIGH 1
#define LOW  0
#define INPUT        0
#define OUTPUT       1
#define INPUT_PULLUP 2

/* Broches analogiques de l'Arduino Mega 2560 */
enum {
  A0 = 54, A1, A2, A3, A4, A5, A6, A7,
  A8, A9, A10, A11, A12, A13, A14, A15
};

const uint8_t NB_BROCHES_MOCK = 96;

struct MockArduino {
  uint32_t horloge_ms;
  int      entree_num[NB_BROCHES_MOCK];   // valeurs lues par digitalRead()
  int      sortie_num[NB_BROCHES_MOCK];   // valeurs écrites par digitalWrite()
  int      entree_ana[NB_BROCHES_MOCK];   // valeurs lues par analogRead()
  int      sortie_pwm[NB_BROCHES_MOCK];   // valeurs écrites par analogWrite()
  uint8_t  mode[NB_BROCHES_MOCK];

  void reinitialiser();
};

extern MockArduino mockIO;

void     pinMode(uint8_t pin, uint8_t mode);
void     digitalWrite(uint8_t pin, int valeur);
int      digitalRead(uint8_t pin);
void     analogWrite(uint8_t pin, int valeur);
int      analogRead(uint8_t pin);
uint32_t millis();
uint32_t micros();
void     delay(uint32_t ms);
void     delayMicroseconds(uint32_t us);

/* Port série simulé : les lignes émises sont conservées pour inspection. */
class SerieMock {
public:
  std::vector<std::string> lignes;
  std::string courante;
  void begin(long) {}
  void print(const char* s)   { courante += s; }
  void print(long v)          { char b[24]; snprintf(b, sizeof(b), "%ld", v); courante += b; }
  void println(const char* s) { courante += s; lignes.push_back(courante); courante.clear(); }
  void println(long v)        { print(v); lignes.push_back(courante); courante.clear(); }
  void println()              { lignes.push_back(courante); courante.clear(); }
};

extern SerieMock Serial;
