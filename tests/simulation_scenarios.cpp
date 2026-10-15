/* ============================================================================
 *  SIMULATION DE RÉFÉRENCE D'UN SCÉNARIO — firmware réel + MÊME modèle
 *  physique que le modèle Simulink (matlab/preparer_simulation.m)
 *
 *  Sert à produire les courbes ATTENDUES de chaque scénario de
 *  matlab/scenario_sechoir.m, pour les comparer à la simulation Simulink
 *  (validation croisée firmware <-> Stateflow, matlab/comparer_scenario.m).
 *
 *  Modèle physique (identique à Simulink) :
 *    puissance gaz  P = Pnom x (1 ; 0,67 ; 0,33 ; 0) selon EV1..EV3 (Pnom = 5000 W)
 *    chambre        tau dT/dt = Kth (P + P_sol) + T_amb - T   (Kth = 0,098 ; tau = 3530 s)
 *    séchage        dH/dt = -K max(T - 35, 0) (H - 20)          (K = 1,14e-5)
 *    brûleur        flamme = gaz ouvert au pas précédent ET pas de panne, OU parasite
 *
 *  Entrée : fichier texte écrit par matlab/exporter_scenario.m
 *     StopTime <s> | Tsec0 <°C> | H0 <%> | param <Nom> <valeur> | signal <Nom> <t> <valeur>
 *  Sortie : CSV sur stdout (un point toutes les 10 s et à chaque changement)
 *     t,T_sec,H_sec,P_gaz,Flame,V_Fl_1,V_Fl_2,V_Fl_3,V_H2,V_But,Etat
 *
 *  Compilation / exécution :  make -C tests reference   (Octave requis)
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
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

static const double PNOM = 5000.0, KTH = 0.098, TAU = 3530.0;
static const double K_SECHAGE = 1.14e-5, T_DEBUT_SECHAGE = 35.0, H_EQ = 20.0;
static const uint32_t PAS_MS = 50;

typedef std::vector<std::pair<double, double> > Signal;
static std::map<std::string, Signal> signaux;
static std::map<std::string, double> params;

/* Valeur maintenue du signal à l'instant t (même règle que From Workspace
 * sans interpolation). */
static double val(const char *nom, double t, double defaut) {
  std::map<std::string, Signal>::const_iterator it = signaux.find(nom);
  if (it == signaux.end()) return defaut;
  double v = defaut;
  for (size_t i = 0; i < it->second.size(); ++i) {
    if (it->second[i].first <= t + 1e-9) v = it->second[i].second;
  }
  return v;
}

static double param(const char *nom, double defaut) {
  std::map<std::string, double>::const_iterator it = params.find(nom);
  return it == params.end() ? defaut : it->second;
}

static void bouton(uint8_t pin, double v) { mockIO.entree_num[pin] = (v != 0.0) ? LOW : HIGH; }

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "usage : %s scenario.txt > resultat.csv\n", argv[0]);
    return 1;
  }
  double stop_s = 3600, T = 25, H = 90;
  std::ifstream f(argv[1]);
  if (!f) { fprintf(stderr, "fichier introuvable : %s\n", argv[1]); return 1; }
  std::string ligne;
  while (std::getline(f, ligne)) {
    std::istringstream is(ligne);
    std::string cle, nom;
    is >> cle;
    if (cle == "StopTime") is >> stop_s;
    else if (cle == "Tsec0") is >> T;
    else if (cle == "H0") is >> H;
    else if (cle == "param") { double v; is >> nom >> v; params[nom] = v; }
    else if (cle == "signal") { double t, v; is >> nom >> t >> v; signaux[nom].push_back(std::make_pair(t, v)); }
  }

  mockIO.reinitialiser();
  memset(EEPROM.donnees, 0xFF, sizeof(EEPROM.donnees));
  mockIO.entree_num[PIN_AU_URGENCE] = LOW;
  mockIO.entree_num[PIN_FLAMME] = !NIVEAU_FLAMME_PRESENTE;
  const uint8_t boutons[] = {PIN_BTN_MENU, PIN_BTN_UP, PIN_BTN_DOWN, PIN_BTN_OK,
                             PIN_BTN_START, PIN_BTN_STOP, PIN_BTN_REARM};
  for (size_t i = 0; i < sizeof(boutons); ++i) mockIO.entree_num[boutons[i]] = HIGH;
  mock_ds18b20[IDX_SONDE_T_SEC] = (float)T;
  mock_ds18b20[IDX_SONDE_T_CAP] = (float)T;
  mock_dht_temp[PIN_DHT_AMB] = (float)val("T_amb", 0, 25);
  mock_dht_hum[PIN_DHT_AMB] = (float)val("H_amb", 0, 35);
  mock_dht_hum[PIN_DHT_EXTR] = (float)H;
  mockIO.entree_ana[PIN_MQ8_H2] = (int)val("MQ8_H2", 0, 50);
  mockIO.entree_ana[PIN_MQ6_BUT] = (int)val("MQ6_But", 0, 50);

  setup();

  /* Paramètres (chart : durées en s ; firmware : certaines en min). */
  Config.Hhyst = (float)param("Hhyst", Config.Hhyst);
  Config.Temps_Min_Fin = (uint32_t)(param("Temps_Min_Fin", Config.Temps_Min_Fin * 60.0) / 60.0);
  Config.Duree_Max_Cycle = (uint32_t)(param("Duree_Max_Cycle", Config.Duree_Max_Cycle * 60.0) / 60.0);
  Config.Temps_Reponse = (uint32_t)param("Temps_Reponse", Config.Temps_Reponse);
  Config.Temps_Purge = (uint32_t)param("Temps_Purge", Config.Temps_Purge);
  Config.Temps_Allumage = (uint32_t)param("Temps_Allumage", Config.Temps_Allumage);
  Config.Periode_Regul = (uint32_t)param("Periode_Regul", Config.Periode_Regul);
  Config.Regul_Auto = param("Regul_Auto", Config.Regul_Auto) != 0.0;
  Config.Palier_Impose = (uint8_t)param("Palier_Impose", Config.Palier_Impose);
  Config.DeltaT_Sol = (float)param("DeltaT_Sol", Config.DeltaT_Sol);
  Config.Marge_Sol = (float)param("Marge_Sol", Config.Marge_Sol);
  Config.Hyst_Sol = (float)param("Hyst_Sol", Config.Hyst_Sol);
  Config.Seuil_Chaud = (float)param("Seuil_Chaud", Config.Seuil_Chaud);
  Config.Fixe_H_sec = param("Fixe_H_sec", Config.Fixe_H_sec) != 0.0;
  Config.Val_H_sec = (float)param("Val_H_sec", Config.Val_H_sec);
  if (param("T_SEC_MAX_SECURITE", T_SEC_MAX_SECURITE) != T_SEC_MAX_SECURITE) {
    fprintf(stderr, "T_SEC_MAX_SECURITE est une constante du firmware (%.0f) : parametre ignore\n",
            (double)T_SEC_MAX_SECURITE);
  }

  printf("t,T_sec,H_sec,P_gaz,Flame,V_Fl_1,V_Fl_2,V_Fl_3,V_H2,V_But,Etat\n");
  const uint32_t stop_ms = (uint32_t)(stop_s * 1000.0);
  bool gaz_prec = false;
  int signature_prec = -1;
  double prochain_log = 0;
  for (uint32_t t_ms = 0; t_ms <= stop_ms; t_ms += PAS_MS) {
    const double t = t_ms / 1000.0;

    /* Consignes et boutons du scénario. */
    Config.Mode_Auto = val("Mode_Auto", t, 1) != 0.0;
    Config.Choix_Mode = (uint8_t)val("Choix_Manuel", t, 2);
    Config.T_cible = (float)val("T_cible", t, 55);
    Config.H_produit_cible = (float)val("H_produit", t, 10);
    Config.Prolong_Defaut = (uint32_t)(val("Tps_Prolongation", t, 1800) / 60.0);
    bouton(PIN_BTN_START, val("Btn_Start", t, 0));
    bouton(PIN_BTN_STOP, val("Btn_Stop", t, 0));
    bouton(PIN_BTN_OK, val("Btn_OK", t, 0));
    bouton(PIN_BTN_UP, val("Btn_UP", t, 0));
    bouton(PIN_BTN_DOWN, val("Btn_DOWN", t, 0));
    bouton(PIN_BTN_MENU, val("Btn_SELECT", t, 0));
    bouton(PIN_BTN_REARM, val("Btn_Rearm", t, 0));
    mockIO.entree_num[PIN_AU_URGENCE] = val("AU_Manuel", t, 0) != 0.0 ? HIGH : LOW;

    /* Capteurs. */
    const double T_amb = val("T_amb", t, 25);
    mock_dht_temp[PIN_DHT_AMB] = (float)T_amb;
    mock_dht_hum[PIN_DHT_AMB] = (float)val("H_amb", t, 35);
    const double p_bar = val("Press_H2", t, 8);
    mockIO.entree_ana[PIN_PRESS_H2] =
        ADC_PRESS_4MA + (int)(p_bar / PRESS_H2_PLEINE_ECHELLE * (ADC_PRESS_20MA - ADC_PRESS_4MA) + 0.5);
    mockIO.entree_ana[PIN_MQ8_H2] = (int)val("MQ8_H2", t, 50);
    mockIO.entree_ana[PIN_MQ6_BUT] = (int)val("MQ6_But", t, 50);
    const bool panne = val("Flamme_panne", t, 0) != 0.0;
    const bool parasite = val("Flamme_parasite", t, 0) != 0.0;
    const bool flamme = (gaz_prec && !panne) || parasite;
    mockIO.entree_num[PIN_FLAMME] = flamme ? NIVEAU_FLAMME_PRESENTE : !NIVEAU_FLAMME_PRESENTE;
    mock_ds18b20[IDX_SONDE_T_SEC] = (float)T;
    mock_dht_hum[PIN_DHT_EXTR] = (float)H;

    /* Une itération du firmware. */
    mockIO.horloge_ms = t_ms;
    loop();
    gaz_prec = V_H2 || V_But;

    /* Modèle physique (Euler explicite). */
    const double frac = (EV1 && EV2 && EV3) ? 1.0 : (EV2 && EV3) ? 0.67 : EV3 ? 0.33 : 0.0;
    const double P_gaz = PNOM * frac;
    const double dt = PAS_MS / 1000.0;
    const double dT = (KTH * (P_gaz + val("P_sol", t, 0)) + T_amb - T) / TAU;
    const double dH = -K_SECHAGE * (T > T_DEBUT_SECHAGE ? T - T_DEBUT_SECHAGE : 0.0) * (H - H_EQ);
    T += dt * dT;
    H += dt * dH;

    /* Journal : toutes les 10 s et à chaque changement des sorties. */
    const int signature = (int)etat_courant * 1000 + EV1 * 100 + EV2 * 10 + EV3 +
                          V_H2 * 10000 + V_But * 20000 + flamme * 40000;
    if (signature != signature_prec || t >= prochain_log) {
      if (t >= prochain_log) prochain_log += 10.0;
      signature_prec = signature;
      printf("%.2f,%.3f,%.3f,%.0f,%d,%d,%d,%d,%d,%d,%d\n", t, T, H, P_gaz, (int)flamme,
             (int)EV1, (int)EV2, (int)EV3, (int)V_H2, (int)V_But, (int)etat_courant);
    }
  }
  return 0;
}
