#include "Arduino.h"
#include "DallasTemperature.h"
#include "DHT.h"
#include "LiquidCrystal_I2C.h"
#include "Wire.h"
#include "EEPROM.h"

MockArduino  mockIO;
SerieMock    Serial;
TwoWire      Wire;
EEPROMClass  EEPROM;

float       mock_ds18b20[NB_SONDES_MOCK];
float       mock_dht_temp[NB_BROCHES_MOCK];
float       mock_dht_hum[NB_BROCHES_MOCK];
std::string mock_lcd_lignes[NB_LIGNES_LCD_MOCK];

void MockArduino::reinitialiser() {
  horloge_ms = 0;
  for (uint8_t i = 0; i < NB_BROCHES_MOCK; i++) {
    entree_num[i] = HIGH;     // repos des entrées à contact vers 0 V
    sortie_num[i] = LOW;
    entree_ana[i] = 0;
    sortie_pwm[i] = 0;
    mode[i] = INPUT;
  }
}

static bool brocheValide(uint8_t pin) { return pin < NB_BROCHES_MOCK; }

void pinMode(uint8_t pin, uint8_t mode) {
  if (brocheValide(pin)) mockIO.mode[pin] = mode;
}

void digitalWrite(uint8_t pin, int valeur) {
  if (brocheValide(pin)) mockIO.sortie_num[pin] = valeur;
}

int digitalRead(uint8_t pin) {
  return brocheValide(pin) ? mockIO.entree_num[pin] : LOW;
}

void analogWrite(uint8_t pin, int valeur) {
  if (brocheValide(pin)) mockIO.sortie_pwm[pin] = valeur;
}

int analogRead(uint8_t pin) {
  return brocheValide(pin) ? mockIO.entree_ana[pin] : 0;
}

uint32_t millis() { return mockIO.horloge_ms; }
uint32_t micros() { return mockIO.horloge_ms * 1000UL; }

void delay(uint32_t ms) { mockIO.horloge_ms += ms; }
void delayMicroseconds(uint32_t us) { mockIO.horloge_ms += us / 1000UL; }

/* --- DallasTemperature --- */
DallasTemperature::DallasTemperature(OneWire*) {}
void  DallasTemperature::begin() {}
void  DallasTemperature::setResolution(uint8_t) {}
void  DallasTemperature::setWaitForConversion(bool) {}
void  DallasTemperature::requestTemperatures() {}
uint8_t DallasTemperature::getDeviceCount() { return NB_SONDES_MOCK; }
float DallasTemperature::getTempCByIndex(uint8_t i) {
  return (i < NB_SONDES_MOCK) ? mock_ds18b20[i] : DEVICE_DISCONNECTED_C;
}

/* --- DHT --- */
DHT::DHT(uint8_t pin, uint8_t) : pin_(pin) {}
void  DHT::begin() {}
float DHT::readTemperature() {
  return brocheValide(pin_) ? mock_dht_temp[pin_] : NAN;
}
float DHT::readHumidity() {
  return brocheValide(pin_) ? mock_dht_hum[pin_] : NAN;
}

/* --- LiquidCrystal_I2C --- */
LiquidCrystal_I2C::LiquidCrystal_I2C(uint8_t, uint8_t, uint8_t)
  : colonne_(0), ligne_(0) {}
void LiquidCrystal_I2C::init() {}
void LiquidCrystal_I2C::begin() {}
void LiquidCrystal_I2C::backlight() {}
void LiquidCrystal_I2C::noBacklight() {}
void LiquidCrystal_I2C::clear() {
  for (uint8_t i = 0; i < NB_LIGNES_LCD_MOCK; i++) mock_lcd_lignes[i].clear();
  colonne_ = 0;
  ligne_ = 0;
}
void LiquidCrystal_I2C::setCursor(uint8_t col, uint8_t ligne) {
  colonne_ = col;
  ligne_ = (ligne < NB_LIGNES_LCD_MOCK) ? ligne : (NB_LIGNES_LCD_MOCK - 1);
}
void LiquidCrystal_I2C::print(const char* texte) {
  std::string& cible = mock_lcd_lignes[ligne_];
  if (cible.size() < colonne_) cible.resize(colonne_, ' ');
  cible.replace(colonne_, std::string::npos, texte);
  colonne_ += (uint8_t)strlen(texte);
}
