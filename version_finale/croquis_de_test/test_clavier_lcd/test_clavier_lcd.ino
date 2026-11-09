/* =====================================================================
 *  TEST CLAVIER 4x4 (S1..S16) AVEC AFFICHAGE LCD — Arduino Mega 2560
 *  Clavier : fils 1..8 -> D30 D32 D34 D36 D38 D40 D44 D46 (D30..D36 = colonnes C4..C1, D38..D46 = lignes R1..R4)
 *            (D42 sautée : arrêt d'urgence)
 *  LCD 20x4 I2C : SDA -> D20, SCL -> D21 (adresse cherchée automatiquement)
 *  Aucune bibliothèque à installer (pilote LCD intégré, Wire seulement).
 *
 *  Écran :
 *   ligne 1 : TEST CLAVIER 4x4  + nombre d'appuis
 *   ligne 2 : touche appuyée et sa position (ligne R, colonne C)
 *   ligne 3 : fonction de la touche dans le programme du séchoir
 *   ligne 4 : "Vus" + 16 cases : # = touche déjà testée, . = pas encore
 *  Moniteur série 115200 bauds : même information.
 * ===================================================================*/
#include <Wire.h>

class LcdI2C {
  uint8_t adr = 0x21, bl = 0x08;
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
  void clear() { commande(0x01); delay(2); }
  void setCursor(uint8_t c, uint8_t l) { static const uint8_t o[4] = {0x00, 0x40, 0x14, 0x54}; commande(0x80 | (c + o[l & 3])); }
  void print(char c) { envoyer((uint8_t)c, 0x01); }
  void print(const char* t) { while (*t) print(*t++); }
  void print(const __FlashStringHelper* f) { PGM_P p = reinterpret_cast<PGM_P>(f); char c; while ((c = pgm_read_byte(p++))) print(c); }
};

LcdI2C lcd;

const uint8_t LIGNES[4]   = {38, 40, 44, 46};   // R1..R4 pilotées à LOW (fils 5..8 du connecteur, relevé sur le clavier réel)
const uint8_t COLONNES[4] = {36, 34, 32, 30};   // C1..C4 lues en INPUT_PULLUP (fils 4..1 du connecteur, relevé sur le clavier réel)

uint16_t precedent = 0;
uint16_t vus = 0;          // bit n-1 = touche Sn déjà appuyée
uint16_t nbAppuis = 0;

const char* fonction(uint8_t n) {
  switch (n) {
    case 1:  return "MENU";
    case 2:  return "UP (+)";
    case 6:  return "OK";
    case 10: return "DOWN (-)";
    case 4:  return "START";
    case 8:  return "STOP";
    case 16: return "REARMEMENT";
    default: return "non utilisee";
  }
}

uint16_t lireClavier() {
  uint16_t etat = 0;
  for (uint8_t r = 0; r < 4; r++) {
    digitalWrite(LIGNES[r], LOW);
    pinMode(LIGNES[r], OUTPUT);
    delayMicroseconds(10);
    for (uint8_t c = 0; c < 4; c++)
      if (digitalRead(COLONNES[c]) == LOW) etat |= (uint16_t)1 << (r * 4 + c);
    pinMode(LIGNES[r], INPUT_PULLUP);
  }
  return etat;
}

void ligne(uint8_t l, const char* t) {          // écrit une ligne complète de 20 caractères
  char b[21];
  snprintf(b, sizeof(b), "%-20s", t);
  lcd.setCursor(0, l); lcd.print(b);
}

void afficherVus() {
  char b[21] = "Vus ";
  for (uint8_t i = 0; i < 16; i++) b[4 + i] = (vus & ((uint16_t)1 << i)) ? '#' : '.';
  b[20] = 0;
  lcd.setCursor(0, 3); lcd.print(b);
}

void afficherEntete() {
  char b[21];
  snprintf(b, sizeof(b), "TEST CLAVIER %4u ap", nbAppuis);
  lcd.setCursor(0, 0); lcd.print(b);
}

void setup() {
  pinMode(4, OUTPUT); digitalWrite(4, LOW);      // ventilateur d'extraction arrêté
  Serial.begin(115200);
  for (uint8_t i = 0; i < 4; i++) {
    pinMode(LIGNES[i], INPUT_PULLUP);
    pinMode(COLONNES[i], INPUT_PULLUP);
  }
  Wire.begin();
  Wire.setWireTimeout(3000, true);
  uint8_t adr = 0x21;                            // adresse trouvée sur ta carte
  for (uint8_t a = 0x20; a <= 0x3F; a++) {       // recherche PCF8574 / PCF8574A
    if (a == 0x28) a = 0x38;
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) { adr = a; break; }
  }
  Serial.print(F("Ecran LCD a 0x")); Serial.println(adr, HEX);
  lcd.init(adr);
  afficherEntete();
  ligne(1, "Appuyer une touche");
  ligne(2, "S1 a S16");
  afficherVus();
  Serial.println(F("Test clavier 4x4 + LCD : appuyer sur chaque touche S1..S16"));
}

void loop() {
  uint16_t etat = lireClavier();
  uint16_t nouveaux = etat & ~precedent;        // fronts d'appui seulement
  for (uint8_t n = 1; n <= 16; n++) {
    if (nouveaux & ((uint16_t)1 << (n - 1))) {
      vus |= (uint16_t)1 << (n - 1);
      nbAppuis++;
      char b[21];
      snprintf(b, sizeof(b), "Touche S%-2u  (R%u,C%u)", n, (n - 1) / 4 + 1, (n - 1) % 4 + 1);
      ligne(1, b);
      snprintf(b, sizeof(b), "Fonction: %s", fonction(n));
      ligne(2, b);
      afficherEntete();
      afficherVus();
      Serial.print(F("Touche S")); Serial.print(n);
      Serial.print(F("  (ligne R")); Serial.print((n - 1) / 4 + 1);
      Serial.print(F(", colonne C")); Serial.print((n - 1) % 4 + 1);
      Serial.print(F(")  -> ")); Serial.println(fonction(n));
      if (vus == 0xFFFF) Serial.println(F(">>> Les 16 touches fonctionnent."));
    }
  }
  if (etat == 0 && precedent != 0 && vus == 0xFFFF) ligne(2, "16/16 touches : OK");
  precedent = etat;
  delay(30);                                     // anti-rebond simple
}
