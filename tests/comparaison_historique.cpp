/* ============================================================================
 *  COMPARAISON D'HISTORIQUE — programme d'origine / programme avec liaison
 *
 *  Un même scénario de 90 min (démarrage, régulation et veilles, T_cible
 *  modifiée, bascule H2 -> GPL, perte de flamme et échecs d'allumage,
 *  ERREUR_COMBUSTION et reprise, retour H2, demande et prolongation, fin,
 *  urgence fuite H2 et réarmement, arrêt) est joué par trois programmes :
 *
 *    -DVERSION_ORIGINE                     copie figée du programme validé,
 *                                          conduite par les boutons
 *    -DAVEC_PUPITRE_LCD=1                  programme avec liaison, boutons
 *    -DAVEC_PUPITRE_LCD=0 -DPAR_COMMANDES  sans pupitre, commandes à distance
 *
 *  Chaque version écrit l'historique (instant, état, source, phase, palier) ;
 *  `make comparaison` vérifie que les trois historiques sont identiques.
 * ==========================================================================*/

#include "mocks/Arduino.h"
#include "mocks/Wire.h"
#include "mocks/OneWire.h"
#include "mocks/DallasTemperature.h"
#include "mocks/DHT.h"
#include "mocks/LiquidCrystal_I2C.h"
#include "mocks/EEPROM.h"

#ifdef VERSION_ORIGINE
#include "reference_origine/sechoir_hybride.ino"
#else
#include "../sechoir_hybride/sechoir_hybride.ino"
#endif

#include <cstdio>
#include <string>

/* --- Modèle de procédé simple : brûleur 5 kW, chambre du 1er ordre --- */
static float    T_chambre    = 25.0f;
static bool     allumeur_ok  = true;
static const float P_NOM = 5000.0f, UA = 10.2f, C_EQ = 3.6e4f, T_AMB = 25.0f;

static int dernier_etat = -1, derniere_source = -1, derniere_phase = -1, dernier_palier = -1;

static void enregistrer() {
  if ((int)etat_courant != dernier_etat || (int)Source_Active != derniere_source ||
      (int)phase_combustion != derniere_phase || (int)palier != dernier_palier) {
    printf("%9.2f  etat=%d source=%d phase=%d palier=%d\n", mockIO.horloge_ms / 1000.0,
           (int)etat_courant, (int)Source_Active, (int)phase_combustion, (int)palier);
    dernier_etat = etat_courant;  derniere_source = Source_Active;
    derniere_phase = phase_combustion;  dernier_palier = palier;
  }
}

static void pas() {
  bool gaz = mockIO.sortie_num[PIN_V_H2] == HIGH || mockIO.sortie_num[PIN_V_BUT] == HIGH;
  bool flamme = gaz && allumeur_ok;
  mockIO.entree_num[PIN_FLAMME] = flamme ? NIVEAU_FLAMME_PRESENTE : !NIVEAU_FLAMME_PRESENTE;
  int nb_ev = (mockIO.sortie_num[PIN_EV1] == HIGH) + (mockIO.sortie_num[PIN_EV2] == HIGH) +
              (mockIO.sortie_num[PIN_EV3] == HIGH);
  float P = flamme ? P_NOM * nb_ev / 3.0f : 0.0f;
  T_chambre += 0.02f * (P - UA * (T_chambre - T_AMB)) / C_EQ;
  mock_ds18b20[IDX_SONDE_T_SEC] = T_chambre;
  mockIO.horloge_ms += 20;
  loop();
  enregistrer();
}

static void attendre(uint32_t ms) { for (uint32_t t = 0; t < ms; t += 20) pas(); }
static void attendreMin(float minutes) { attendre((uint32_t)(minutes * 60000.0f)); }

enum Action { A_START, A_STOP, A_PROLONG_OK, A_TERMINER, A_ERREUR_REESSAYER, A_REARM, A_TCIBLE_PLUS };

#ifdef PAR_COMMANDES
static void agir(Action a) {
  char ligne[40];
  switch (a) {
    case A_START:            strcpy(ligne, "START"); break;
    case A_STOP:             strcpy(ligne, "STOP"); break;
    case A_PROLONG_OK:       strcpy(ligne, "PROLONG OK"); break;
    case A_TERMINER:         strcpy(ligne, "TERMINER"); break;
    case A_ERREUR_REESSAYER: strcpy(ligne, "ERREUR REESSAYER"); break;
    case A_REARM:            strcpy(ligne, "REARM"); break;
    case A_TCIBLE_PLUS:      snprintf(ligne, sizeof(ligne), "SET t_cible %.1f", Config.T_cible + 1.0f); break;
  }
  Serial.injecter(std::string(ligne) + "\n");
  attendre(120);                 // même durée qu'un appui (60 ms + 60 ms)
}
#else
static void agir(Action a) {
  uint8_t pin = PIN_BTN_START;
  switch (a) {
    case A_START:            pin = PIN_BTN_START; break;
    case A_STOP:             pin = PIN_BTN_STOP;  break;
    case A_PROLONG_OK:       pin = PIN_BTN_OK;    break;
    case A_TERMINER:         pin = PIN_BTN_STOP;  break;
    case A_ERREUR_REESSAYER: pin = PIN_BTN_OK;    break;   // choix par défaut : REESSAYER
    case A_REARM:            pin = PIN_BTN_REARM; break;
    case A_TCIBLE_PLUS:      pin = PIN_BTN_UP;    break;
  }
  mockIO.entree_num[pin] = LOW;
  attendre(60);
  mockIO.entree_num[pin] = HIGH;
  attendre(60);
}
#endif

static int adcPression(float bar) {
  return ADC_PRESS_4MA + (int)(bar / PRESS_H2_PLEINE_ECHELLE * (float)(ADC_PRESS_20MA - ADC_PRESS_4MA));
}

int main() {
  mockIO.reinitialiser();
  memset(EEPROM.donnees, 0xFF, sizeof(EEPROM.donnees));
  mockIO.entree_num[PIN_AU_URGENCE] = LOW;
  mockIO.entree_ana[PIN_MQ8_H2] = 50;
  mockIO.entree_ana[PIN_MQ6_BUT] = 50;
  mockIO.entree_ana[PIN_PRESS_H2] = adcPression(8.0f);
  mock_ds18b20[IDX_SONDE_T_SEC] = T_chambre;
  mock_ds18b20[IDX_SONDE_T_CAP] = 20.0f;
  mock_dht_temp[PIN_DHT_AMB] = T_AMB;          // T_cap estimée 45 °C : pas de solaire
  mock_dht_hum[PIN_DHT_AMB] = 35.0f;
  mock_dht_hum[PIN_DHT_EXTR] = 90.0f;
  setup();
  Config.Mode_Auto = true;
  Config.Duree_Max_Cycle = 45UL;
  enregistrer();

  attendre(1000);
  agir(A_START);                                  // H2, purge, allumage, paliers
  attendreMin(10);
  agir(A_TCIBLE_PLUS);                            // T_cible 55 -> 56 en cycle
  attendreMin(10);
  mockIO.entree_ana[PIN_PRESS_H2] = adcPression(1.0f);   // réservoir H2 vide -> GPL
  attendreMin(5);
  allumeur_ok = false;                            // perte de flamme, échecs -> ERREUR
  attendreMin(10);
  allumeur_ok = true;
  agir(A_ERREUR_REESSAYER);
  attendreMin(3);
  mockIO.entree_ana[PIN_PRESS_H2] = adcPression(8.0f);   // pression rétablie -> H2
  attendreMin(9);                                 // durée max 45 min -> DEMANDE
  attendre(20000);
  agir(A_PROLONG_OK);                             // PROLONGATION 30 min -> DEMANDE
  attendreMin(31);
  agir(A_TERMINER);                               // SECHAGE_TERMINE -> ATTENTE (5 min)
  attendreMin(6);
  agir(A_START);
  attendreMin(3);
  mockIO.entree_ana[PIN_MQ8_H2] = 600;            // fuite H2 -> URGENCE
  attendre(10000);
  mockIO.entree_ana[PIN_MQ8_H2] = 50;
  attendre(10000);
  agir(A_REARM);                                  // -> ATTENTE
  attendre(2000);
  agir(A_START);
  attendreMin(3);
  agir(A_STOP);
  attendreMin(1);
  return 0;
}
