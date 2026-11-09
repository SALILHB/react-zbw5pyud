/* =====================================================================
 *  TEST ÉCRAN LCD I2C + VENTILATEUR D'EXTRACTION (D4) — Arduino Mega 2560
 *  Sans clavier. Séquence automatique de vitesses, affichée sur le LCD.
 *  Aucune bibliothèque à installer (pilote LCD intégré, Wire seulement).
 *
 *  Câblage :
 *   - LCD 20x4 I2C (adresse cherchée automatiquement, 0x21 sur ta carte) : SDA -> D20, SCL -> D21, VCC -> 5 V, GND -> GND
 *   - D4 -> 1 kΩ -> EL817 broche 1 ; EL817 broche 2 -> GND Arduino
 *     (étage EL817 -> diode Db -> grille IRF510 de l'extraction)
 *   - Masse Arduino reliée à la masse 12 V.
 *   - Interrupteur MANUEL de l'extraction sur ARRÊT (sinon le NE555
 *     impose sa propre vitesse par le OU à diodes).
 *
 *  Séquence (en boucle) : 0 % -> 25 % -> 50 % -> 75 % -> 100 % -> 50 % -> 0 %
 *  Chaque palier dure DUREE_PALIER_S secondes.
 *
 *  Moniteur série (115200 bauds), facultatif :
 *   - taper un nombre 0..100 puis Entrée  -> vitesse fixe en %
 *   - taper  a  puis Entrée               -> retour à la séquence auto
 * ===================================================================*/
#include <Wire.h>

/* ---- Pilote LCD I2C minimal (module PCF8574) : aucune bibliothèque à installer ----
 * Brochage standard du module : P0=RS, P1=RW, P2=EN, P3=rétroéclairage, P4..P7=D4..D7 */
class LcdI2C {
  uint8_t adr, bl = 0x08;
  void ecrire(uint8_t v) { Wire.beginTransmission(adr); Wire.write(v | bl); Wire.endTransmission(); }
  void impulsion(uint8_t v) { ecrire(v | 0x04); delayMicroseconds(1); ecrire(v & ~0x04); delayMicroseconds(50); }
  void quartet(uint8_t v) { ecrire(v); impulsion(v); }
  void envoyer(uint8_t val, uint8_t mode) { quartet((val & 0xF0) | mode); quartet(((val << 4) & 0xF0) | mode); }
  void commande(uint8_t c) { envoyer(c, 0); }
public:
  LcdI2C(uint8_t a, uint8_t, uint8_t) : adr(a) {}
  void setAdresse(uint8_t a) { adr = a; }
  void init() {
    Wire.begin(); delay(50); ecrire(0); delay(100);
    quartet(0x30); delayMicroseconds(4500); quartet(0x30); delayMicroseconds(4500);
    quartet(0x30); delayMicroseconds(150);  quartet(0x20);
    commande(0x28); commande(0x0C); clear(); commande(0x06);
  }
  void backlight() { bl = 0x08; ecrire(0); }
  void clear() { commande(0x01); delay(2); }
  void setCursor(uint8_t c, uint8_t l) { static const uint8_t o[4] = {0x00, 0x40, 0x14, 0x54}; commande(0x80 | (c + o[l & 3])); }
  void print(char c) { envoyer((uint8_t)c, 0x01); }
  void print(const char* t) { while (*t) print(*t++); }
  void print(const __FlashStringHelper* f) { PGM_P p = reinterpret_cast<PGM_P>(f); char c; while ((c = pgm_read_byte(p++))) print(c); }
};

const uint8_t  LCD_ADRESSE    = 0x21;   // adresse trouvée sur ta carte (scan I2C) ; recherche auto au démarrage
const uint8_t  BROCHE_EXTR    = 4;      // sortie PWM de l'extraction
const uint16_t DUREE_PALIER_S = 10;     // durée de chaque vitesse
const uint16_t KICK_MS        = 400;    // impulsion à 100 % au démarrage

const uint8_t PALIERS[] = {0, 25, 50, 75, 100, 50};
const uint8_t NB_PALIERS = sizeof(PALIERS);

LcdI2C lcd(LCD_ADRESSE, 20, 4);

uint8_t  indice      = 0;
uint8_t  vitesse     = 0;       // % appliqué
bool     modeAuto    = true;
uint32_t debutPalier = 0;

void appliquer(uint8_t pct) {
  pct = constrain(pct, 0, 100);
  if (pct > 0 && vitesse == 0) {          // aide au démarrage du moteur
    analogWrite(BROCHE_EXTR, 255);
    delay(KICK_MS);
  }
  vitesse = pct;
  analogWrite(BROCHE_EXTR, map(pct, 0, 100, 0, 255));
}

void barre(uint8_t pct) {                 // barre de 20 caractères, ligne 3
  lcd.setCursor(0, 3);
  uint8_t n = (pct * 20 + 50) / 100;
  for (uint8_t i = 0; i < 20; i++) lcd.print(i < n ? (char)255 : '-');
}

void afficher() {
  char l[21];
  lcd.setCursor(0, 0); lcd.print(F("TEST EXTRACTION D4  "));
  snprintf(l, sizeof(l), "Vitesse : %3u %%     ", vitesse);
  lcd.setCursor(0, 1); lcd.print(l);
  if (modeAuto) {
    uint32_t ecoule = (millis() - debutPalier) / 1000UL;
    uint16_t reste  = ecoule < DUREE_PALIER_S ? DUREE_PALIER_S - ecoule : 0;
    snprintf(l, sizeof(l), "Auto %u/%u  reste %2us", indice + 1, NB_PALIERS, reste);
  } else {
    snprintf(l, sizeof(l), "Manuel (serie)      ");
  }
  lcd.setCursor(0, 2); lcd.print(l);
  barre(vitesse);
}

void lireSerie() {
  if (!Serial.available()) return;
  String s = Serial.readStringUntil('\n'); s.trim();
  if (s.length() == 0) return;
  if (s[0] == 'a' || s[0] == 'A') {
    modeAuto = true; indice = 0; debutPalier = millis();
    appliquer(PALIERS[indice]);
    Serial.println(F("Mode auto"));
  } else {
    int v = s.toInt();
    if (v >= 0 && v <= 100) {
      modeAuto = false; appliquer(v);
      Serial.print(F("Vitesse fixe : ")); Serial.print(v); Serial.println(F(" %"));
    }
  }
}

void setup() {
  pinMode(BROCHE_EXTR, OUTPUT);
  analogWrite(BROCHE_EXTR, 0);            // ventilateur arrêté au démarrage
  Serial.begin(115200);
  Serial.setTimeout(50);
  Wire.begin();
  Wire.setWireTimeout(3000, true);
  for (uint8_t a = 0x20; a <= 0x3F; a++) {          // recherche de l'écran (PCF8574 / PCF8574A)
    if (a == 0x28) a = 0x38;
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) { lcd.setAdresse(a); Serial.print(F("Ecran trouve a 0x")); Serial.println(a, HEX); break; }
  }
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(F("Sechoir hybride"));
  lcd.setCursor(0, 1); lcd.print(F("Test LCD + ventil."));
  lcd.setCursor(0, 2); lcd.print(F("Extraction sur D4"));
  delay(2000);
  lcd.clear();
  debutPalier = millis();
  appliquer(PALIERS[0]);
  Serial.println(F("Test extraction D4 : 0..100 + Entree = vitesse fixe, a = auto"));
}

void loop() {
  lireSerie();
  if (modeAuto && millis() - debutPalier >= DUREE_PALIER_S * 1000UL) {
    indice = (indice + 1) % NB_PALIERS;
    debutPalier = millis();
    appliquer(PALIERS[indice]);
    Serial.print(F("Palier ")); Serial.print(indice + 1);
    Serial.print(F(" : ")); Serial.print(vitesse); Serial.println(F(" %"));
  }
  static uint32_t dernier = 0;
  if (millis() - dernier >= 250) { dernier = millis(); afficher(); }
}
