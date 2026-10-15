/* ============================================================================
 *  SIMULATION D'UN CYCLE COMPLET — firmware réel + modèle thermique illustratif
 *
 *  Le programme Arduino est compilé tel quel (mêmes mocks que le banc de
 *  tests). Un modèle thermique du 1er ordre, volontairement simple et
 *  ILLUSTRATIF (ce n'est pas le modèle identifié du mémoire), ferme la boucle :
 *
 *      tau * dT_sec/dt = -(T_sec - T_amb) + G * u(palier)
 *      u = 1 / 0,67 / 0,33 / 0 selon le palier, 0 si le gaz est fermé
 *
 *  L'humidité de l'air extrait décroît pendant la chauffe. La flamme est
 *  présente dès que le gaz est ouvert (brûleur idéal). Sortie : CSV sur
 *  stdout (t_min, T_sec, palier_pct, etat, H_sec, H_fin).
 *
 *  Compilation / exécution :  make -C tests simulation
 * ==========================================================================*/

#include "mocks/Arduino.h"
#include "mocks/Wire.h"
#include "mocks/OneWire.h"
#include "mocks/DallasTemperature.h"
#include "mocks/DHT.h"
#include "mocks/LiquidCrystal_I2C.h"
#include "mocks/EEPROM.h"

#include "../sechoir_hybride/sechoir_hybride.ino"

#include <cstdio>
#include <cstring>

static const float TAU_S   = 1200.0f;  // constante de temps de la chambre (s)
static const float GAIN_C  = 120.0f;   // élévation à 100 % en régime établi (°C)
static const float T_AMB_C = 25.0f;
static const uint32_t PAS_MS = 50;

static float fractionPuissance() {
  bool gaz = V_H2 || V_But;
  if (!gaz) return 0.0f;
  if (EV1 && EV2 && EV3) return 1.0f;
  if (EV2 && EV3) return 0.67f;
  if (EV3) return 0.33f;
  return 0.0f;
}

int main() {
  mockIO.reinitialiser();
  memset(EEPROM.donnees, 0xFF, sizeof(EEPROM.donnees));
  mockIO.entree_num[PIN_AU_URGENCE] = LOW;
  mockIO.entree_num[PIN_FLAMME] = !NIVEAU_FLAMME_PRESENTE;
  mockIO.entree_ana[PIN_MQ8_H2] = 50;
  mockIO.entree_ana[PIN_MQ6_BUT] = 50;
  mockIO.entree_ana[PIN_PRESS_H2] = ADC_PRESS_4MA + 700;   // ~8 bar : H2 disponible
  float T = T_AMB_C;
  mock_ds18b20[IDX_SONDE_T_SEC] = T;
  mock_ds18b20[IDX_SONDE_T_CAP] = T;
  mock_dht_temp[PIN_DHT_AMB] = T_AMB_C;
  mock_dht_hum[PIN_DHT_AMB] = 35.0f;       // H_amb -> H_fin = 10 + 35 = 45 %
  mock_dht_hum[PIN_DHT_EXTR] = 90.0f;

  setup();
  Config.Mode_Auto = true;                 // arbitrage automatique
  Config.T_cible = 55.0f;
  Config.Hhyst = 4.0f;
  Config.Temps_Min_Fin = 60UL;             // 1 h
  Config.Duree_Max_Cycle = 600UL;
  Config.Temps_Reponse = 300UL;            // 5 min sans réponse -> fin
  Config.DeltaT_Sol = 20.0f;               // T_cap estimée = 45 < 55 : pas de solaire

  printf("t_min,T_sec,palier_pct,etat,H_sec,H_fin\n");
  uint32_t t_ms = 0;
  bool demarre = false;
  const uint32_t duree_ms = 150UL * 60000UL;
  uint32_t prochain_log = 0;
  while (t_ms < duree_ms) {
    /* Appui sur START à t = 10 s. */
    if (!demarre && t_ms >= 10000) {
      mockIO.entree_num[PIN_BTN_START] = LOW;
      if (t_ms >= 10100) { mockIO.entree_num[PIN_BTN_START] = HIGH; demarre = true; }
    }
    /* Modèle thermique (Euler explicite). */
    float u = fractionPuissance();
    T += (PAS_MS / 1000.0f) * (-(T - T_AMB_C) + GAIN_C * u) / TAU_S;
    mock_ds18b20[IDX_SONDE_T_SEC] = T;
    /* Brûleur idéal : flamme dès que le gaz est ouvert. */
    mockIO.entree_num[PIN_FLAMME] = (V_H2 || V_But) ? NIVEAU_FLAMME_PRESENTE : !NIVEAU_FLAMME_PRESENTE;
    /* Séchage : l'humidité de l'air extrait décroît quand la chambre est chaude. */
    float h = mock_dht_hum[PIN_DHT_EXTR];
    float vitesse = (T > 35.0f) ? (T - 35.0f) * 0.0000008f : 0.0f;   // %/ms, illustratif
    h -= vitesse * PAS_MS * (h - 20.0f) / 70.0f * 1.0f;
    mock_dht_hum[PIN_DHT_EXTR] = h;

    mockIO.horloge_ms += PAS_MS;
    t_ms += PAS_MS;
    loop();

    if (t_ms >= prochain_log) {
      prochain_log += 10000;   // un point toutes les 10 s
      int pct = 0;
      if (phase_combustion != PH_PURGE && (V_H2 || V_But)) {
        pct = (palier == PALIER_100) ? 100 : (palier == PALIER_67) ? 67 : (palier == PALIER_33) ? 33 : 0;
      }
      printf("%.2f,%.2f,%d,%s,%.1f,%.1f\n", t_ms / 60000.0f, T_sec, pct,
             nomEtat(etat_courant), H_sec, H_fin);
    }
  }
  return 0;
}
