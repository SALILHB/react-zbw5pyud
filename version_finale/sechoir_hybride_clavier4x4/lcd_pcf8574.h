/* ============================================================================
 *  lcd_pcf8574.h — pilote minimal LCD HD44780 20x4 + module I2C PCF8574
 *  [BANC 2026-11-09] Remplace la bibliothèque LiquidCrystal_I2C (non installée
 *  sur le PC de l'atelier) : aucune bibliothèque externe, Wire seulement.
 *  Essayé sur la carte réelle (test_lcd_seul, test_clavier_lcd) à l'adresse 0x21.
 *
 *  Brochage du module : P0 = RS, P1 = RW, P2 = EN, P3 = rétroéclairage,
 *                       P4..P7 = D4..D7 de l'afficheur.
 *  Dérive de Print : lcd.print() accepte texte, entiers, flottants et F().
 *  API identique à celle utilisée par le programme : init, backlight, clear,
 *  setCursor, print (+ setAdresse pour l'adresse trouvée au démarrage).
 * ==========================================================================*/
#pragma once
#include <Arduino.h>
#include <Wire.h>

class LcdPcf8574 : public Print {
  uint8_t adr, nbCol, nbLig, bl = 0x08;          // bl : bit P3 = rétroéclairage
  void ecrire(uint8_t v) { Wire.beginTransmission(adr); Wire.write(v | bl); Wire.endTransmission(); }
  void impulsion(uint8_t v) { ecrire(v | 0x04); delayMicroseconds(1); ecrire(v & ~0x04); delayMicroseconds(50); }
  void quartet(uint8_t v) { ecrire(v); impulsion(v); }
  void envoyer(uint8_t val, uint8_t mode) { quartet((val & 0xF0) | mode); quartet(((val << 4) & 0xF0) | mode); }
  void commande(uint8_t c) { envoyer(c, 0); }
public:
  LcdPcf8574(uint8_t a, uint8_t col, uint8_t lig) : adr(a), nbCol(col), nbLig(lig) {}
  void setAdresse(uint8_t a) { adr = a; }
  uint8_t adresse() const { return adr; }
  void init() {                                   // séquence d'initialisation 4 bits HD44780
    delay(50); ecrire(0); delay(100);
    quartet(0x30); delayMicroseconds(4500); quartet(0x30); delayMicroseconds(4500);
    quartet(0x30); delayMicroseconds(150);  quartet(0x20);
    commande(0x28); commande(0x0C); clear(); commande(0x06);
  }
  void begin() { init(); }
  void backlight()   { bl = 0x08; ecrire(0); }
  void noBacklight() { bl = 0x00; ecrire(0); }
  void clear() { commande(0x01); delay(2); }
  void setCursor(uint8_t c, uint8_t l) {
    static const uint8_t o[4] = {0x00, 0x40, 0x14, 0x54};   // 20x4
    commande(0x80 | (c + o[l & 3]));
  }
  size_t write(uint8_t c) override { envoyer(c, 0x01); return 1; }
  using Print::write;
};
