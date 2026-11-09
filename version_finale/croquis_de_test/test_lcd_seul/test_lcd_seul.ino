/* =====================================================================
 *  TEST ÉCRAN LCD I2C SEUL — Arduino Mega 2560
 *  SDA -> D20, SCL -> D21, VCC -> 5 V, GND -> GND
 *  Cherche l'adresse de l'écran tout seul (0x20..0x27, 0x38..0x3F),
 *  puis affiche un texte, un compteur et un défilement de caractères.
 *  Aucune bibliothèque à installer (pilote LCD intégré, Wire seulement).
 *  Moniteur série 115200 bauds : affiche l'adresse trouvée.
 * ===================================================================*/
#include <Wire.h>

class LcdI2C {
  uint8_t adr, bl = 0x08;
  void ecrire(uint8_t v) { Wire.beginTransmission(adr); Wire.write(v | bl); Wire.endTransmission(); }
  void impulsion(uint8_t v) { ecrire(v | 0x04); delayMicroseconds(1); ecrire(v & ~0x04); delayMicroseconds(50); }
  void quartet(uint8_t v) { ecrire(v); impulsion(v); }
  void envoyer(uint8_t val, uint8_t mode) { quartet((val & 0xF0) | mode); quartet(((val << 4) & 0xF0) | mode); }
  void commande(uint8_t c) { envoyer(c, 0); }
public:
  void init(uint8_t a) {
    adr = a; delay(50); ecrire(0); delay(100);
    quartet(0x30); delayMicroseconds(4500); quartet(0x30); delayMicroseconds(4500);
    quartet(0x30); delayMicroseconds(150);  quartet(0x20);
    commande(0x28); commande(0x0C); clear(); commande(0x06);
  }
  void backlight(bool on) { bl = on ? 0x08 : 0x00; ecrire(0); }
  void clear() { commande(0x01); delay(2); }
  void setCursor(uint8_t c, uint8_t l) { static const uint8_t o[4] = {0x00, 0x40, 0x14, 0x54}; commande(0x80 | (c + o[l & 3])); }
  void print(char c) { envoyer((uint8_t)c, 0x01); }
  void print(const char* t) { while (*t) print(*t++); }
  void print(const __FlashStringHelper* f) { PGM_P p = reinterpret_cast<PGM_P>(f); char c; while ((c = pgm_read_byte(p++))) print(c); }
};

LcdI2C lcd;
uint8_t adresse = 0;

uint8_t chercherAdresse() {
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      Serial.print(F("Peripherique I2C trouve a 0x")); Serial.println(a, HEX);
      if ((a >= 0x20 && a <= 0x27) || (a >= 0x38 && a <= 0x3F)) return a;
    }
  }
  return 0;
}

void setup() {
  pinMode(4, OUTPUT); digitalWrite(4, LOW);      // ventilateur d'extraction arrêté
  Serial.begin(115200);
  Wire.begin();
  Wire.setWireTimeout(3000, true);               // évite un blocage si SDA/SCL est en défaut
  Serial.println(F("Test LCD seul : recherche de l'ecran..."));
  adresse = chercherAdresse();
  if (adresse == 0) {
    Serial.println(F("AUCUN ecran trouve : verifier SDA=D20, SCL=D21, 5 V et GND."));
    return;                                       // loop() continue de chercher
  }
  Serial.print(F("Ecran utilise a l'adresse 0x")); Serial.println(adresse, HEX);
  lcd.init(adresse);
  lcd.backlight(true);

  // 1) clignotement du rétroéclairage
  for (uint8_t i = 0; i < 3; i++) { lcd.backlight(false); delay(250); lcd.backlight(true); delay(250); }

  // 2) écran de présentation
  char l[21];
  lcd.setCursor(0, 0); lcd.print(F("  SECHOIR HYBRIDE   "));
  lcd.setCursor(0, 1); lcd.print(F("  Test ecran LCD    "));
  snprintf(l, sizeof(l), "  Adresse I2C 0x%02X  ", adresse);
  lcd.setCursor(0, 2); lcd.print(l);
  lcd.setCursor(0, 3); lcd.print(F("  ESSA Tlemcen      "));
  delay(4000);

  // 3) remplissage complet (vérifie les 80 cases)
  lcd.clear();
  for (uint8_t r = 0; r < 4; r++) { lcd.setCursor(0, r); for (uint8_t c = 0; c < 20; c++) lcd.print((char)255); }
  delay(2000);
  lcd.clear();
}

void scanPeriodique() {                         // toutes les 3 s : liste du bus I2C
  static uint32_t t = 0;
  if (millis() - t < 3000) return;
  t = millis();
  uint8_t nb = 0;
  Serial.print(F("Scan I2C : "));
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) { Serial.print(F("0x")); Serial.print(a, HEX); Serial.print(' '); nb++; }
  }
  if (nb == 0) Serial.print(F("RIEN -> verifier SDA=D20, SCL=D21, 5 V, GND"));
  Serial.println();
  if (adresse == 0) {                             // écran branché après coup
    adresse = chercherAdresse();
    if (adresse) { lcd.init(adresse); lcd.backlight(true); }
  }
}

void loop() {
  scanPeriodique();
  if (adresse == 0) return;
  static uint32_t t0 = 0;
  static uint8_t decal = 0;
  if (millis() - t0 < 300) return;
  t0 = millis();
  char l[21];
  uint32_t s = millis() / 1000UL;
  lcd.setCursor(0, 0); lcd.print(F("TEST LCD : OK       "));
  snprintf(l, sizeof(l), "Temps : %02lu:%02lu:%02lu    ", s / 3600UL, (s / 60UL) % 60UL, s % 60UL);
  lcd.setCursor(0, 1); lcd.print(l);
  snprintf(l, sizeof(l), "Adresse : 0x%02X      ", adresse);
  lcd.setCursor(0, 2); lcd.print(l);
  lcd.setCursor(0, 3);                            // défilement ABC...789
  static const char motif[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 ";
  const uint8_t n = sizeof(motif) - 1;
  for (uint8_t c = 0; c < 20; c++) lcd.print(motif[(decal + c) % n]);
  decal = (decal + 1) % n;
}
