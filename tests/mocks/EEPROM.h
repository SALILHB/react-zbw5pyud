/* Mock de la bibliothèque EEPROM (AVR) — banc de tests natif.
 * Reproduit EEPROM.get()/put() sur un tableau en mémoire, avec la même
 * sémantique (copie d'octets bruts, aucune validation de type). */
#pragma once
#include <stdint.h>
#include <string.h>

const size_t TAILLE_EEPROM_MOCK = 4096;   // Mega 2560 : 4 Ko d'EEPROM interne

class EEPROMClass {
public:
  uint8_t donnees[TAILLE_EEPROM_MOCK];

  EEPROMClass() { memset(donnees, 0xFF, sizeof(donnees)); }

  template <typename T>
  T& get(int adresse, T& valeur) {
    memcpy(&valeur, &donnees[adresse], sizeof(T));
    return valeur;
  }

  template <typename T>
  const T& put(int adresse, const T& valeur) {
    memcpy(&donnees[adresse], &valeur, sizeof(T));
    return valeur;
  }

  uint8_t read(int adresse) { return donnees[adresse]; }
  void    write(int adresse, uint8_t valeur) { donnees[adresse] = valeur; }
  size_t  length() { return TAILLE_EEPROM_MOCK; }
};

extern EEPROMClass EEPROM;
