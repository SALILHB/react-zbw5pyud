/* ============================================================================
 *  BANC DE TESTS — Liaison interface opérateur (USB / ESP32)
 *
 *  Le programme Arduino est compilé tel quel sur PC avec les mocks. Les
 *  commandes sont déposées dans Serial (USB) ou Serial1 (ESP32) ; on vérifie
 *  l'état de la machine et les accusés de réception émis.
 *
 *    make -C tests liaison          (AVEC_PUPITRE_LCD 0 puis 1)
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
#include <string>

/* --------------------------------------------------------------------------
 *  Micro-cadre de test
 * ------------------------------------------------------------------------*/
static int nb_verif = 0;
static int nb_echecs = 0;

static void verifier(bool condition, const char* libelle) {
  nb_verif++;
  if (!condition) {
    nb_echecs++;
    printf("    [ECHEC] %s\n", libelle);
  }
}

static void ouvrirCas(const char* nom) { printf("  %s\n", nom); }

/* --------------------------------------------------------------------------
 *  Pilotage
 * ------------------------------------------------------------------------*/
static void avancer(uint32_t duree_ms, uint32_t pas = 20) {
  uint32_t reste = duree_ms;
  while (reste > 0) {
    uint32_t d = (reste < pas) ? reste : pas;
    mockIO.horloge_ms += d;
    reste -= d;
    loop();
  }
}

static int adcPression(float bar) {
  if (bar <= 0.0f) return ADC_PRESS_4MA;
  return ADC_PRESS_4MA + (int)(bar / PRESS_H2_PLEINE_ECHELLE *
                               (float)(ADC_PRESS_20MA - ADC_PRESS_4MA));
}
static void reglerTsec(float v)  { mock_ds18b20[IDX_SONDE_T_SEC] = v; }
static void reglerTamb(float v)  { mock_dht_temp[PIN_DHT_AMB] = v; }
static void reglerFlamme(bool presente) {
  mockIO.entree_num[PIN_FLAMME] = presente ? NIVEAU_FLAMME_PRESENTE : !NIVEAU_FLAMME_PRESENTE;
}
static uint32_t purgeMs()    { return Config.Temps_Purge * 1000UL; }
static uint32_t allumageMs() { return Config.Temps_Allumage * 1000UL; }

/* Envoie une commande comme le ferait la page, puis laisse tourner 120 ms
 * (même durée qu'un appui de bouton au banc : 60 ms appuyé + 60 ms relâché). */
static void envoyer(const char* ligne, SerieMock& port = Serial) {
  port.injecter(std::string(ligne) + "\n");
  avancer(120);
}

#if AVEC_PUPITRE_LCD
static void appuyer(uint8_t pin) {
  mockIO.entree_num[pin] = LOW;
  avancer(60);
  mockIO.entree_num[pin] = HIGH;
  avancer(60);
}
#endif

/* Dernier accusé de réception d'une commande, lu dans les lignes émises. */
struct Ack { bool trouve; bool ok; std::string val; std::string raison; };

static std::string champJson(const std::string& l, const char* cle) {
  std::string motif = std::string("\"") + cle + "\":\"";
  size_t p = l.find(motif);
  if (p == std::string::npos) return "";
  p += motif.size();
  size_t q = l.find('"', p);
  return l.substr(p, q - p);
}

static Ack dernierAck(const char* cmd, SerieMock& port = Serial) {
  Ack a = {false, false, "", ""};
  std::string motif = std::string("{\"t\":\"ack\",\"cmd\":\"") + cmd + "\"";
  for (size_t i = port.lignes.size(); i-- > 0;) {
    const std::string& l = port.lignes[i];
    if (l.compare(0, motif.size(), motif) == 0) {
      a.trouve = true;
      a.ok = l.find("\"ok\":true") != std::string::npos;
      a.val = champJson(l, "val");
      a.raison = champJson(l, "raison");
      return a;
    }
  }
  return a;
}

static const std::string* derniereLigne(const char* debut, SerieMock& port = Serial) {
  for (size_t i = port.lignes.size(); i-- > 0;) {
    if (port.lignes[i].compare(0, strlen(debut), debut) == 0) return &port.lignes[i];
  }
  return NULL;
}

static void viderSorties() {
  Serial.lignes.clear();  Serial.courante.clear();
  Serial1.lignes.clear(); Serial1.courante.clear();
}

/* Même initialisation que le banc principal : T_init=25 -> T_cible=55. */
static void initBanc() {
  mockIO.reinitialiser();
  Serial.viderEntree();  Serial1.viderEntree();
  Serial.limiter_tx = false;  Serial1.limiter_tx = false;
  Serial.place_tx = 63;       Serial1.place_tx = 63;
  viderSorties();
  memset(EEPROM.donnees, 0xFF, sizeof(EEPROM.donnees));

  mockIO.entree_num[PIN_AU_URGENCE] = LOW;
  mockIO.entree_num[PIN_FLAMME]     = LOW;
  mockIO.entree_ana[PIN_MQ8_H2]     = 50;
  mockIO.entree_ana[PIN_MQ6_BUT]    = 50;
  mockIO.entree_ana[PIN_PRESS_H2]   = adcPression(8.0f);
  reglerTsec(25.0f);
  mock_ds18b20[IDX_SONDE_T_CAP] = 20.0f;
  reglerTamb(25.0f);
  mock_dht_temp[PIN_DHT_EXTR] = 40.0f;
  mock_dht_hum[PIN_DHT_AMB]  = 35.0f;
  mock_dht_hum[PIN_DHT_EXTR] = 90.0f;

  setup();

  Config.Mode_Auto = false;  Config.Choix_Mode = 2;
  Config.T_cible = 55.0f;    Config.T_init = 25.0f;
  Config.Duree_Max_Cycle = 600UL;  Config.Prolong_Defaut = 30UL;
  Config.Temps_Min_Fin = 120UL;    Config.Temps_Reponse = 300UL;
  Config.Temps_Purge = 120UL;      Config.Temps_Allumage = 4UL;
  Config.Hhyst = 5.0f;  Config.Periode_Regul = 1UL;  Config.Regul_Auto = true;
  Config.Press_H2_Min = 2.0f;  Config.DeltaT_Sol = 20.0f;
  Config.Marge_Sol = 0.0f;     Config.Hyst_Sol = 5.0f;  Config.Seuil_Chaud = 40.0f;
  avancer(100);
}

/* Démarrage à distance en H2 manuel, jusqu'à la flamme (palier 100 %). */
static void demarrerH2() {
  envoyer("START");
  avancer(purgeMs());
  reglerFlamme(true);
  avancer(200);
}

/* Trois échecs d'allumage -> ERREUR_COMBUSTION. */
static void provoquerErreurCombustion() {
  envoyer("START");
  reglerFlamme(false);
  for (int i = 0; i < 3; i++) avancer(purgeMs() + allumageMs() + 100);
}

/* --------------------------------------------------------------------------
 *  Cas de test
 * ------------------------------------------------------------------------*/
static void cas_telemesure_et_journal() {
  ouvrirCas("1. Telemesure 1 s, journal non melange aux trames, USB = ESP32");
  initBanc();
  viderSorties();
  avancer(2100);
  const std::string* tel = derniereLigne("{\"t\":\"tel\"");
  verifier(tel != NULL, "trame de telemesure emise");
  if (tel) {
    verifier(tel->back() == '}', "trame complete");
    verifier(tel->find("\"etat\":\"ATTENTE\"") != std::string::npos, "etat ATTENTE");
    verifier(tel->find("\"tsec\":25.0") != std::string::npos, "T_sec dans la trame");
    verifier(tel->find("\"urgp\":0") != std::string::npos, "pas de cause d'urgence presente");
  }
  int nb_tel = 0;
  for (size_t i = 0; i < Serial.lignes.size(); i++) {
    if (Serial.lignes[i].compare(0, 10, "{\"t\":\"tel\"") == 0) nb_tel++;
  }
  verifier(nb_tel == 2 || nb_tel == 3, "une trame par seconde environ");
  verifier(Serial.lignes == Serial1.lignes, "memes lignes sur l'USB et sur Serial1 (ESP32)");

  /* Lignes de debogage : emises entieres, jamais au milieu d'une trame. */
  demarrerH2();
  bool vu_fsm = false, melange = false;
  for (size_t i = 0; i < Serial.lignes.size(); i++) {
    const std::string& l = Serial.lignes[i];
    if (l == "FSM -> H2") vu_fsm = true;
    if (l[0] == '{' && (l.back() != '}' || l.find("FSM") != std::string::npos)) melange = true;
    if (l[0] != '{' && l.find("{\"t\"") != std::string::npos) melange = true;
  }
  verifier(vu_fsm, "ligne de journal 'FSM -> H2' transmise");
  verifier(!melange, "aucune ligne melangee");
}

static void cas_emission_non_bloquante() {
  ouvrirCas("2. Emission par morceaux : tampon d'emission de 10 octets");
  initBanc();
  viderSorties();
  Serial.limiter_tx = true;  Serial1.limiter_tx = true;
  size_t max_par_tour = 0;
  for (int k = 0; k < 400; k++) {             // 8 s
    Serial.place_tx = 10;  Serial1.place_tx = 10;
    size_t avant = Serial.courante.size();
    for (size_t i = 0; i < Serial.lignes.size(); i++) avant += Serial.lignes[i].size() + 1;
    avancer(20);
    size_t apres = Serial.courante.size();
    for (size_t i = 0; i < Serial.lignes.size(); i++) apres += Serial.lignes[i].size() + 1;
    if (apres - avant > max_par_tour) max_par_tour = apres - avant;
  }
  verifier(max_par_tour <= 10, "jamais plus que la place libre par tour");
  const std::string* tel = derniereLigne("{\"t\":\"tel\"");
  verifier(tel != NULL && tel->back() == '}', "trames completes malgre le petit tampon");
}

static void cas_meta_et_cfg() {
  ouvrirCas("3. GET META / GET CFG : 31 champs, bornes des constantes du programme");
  initBanc();
  envoyer("GET META");
  avancer(200);
  const std::string* meta = derniereLigne("{\"t\":\"meta\"");
  verifier(meta != NULL, "trame meta emise");
  if (meta) {
    size_t n = 0, p = 0;
    while ((p = meta->find("{\"k\":", p)) != std::string::npos) { n++; p++; }
    verifier(n == 31, "31 champs dans meta");
    verifier(meta->find("\"k\":\"t_cible\",\"l\":\"Température cible\",\"u\":\"°C\",\"ty\":\"n\",\"g\":\"Cycle\",\"s\":0,\"min\":30.0,\"max\":90.0,\"pas\":1.0") != std::string::npos,
             "t_cible : bornes T_CIBLE_MIN/MAX, pas PAS_TEMPERATURE");
    verifier(meta->find("\"k\":\"temps_purge\",\"l\":\"Durée de purge\",\"u\":\"s\",\"ty\":\"e\",\"g\":\"Sécurité combustion\",\"s\":1,\"min\":60,\"max\":300,\"pas\":10") != std::string::npos,
             "temps_purge : parametre de securite, plancher 60 s");
    verifier(meta->find("\"k\":\"press_val\"") != std::string::npos && meta->substr(meta->size() - 3) == "}]}",
             "dernier champ press_val, trame fermee");
  }
  envoyer("GET CFG");
  avancer(200);
  const std::string* cfg = derniereLigne("{\"t\":\"cfg\"");
  verifier(cfg != NULL, "trame cfg emise");
  if (cfg) {
    verifier(cfg->find("\"t_cible\":55.0") != std::string::npos, "t_cible = 55.0");
    verifier(cfg->find("\"temps_purge\":120") != std::string::npos, "temps_purge = 120");
    verifier(cfg->find("\"press_val\":5.0}") != std::string::npos, "dernier champ et fin de trame");
  }
}

static void cas_set_et_save() {
  ouvrirCas("4. SET borne, refuse en cycle sauf t_cible ; SAVE explicite");
  initBanc();
  envoyer("SET t_cible 200");
  Ack a = dernierAck("SET");
  verifier(a.ok && a.val == "90.0" && Config.T_cible == 90.0f, "t_cible 200 -> 90.0 (borne)");
  envoyer("SET temps_purge 5");
  a = dernierAck("SET");
  verifier(a.ok && a.val == "60" && Config.Temps_Purge == 60UL, "temps_purge 5 -> 60 (plancher de securite)");
  envoyer("SET seuil_mq8 2000");
  a = dernierAck("SET");
  verifier(a.ok && a.val == "900", "seuil_mq8 2000 -> 900");
  envoyer("SET choix_mode 9");
  a = dernierAck("SET");
  verifier(a.ok && a.val == "2", "choix_mode hors liste -> valeur par defaut 2");
  envoyer("SET palier_impose -1");
  a = dernierAck("SET");
  verifier(a.ok && a.val == "1", "palier_impose -1 -> valeur par defaut 67 %");
  envoyer("SET t_cible abc");
  verifier(!dernierAck("SET").ok, "valeur non numerique refusee");
  envoyer("SET inconnue 3");
  a = dernierAck("SET");
  verifier(!a.ok && a.raison == "cle inconnue", "cle inconnue refusee");

  /* EEPROM : rien n'est écrit avant SAVE. */
  envoyer("SET t_cible 60");
  Configuration lue;
  EEPROM.get(EEPROM_ADR_CONFIG, lue);
  verifier(lue.sentinelle != CONFIG_SENTINELLE || lue.T_cible != 60.0f, "pas d'ecriture EEPROM sans SAVE");
  envoyer("SAVE");
  EEPROM.get(EEPROM_ADR_CONFIG, lue);
  verifier(dernierAck("SAVE").ok && lue.T_cible == 60.0f, "SAVE ecrit la configuration");

  /* En cycle : seul t_cible, sauvegardé comme au pupitre. */
  envoyer("SET t_cible 55");
  Config.Temps_Purge = 120UL;
  demarrerH2();
  verifier(etat_courant == ETAT_MODE_H2, "cycle H2 en cours");
  envoyer("SET hhyst 3");
  a = dernierAck("SET");
  verifier(!a.ok && Config.Hhyst == 5.0f, "hhyst refuse en cycle");
  envoyer("SET t_cible 58");
  a = dernierAck("SET");
  EEPROM.get(EEPROM_ADR_CONFIG, lue);
  verifier(a.ok && a.val == "58.0" && T3 == 58.0f, "t_cible accepte en cycle, seuils recalcules");
  verifier(lue.T_cible == 58.0f, "t_cible en cycle sauvegardee (comme UP/DOWN)");
  envoyer("SAVE");
  verifier(!dernierAck("SAVE").ok, "SAVE refuse en cycle");
}

static void cas_start_stop() {
  ouvrirCas("5. START seulement hors cycle ; STOP dans chaque etat ou le bouton agit");
  initBanc();
  envoyer("STOP");
  verifier(!dernierAck("STOP").ok && etat_courant == ETAT_ATTENTE_DEMARRAGE, "STOP sans effet en ATTENTE");
  envoyer("START");
  Ack a = dernierAck("START");
  verifier(a.ok && a.val == "H2" && etat_courant == ETAT_MODE_H2, "START en ATTENTE -> H2");
  envoyer("START");
  verifier(!dernierAck("START").ok && etat_courant == ETAT_MODE_H2, "START refuse en cycle");
  envoyer("STOP");
  verifier(dernierAck("STOP").ok && etat_courant == ETAT_SECHAGE_TERMINE, "STOP en H2 -> TERMINE");
  envoyer("START");
  verifier(dernierAck("START").ok && etat_courant == ETAT_MODE_H2, "START en TERMINE -> nouveau cycle");

  /* GPL */
  initBanc();
  Config.Choix_Mode = 3;
  envoyer("START");
  verifier(etat_courant == ETAT_MODE_GPL, "START -> GPL (manuel)");
  envoyer("STOP");
  verifier(etat_courant == ETAT_SECHAGE_TERMINE, "STOP en GPL -> TERMINE");

  /* SOLAIRE */
  initBanc();
  Config.Choix_Mode = 1;
  envoyer("START");
  verifier(etat_courant == ETAT_MODE_SOLAIRE, "START -> SOLAIRE (manuel)");
  envoyer("STOP");
  verifier(etat_courant == ETAT_SECHAGE_TERMINE, "STOP en SOLAIRE -> TERMINE");

  /* DEMANDE_PROLONGATION puis PROLONGATION */
  initBanc();
  Config.Duree_Max_Cycle = 15UL;
  demarrerH2();
  avancer(15UL * 60000UL);
  verifier(etat_courant == ETAT_DEMANDE_PROLONGATION, "duree max -> DEMANDE");
  envoyer("PROLONG 45");
  a = dernierAck("PROLONG");
  verifier(a.ok && a.val == "45" && duree_prolongation_min == 45UL, "PROLONG 45 regle N");
  envoyer("PROLONG 1");
  verifier(duree_prolongation_min == PROLONG_MIN, "PROLONG 1 -> PROLONG_MIN");
  envoyer("PROLONG 45");
  envoyer("PROLONG OK");
  verifier(dernierAck("PROLONG").ok && etat_courant == ETAT_PROLONGATION, "PROLONG OK -> PROLONGATION");
  envoyer("STOP");
  verifier(etat_courant == ETAT_SECHAGE_TERMINE, "STOP en PROLONGATION -> TERMINE");

  initBanc();
  Config.Duree_Max_Cycle = 15UL;
  demarrerH2();
  avancer(15UL * 60000UL);
  envoyer("TERMINER");
  verifier(dernierAck("TERMINER").ok && etat_courant == ETAT_SECHAGE_TERMINE, "TERMINER en DEMANDE -> TERMINE");

  /* ERREUR_COMBUSTION */
  initBanc();
  provoquerErreurCombustion();
  verifier(etat_courant == ETAT_ERREUR_COMBUSTION, "3 echecs -> ERREUR_COMBUSTION");
  envoyer("STOP");
  verifier(dernierAck("STOP").ok && etat_courant == ETAT_SECHAGE_TERMINE, "STOP en ERREUR -> TERMINE");

  /* STOP accepte depuis l'ESP32 aussi */
  initBanc();
  envoyer("START", Serial1);
  envoyer("STOP", Serial1);
  verifier(etat_courant == ETAT_SECHAGE_TERMINE, "START/STOP par Serial1 (ESP32)");
}

static void cas_erreur_combustion() {
  ouvrirCas("6. ERREUR REESSAYER / MANUEL / AUTO ; refuse hors ERREUR_COMBUSTION");
  initBanc();
  envoyer("ERREUR REESSAYER");
  verifier(!dernierAck("ERREUR").ok, "ERREUR refuse en ATTENTE");

  initBanc();
  provoquerErreurCombustion();
  verifier(Buzzer, "buzzer actif en ERREUR");
  envoyer("ERREUR REESSAYER");
  verifier(dernierAck("ERREUR").ok && etat_courant == ETAT_MODE_H2 && nb_echecs_allumage == 0 &&
           phase_combustion == PH_PURGE, "REESSAYER -> H2, purge, compteur remis a 0");

  initBanc();
  provoquerErreurCombustion();
  envoyer("ERREUR MANUEL");
  verifier(etat_courant == ETAT_ATTENTE_DEMARRAGE && !Config.Mode_Auto, "MANUEL -> ATTENTE, mode manuel");

  initBanc();
  provoquerErreurCombustion();
  envoyer("ERREUR AUTO");
  verifier(etat_courant == ETAT_MODE_H2 && Config.Mode_Auto, "AUTO -> source active, mode automatique");

  initBanc();
  provoquerErreurCombustion();
  envoyer("ERREUR N_IMPORTE");
  verifier(!dernierAck("ERREUR").ok && etat_courant == ETAT_ERREUR_COMBUSTION, "choix inconnu refuse");

#if AVEC_PUPITRE_LCD
  /* Même état d'arrivée qu'au pupitre : MENU (choix) puis OK. */
  const char* choix[3] = {"ERREUR REESSAYER", "ERREUR MANUEL", "ERREUR AUTO"};
  for (int c = 0; c < 3; c++) {
    initBanc();
    provoquerErreurCombustion();
    for (int k = 0; k < c; k++) appuyer(PIN_BTN_MENU);
    appuyer(PIN_BTN_OK);
    EtatFSM etat_bouton = etat_courant;
    bool auto_bouton = Config.Mode_Auto;
    initBanc();
    provoquerErreurCombustion();
    envoyer(choix[c]);
    verifier(etat_courant == etat_bouton && Config.Mode_Auto == auto_bouton, choix[c]);
  }
#endif
}

static void cas_urgence_et_rearmement() {
  ouvrirCas("7. URGENCE : seule REARM en sort, refusee tant qu'une cause est presente");
  initBanc();
  demarrerH2();
  mockIO.entree_ana[PIN_MQ8_H2] = 600;
  avancer(100);
  verifier(etat_courant == ETAT_URGENCE_ATEX && !V_H2, "fuite H2 -> URGENCE, gaz ferme");

  const char* autres[] = {"START", "STOP", "SET t_cible 50", "SAVE", "PROLONG OK",
                          "PROLONG 40", "TERMINER", "ERREUR AUTO", "ERREUR REESSAYER", "PING", "GET CFG"};
  for (size_t i = 0; i < sizeof(autres) / sizeof(autres[0]); i++) {
    envoyer(autres[i]);
    envoyer(autres[i], Serial1);
  }
  mockIO.entree_ana[PIN_MQ8_H2] = 50;   // la cause disparaît, mais personne n'a réarmé
  avancer(1000);
  verifier(etat_courant == ETAT_URGENCE_ATEX, "aucune autre commande ne sort de URGENCE");

  mockIO.entree_ana[PIN_MQ8_H2] = 600;
  envoyer("REARM");
  Ack a = dernierAck("REARM");
  verifier(!a.ok && a.raison.find("MQ-8 600 > 350") != std::string::npos, "REARM refuse : fuite H2 (raison donnee)");
  mockIO.entree_ana[PIN_MQ8_H2] = 50;

  mockIO.entree_num[PIN_AU_URGENCE] = HIGH;   // AU enfoncé (contact NC ouvert)
  avancer(100);
  envoyer("REARM");
  a = dernierAck("REARM");
  verifier(!a.ok && a.raison.find("arret d'urgence") != std::string::npos, "REARM refuse : AU enfonce");
  mockIO.entree_num[PIN_AU_URGENCE] = LOW;

  reglerFlamme(true);
  avancer(100);
  envoyer("REARM");
  a = dernierAck("REARM");
  verifier(!a.ok && a.raison.find("flamme") != std::string::npos, "REARM refuse : flamme encore detectee");
  reglerFlamme(false);

  mockIO.entree_ana[PIN_MQ6_BUT] = 700;
  avancer(100);
  envoyer("REARM");
  verifier(!dernierAck("REARM").ok && etat_courant == ETAT_URGENCE_ATEX, "REARM refuse : fuite GPL");
  mockIO.entree_ana[PIN_MQ6_BUT] = 50;
  avancer(100);

  envoyer("REARM");
  a = dernierAck("REARM");
  verifier(a.ok && etat_courant == ETAT_ATTENTE_DEMARRAGE && !Buzzer, "REARM accepte -> ATTENTE");
  envoyer("REARM");
  verifier(!dernierAck("REARM").ok, "REARM refuse hors URGENCE");

  /* L'arrêt d'urgence matériel reste prioritaire sur toute commande. */
  initBanc();
  demarrerH2();
  mockIO.entree_num[PIN_AU_URGENCE] = HIGH;
  envoyer("STOP");
  verifier(etat_courant == ETAT_URGENCE_ATEX && cause_urgence == URG_ARRET_URGENCE,
           "AU materiel : URGENCE, meme si STOP arrive au meme instant");
}

static void cas_lignes_invalides() {
  ouvrirCas("8. Ligne de 200 caracteres, commande inconnue : rejet sans plantage");
  initBanc();
  std::string longue(200, 'A');
  envoyer(longue.c_str());
  avancer(200);
  Ack a = dernierAck("?");
  verifier(a.trouve && !a.ok, "ligne trop longue rejetee");
  envoyer("FAIRE_UN_CAFE");
  a = dernierAck("FAIRE_UN_");   // nom de commande tronque a 9 caracteres
  verifier(a.trouve && !a.ok && a.raison == "commande inconnue", "commande inconnue rejetee");
  envoyer("");
  envoyer("   ");
  envoyer("start");
  verifier(etat_courant == ETAT_MODE_H2, "commande en minuscules acceptee, programme toujours vivant");
  envoyer("PING");
  verifier(dernierAck("PING").ok && dernierAck("PING").val == "sechoir-if 1", "PING repond");
  /* Une commande coupée en plusieurs paquets série est reconstituée. */
  initBanc();
  Serial.injecter("STA");
  avancer(40);
  Serial.injecter("RT\r\n");
  avancer(80);
  verifier(etat_courant == ETAT_MODE_H2, "commande recue en deux morceaux, fin de ligne CRLF");
}

#if AVEC_PUPITRE_LCD
static void cas_pupitre_et_interface() {
  ouvrirCas("9. Avec pupitre : STOP distant = STOP bouton ; SET refuse si menu local ouvert");
  initBanc();
  demarrerH2();
  appuyer(PIN_BTN_STOP);
  EtatFSM par_bouton = etat_courant;
  initBanc();
  demarrerH2();
  envoyer("STOP");
  verifier(etat_courant == par_bouton, "meme etat d'arrivee qu'avec le bouton STOP");

  initBanc();
  appuyer(PIN_BTN_MENU);
  verifier(etat_courant == ETAT_CONFIG_MENU, "menu local ouvert");
  envoyer("SET t_cible 60");
  verifier(!dernierAck("SET").ok && Config.T_cible == 55.0f, "SET refuse pendant le menu local");
  envoyer("START");
  verifier(etat_courant == ETAT_MODE_H2, "START accepte depuis le menu (comme le bouton)");
}
#endif

int main() {
  printf("=== BANC LIAISON INTERFACE — AVEC_PUPITRE_LCD %d ===\n\n", (int)AVEC_PUPITRE_LCD);
  cas_telemesure_et_journal();
  cas_emission_non_bloquante();
  cas_meta_et_cfg();
  cas_set_et_save();
  cas_start_stop();
  cas_erreur_combustion();
  cas_urgence_et_rearmement();
  cas_lignes_invalides();
#if AVEC_PUPITRE_LCD
  cas_pupitre_et_interface();
#endif
  printf("\n-----------------------------------------------------\n");
  printf("Verifications : %d   Echecs : %d\n", nb_verif, nb_echecs);
  printf("Resultat      : %s\n", nb_echecs == 0 ? "TOUS LES TESTS PASSENT" : "ECHECS");
  printf("-----------------------------------------------------\n");
  return nb_echecs == 0 ? 0 : 1;
}
