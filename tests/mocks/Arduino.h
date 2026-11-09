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

/* Port série simulé : les lignes émises sont conservées pour inspection ;
 * les lignes à « recevoir » sont déposées par injecter(). place_tx simule la
 * place libre du tampon d'émission (63 octets sur l'ATmega2560). */
class SerieMock {
public:
  std::vector<std::string> lignes;
  std::string courante;
  std::string entree;
  size_t      pos_entree = 0;
  int         place_tx = 63;
  bool        limiter_tx = false;   // true : chaque octet écrit consomme place_tx
  void begin(long) {}
  void print(const char* s)   { for (; *s; s++) write((uint8_t)*s); }
  void print(long v)          { char b[24]; snprintf(b, sizeof(b), "%ld", v); print(b); }
  void println(const char* s) { print(s); write('\n'); }
  void println(long v)        { print(v); write('\n'); }
  void println()              { write('\n'); }
  size_t write(uint8_t c) {
    if (limiter_tx && place_tx > 0) place_tx--;
    if (c == '\n') { lignes.push_back(courante); courante.clear(); }
    else courante += (char)c;
    return 1;
  }
  size_t write(const uint8_t* b, size_t n) { for (size_t i = 0; i < n; i++) write(b[i]); return n; }
  int availableForWrite()     { return place_tx; }
  int available()             { return (int)(entree.size() - pos_entree); }
  int read()                  { return pos_entree < entree.size() ? (uint8_t)entree[pos_entree++] : -1; }
  void injecter(const std::string& s) { entree += s; }
  void viderEntree()          { entree.clear(); pos_entree = 0; }
};

extern SerieMock Serial;
extern SerieMock Serial1;

/* Mémoire programme (PROGMEM) : sur PC, tout est en RAM. */
#define PROGMEM
#define PSTR(s) (s)
#define F(s) (s)
#define snprintf_P snprintf
#define strcmp_P strcmp
#define strncpy_P strncpy
#define memcpy_P memcpy
#define pgm_read_byte(p) (*(const uint8_t*)(p))
#define pgm_read_word(p) (*(p))
