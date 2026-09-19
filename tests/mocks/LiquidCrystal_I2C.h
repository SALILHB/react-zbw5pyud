/* Mock de la bibliothèque LiquidCrystal_I2C — banc de tests natif.
 * Le contenu affiché est relu dans mock_lcd_lignes[0..1]. */
#pragma once
#include <stdint.h>
#include <string>

extern std::string mock_lcd_lignes[2];

class LiquidCrystal_I2C {
public:
  LiquidCrystal_I2C(uint8_t adresse, uint8_t colonnes, uint8_t lignes);
  void init();
  void begin();
  void backlight();
  void noBacklight();
  void clear();
  void setCursor(uint8_t colonne, uint8_t ligne);
  void print(const char* texte);
private:
  uint8_t colonne_;
  uint8_t ligne_;
};
