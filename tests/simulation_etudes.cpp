/* ============================================================================
 *  SIMULATION DES ÉTUDES E1 À E4 — firmware réel + modèle physique ÉTENDU
 *
 *  Même principe que simulation_scenarios.cpp (qui reste inchangé et produit
 *  les références des scénarios 1 à 16), avec les compléments des études du
 *  mémoire (DEMANDES_SIMULATION.md) :
 *
 *    chambre      C_eq dT/dt = P + P_sol + UA f_UA(t) (T_amb - T)
 *                 défauts : UA = 1/0,098 W/K, C_eq = 3530/0,098 J/K
 *                 (identique à tau dT/dt = Kth (P + P_sol) + T_amb - T)
 *                 f_UA(t) : signal UA_facteur (ouverture de porte : x10)
 *    mesure       T_mes = T(t - retard) + Bruit_T(t), quantifiée à 0,0625 °C
 *                 si et_quantif = 1 (DS18B20) ; c'est T_mes que lit le firmware
 *    stock H2     m_H2 += P / (eta_b PCI_H2) dt quand V_H2 est ouverte ;
 *                 Press_H2 = Press_H2(scénario) x max(0, 1 - m_H2 / m0)
 *                 (m0 = 0 : stock illimité)
 *    GPL          signal GPL_dispo : 0 = bouteille vide -> pas de flamme ni de
 *                 puissance quand seule V_But est ouverte
 *
 *  Entrée : même format que simulation_scenarios.cpp, plus
 *     param et_UA <W/K> | et_Ceq <J/K> | et_retard <s> | et_quantif <0/1>
 *     param et_m0 <g> | et_eta <-> | et_PCI_H2 <kWh/kg> | et_log <s>
 *     signal UA_facteur | Bruit_T | GPL_dispo
 *  Sortie : CSV sur stdout (un point toutes les et_log s et à chaque changement)
 *     t,T_sec,T_mes,H_sec,P_gaz,P_sol,T_amb,Flame,V_Fl_1,V_Fl_2,V_Fl_3,V_H2,V_But,
 *     Etat,Spark,PWM_Ext,Press_H2,m_H2,Source
 *
 *  Compilation / exécution :  make -C tests simulation_etudes
 *                             ./simulation_etudes cas.txt > cas.csv
 * ==========================================================================*/

#include "mocks/Arduino.h"
#include "mocks/Wire.h"
#include "mocks/OneWire.h"
#include "mocks/DallasTemperature.h"
#include "mocks/DHT.h"
#include "mocks/LiquidCrystal_I2C.h"
#include "mocks/EEPROM.h"

#include "../sechoir_hybride/sechoir_hybride.ino"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <deque>
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

/* Valeur maintenue du signal à l'instant t (From Workspace sans interpolation).
 * Les points sont triés par t croissant : recherche avec un curseur par signal. */
static std::map<std::string, size_t> curseurs;
static double val(const char *nom, double t, double defaut) {
  std::map<std::string, Signal>::const_iterator it = signaux.find(nom);
  if (it == signaux.end()) return defaut;
  const Signal &s = it->second;
  size_t &k = curseurs[nom];
  while (k + 1 < s.size() && s[k + 1].first <= t + 1e-9) ++k;
  if (s.empty() || s[0].first > t + 1e-9) return defaut;
  return s[k].second;
}

static double param(const char *nom, double defaut) {
  std::map<std::string, double>::const_iterator it = params.find(nom);
  return it == params.end() ? defaut : it->second;
}

static void bouton(uint8_t pin, double v) { mockIO.entree_num[pin] = (v != 0.0) ? LOW : HIGH; }

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "usage : %s cas.txt > resultat.csv\n", argv[0]);
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

  /* Paramètres des études (défauts = modèle validé). */
  const double UA = param("et_UA", 1.0 / KTH);             // W/K
  const double C_EQ = param("et_Ceq", TAU / KTH);          // J/K
  const double RETARD = param("et_retard", 0.0);           // s
  const bool QUANTIF = param("et_quantif", 0.0) != 0.0;
  const double M0 = param("et_m0", 0.0);                   // g (0 : illimité)
  const double ETA = param("et_eta", 0.80);
  const double PCI_H2 = param("et_PCI_H2", 33.3) * 3.6e3;  // kWh/kg -> J/g
  const double PAS_LOG = param("et_log", 10.0);            // s

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

  /* Paramètres du firmware (chart : durées en s ; firmware : certaines en min). */
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

  printf("t,T_sec,T_mes,H_sec,P_gaz,P_sol,T_amb,Flame,V_Fl_1,V_Fl_2,V_Fl_3,V_H2,V_But,"
         "Etat,Spark,PWM_Ext,Press_H2,m_H2,Source\n");
  const uint32_t stop_ms = (uint32_t)(stop_s * 1000.0);
  const double dt = PAS_MS / 1000.0;
  const size_t n_retard = (size_t)(RETARD / dt + 0.5);
  std::deque<double> historique;   // T des n_retard derniers pas
  bool h2_prec = false, but_prec = false;
  double m_H2 = 0.0;
  long signature_prec = -1;
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

    /* Mesure de T_sec : retard, bruit, quantification. */
    historique.push_back(T);
    while (historique.size() > n_retard + 1) historique.pop_front();
    double T_mes = historique.front() + val("Bruit_T", t, 0);
    if (QUANTIF) T_mes = std::floor(T_mes / 0.0625 + 0.5) * 0.0625;

    /* Capteurs. */
    const double T_amb = val("T_amb", t, 25);
    const double P_sol = val("P_sol", t, 0);
    mock_dht_temp[PIN_DHT_AMB] = (float)T_amb;
    mock_dht_hum[PIN_DHT_AMB] = (float)val("H_amb", t, 35);
    double p_bar = val("Press_H2", t, 8);
    if (M0 > 0) p_bar *= std::max(0.0, 1.0 - m_H2 / M0);
    mockIO.entree_ana[PIN_PRESS_H2] =
        ADC_PRESS_4MA + (int)(p_bar / PRESS_H2_PLEINE_ECHELLE * (ADC_PRESS_20MA - ADC_PRESS_4MA) + 0.5);
    mockIO.entree_ana[PIN_MQ8_H2] = (int)val("MQ8_H2", t, 50);
    mockIO.entree_ana[PIN_MQ6_BUT] = (int)val("MQ6_But", t, 50);
    const bool gpl_dispo = val("GPL_dispo", t, 1) != 0.0;
    const bool panne = val("Flamme_panne", t, 0) != 0.0;
    const bool parasite = val("Flamme_parasite", t, 0) != 0.0;
    const bool flamme = ((h2_prec || (but_prec && gpl_dispo)) && !panne) || parasite;
    mockIO.entree_num[PIN_FLAMME] = flamme ? NIVEAU_FLAMME_PRESENTE : !NIVEAU_FLAMME_PRESENTE;
    mock_ds18b20[IDX_SONDE_T_SEC] = (float)T_mes;
    mock_dht_hum[PIN_DHT_EXTR] = (float)H;

    /* Une itération du firmware. */
    mockIO.horloge_ms = t_ms;
    loop();
    h2_prec = V_H2;
    but_prec = V_But;

    /* Modèle physique (Euler explicite). Sans GPL, V_But seule ne débite rien
     * (même règle que le bloc DISPONIBILITE_GAZ de Etudes_Sechoir.slx). */
    const double frac = (EV1 && EV2 && EV3) ? 1.0 : (EV2 && EV3) ? 0.67 : EV3 ? 0.33 : 0.0;
    const bool sans_gaz = V_But && !V_H2 && !gpl_dispo;
    const double P_gaz = sans_gaz ? 0.0 : PNOM * frac;
    if (V_H2) m_H2 += P_gaz * dt / (ETA * PCI_H2);
    const double ua = UA * val("UA_facteur", t, 1);
    const double dT = (P_gaz + P_sol + ua * (T_amb - T)) / C_EQ;
    const double dH = -K_SECHAGE * (T > T_DEBUT_SECHAGE ? T - T_DEBUT_SECHAGE : 0.0) * (H - H_EQ);
    T += dt * dT;
    H += dt * dH;

    /* Journal : toutes les PAS_LOG s et à chaque changement des sorties. */
    const long signature = (long)etat_courant * 1000 + EV1 * 100 + EV2 * 10 + EV3 +
                           V_H2 * 10000L + V_But * 20000L + flamme * 40000L +
                           Spark * 80000L + (long)Source_Active * 160000L + (long)PWM_Extract * 1000000L;
    if (signature != signature_prec || t >= prochain_log) {
      if (t >= prochain_log) prochain_log += PAS_LOG;
      signature_prec = signature;
      printf("%.2f,%.4f,%.4f,%.3f,%.0f,%.2f,%.3f,%d,%d,%d,%d,%d,%d,%d,%d,%d,%.3f,%.3f,%d\n", t, T, T_mes, H,
             P_gaz, P_sol, T_amb, (int)flamme, (int)EV1, (int)EV2, (int)EV3, (int)V_H2, (int)V_But,
             (int)etat_courant, (int)Spark, (int)PWM_Extract, p_bar, m_H2, (int)Source_Active);
    }
  }
  return 0;
}
