/* ============================================================================
 *  BANC DE TESTS — Séchoir solaire hybride v2
 *
 *  Le programme Arduino est compilé TEL QUEL sur PC : les bibliothèques
 *  matérielles sont remplacées par les mocks de tests/mocks/. Le banc pilote
 *  les capteurs et les boutons puis appelle loop(), exactement comme la carte.
 *
 *  Compilation / exécution :  make -C tests run
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

static void verifierEgalFloat(float obtenu, float attendu, const char* libelle) {
  bool ok = (obtenu - attendu < 0.01f) && (attendu - obtenu < 0.01f);
  nb_verif++;
  if (!ok) {
    nb_echecs++;
    printf("    [ECHEC] %s (obtenu %.3f, attendu %.3f)\n", libelle, obtenu, attendu);
  }
}

static void ouvrirCas(const char* nom) { printf("  %s\n", nom); }

/* --------------------------------------------------------------------------
 *  Pilotage du banc
 * ------------------------------------------------------------------------*/

static void avancer(uint32_t duree_ms, uint32_t pas = 20) {
  if (duree_ms == 0) { loop(); return; }
  uint32_t reste = duree_ms;
  while (reste > 0) {
    uint32_t d = (reste < pas) ? reste : pas;
    mockIO.horloge_ms += d;
    reste -= d;
    loop();
  }
}

static void uneIteration() { avancer(20, 20); }
static void rafraichirCapteurs() { avancer(1200); }

static void appuyer(uint8_t pin) {
  mockIO.entree_num[pin] = LOW;
  avancer(60);
  mockIO.entree_num[pin] = HIGH;
  avancer(60);
}

static int adcPression(float bar) {
  if (bar <= 0.0f) return ADC_PRESS_4MA;
  return ADC_PRESS_4MA + (int)(bar / PRESS_H2_PLEINE_ECHELLE *
                               (float)(ADC_PRESS_20MA - ADC_PRESS_4MA));
}

static void reglerTsec(float valeur) { mock_ds18b20[IDX_SONDE_T_SEC] = valeur; }
static void reglerTcap(float valeur) { mock_ds18b20[IDX_SONDE_T_CAP] = valeur; }
static void reglerHsec(float valeur) { mock_dht_hum[PIN_DHT_EXTR] = valeur; }
static void reglerHamb(float valeur) { mock_dht_hum[PIN_DHT_AMB] = valeur; }
static void reglerFlamme(bool presente) {
  mockIO.entree_num[PIN_FLAMME] = presente ? NIVEAU_FLAMME_PRESENTE
                                           : !NIVEAU_FLAMME_PRESENTE;
}

static uint32_t purgeMs()   { return Config.Temps_Purge * 1000UL; }
static uint32_t allumageMs(){ return Config.Temps_Allumage * 1000UL; }

/* Remet le banc et la carte dans un état connu, avec des consignes de
 * référence : T_init=25 -> T_cible=55 (T1=35, T2=45, T3=55). */
static void initBanc() {
  mockIO.reinitialiser();
  Serial.lignes.clear();
  memset(EEPROM.donnees, 0xFF, sizeof(EEPROM.donnees));   // EEPROM vierge

  mockIO.entree_num[PIN_AU_URGENCE] = LOW;   // contact NC fermé = pas d'urgence
  mockIO.entree_num[PIN_FLAMME]     = LOW;
  mockIO.entree_ana[PIN_MQ8_H2]     = 50;
  mockIO.entree_ana[PIN_MQ6_BUT]    = 50;
  mockIO.entree_ana[PIN_PRESS_H2]   = adcPression(8.0f);

  reglerTsec(25.0f);
  reglerTcap(20.0f);                          // solaire indisponible
  mock_dht_temp[PIN_DHT_AMB]  = 25.0f;
  mock_dht_temp[PIN_DHT_EXTR] = 40.0f;
  reglerHamb(35.0f);
  reglerHsec(90.0f);

  setup();   // charge Config depuis l'EEPROM (vierge -> valeurs par défaut)

  Config.Mode_Auto       = false;             // mode manuel par défaut au banc
  Config.Choix_Mode      = 2;                 // H2
  Config.T_cible         = 55.0f;
  Config.T_init          = 25.0f;
  Config.H_produit_cible = 10.0f;
  Config.Duree_Max_Cycle = 600UL;             // min
  Config.Temps_Prolongation = 180UL;          // min
  Config.Temps_Min_Fin   = 120UL;             // min (2 h)
  Config.Temps_Arret_Auto = 300UL;            // s
  Config.Temps_Purge     = 120UL;             // s
  Config.Temps_Allumage  = 4UL;               // s
  Config.Hhyst           = 5.0f;
  Config.Press_H2_Min    = 2.0f;
}

static void demarrerEtAllumer() {
  appuyer(PIN_BTN_START);
  avancer(purgeMs() + 200);
  reglerFlamme(true);
  avancer(100);
}

/* --------------------------------------------------------------------------
 *  CAS 1 — Démarrage à froid : PURGE -> ALLUMAGE -> PALIER 100 %
 * ------------------------------------------------------------------------*/
static void cas_demarrage_a_froid() {
  ouvrirCas("1. Demarrage a froid : purge -> allumage -> palier 100 %");
  initBanc();

  appuyer(PIN_BTN_START);
  verifier(etat_courant == ETAT_MODE_H2, "entree en MODE_H2");
  verifier(phase_combustion == PH_PURGE, "phase initiale = PURGE");
  verifier(raison_purge == PURGE_DEMARRAGE, "raison = demarrage");
  verifier(palier == PALIER_100, "demarrage toujours au palier 100 %");
  verifier(!V_H2 && !V_But, "vannes source fermees pendant la purge");
  verifier(!EV1 && !EV2 && !EV3, "aucune EV ouverte pendant la purge");
  verifier(!Spark, "Spark inactif pendant la purge");
  verifier(PWM_Purge == PWM_PURGE_BALAYAGE, "ventilation de purge maximale");

  avancer(purgeMs() - 2000);
  verifier(phase_combustion == PH_PURGE, "toujours en purge juste avant l'echeance");
  verifier(!EV1 && !EV2 && !EV3, "gaz toujours ferme avant la fin de purge");

  avancer(2500);
  verifier(phase_combustion == PH_ALLUMAGE, "passage en ALLUMAGE a l'echeance de purge");
  verifier(V_H2 && !V_But, "vanne source H2 ouverte, GPL fermee");
  verifier(EV1 && EV2 && EV3, "ouverture au palier 100% : 3 EV");
  verifier(Spark, "Spark actif pendant l'allumage");

  reglerFlamme(true);
  uneIteration();
  verifier(phase_combustion == PH_PALIER_100, "flamme confirmee -> PALIER_100");
  verifier(!Spark, "Spark coupe des la confirmation de flamme");
  verifierEgalFloat(T1, 35.0f, "seuil T1");
  verifierEgalFloat(T2, 45.0f, "seuil T2");
  verifierEgalFloat(T3, 55.0f, "seuil T3 = T_cible");
}

/* --------------------------------------------------------------------------
 *  CAS 2 — Hysteresis a QUATRE niveaux (100/67/33/0), y compris le
 *  non-basculement dans chaque bande.
 *  T1=35, T2=45, T3=55(=T_cible) ; seuils effectifs a +/-2,5.
 * ------------------------------------------------------------------------*/
static void cas_hysteresis_quatre_niveaux() {
  ouvrirCas("2. Hysteresis 4 niveaux : 100/67/33/0%, non-basculement dans la bande");
  initBanc();
  demarrerEtAllumer();
  verifier(palier == PALIER_100, "point de depart : palier 100 %");

  reglerTsec(37.0f);  rafraichirCapteurs();
  verifier(palier == PALIER_100, "37,0 C < T1+2,5 : pas de basculement");

  reglerTsec(37.5f);  rafraichirCapteurs();
  verifier(palier == PALIER_67, "T_sec >= T1+2,5 : 100% -> 67% (EV1 fermee)");
  verifier(!EV1 && EV2 && EV3, "palier 67% : EV2+EV3");

  reglerTsec(47.0f);  rafraichirCapteurs();
  verifier(palier == PALIER_67, "47,0 C < T2+2,5 : pas de basculement");

  reglerTsec(47.5f);  rafraichirCapteurs();
  verifier(palier == PALIER_33, "T_sec >= T2+2,5 : 67% -> 33% (EV2 fermee)");
  verifier(!EV1 && !EV2 && EV3, "palier 33% : EV3 seule");

  reglerTsec(57.0f);  rafraichirCapteurs();
  verifier(palier == PALIER_33 || palier == PALIER_0, "en transit vers 0% autour de T3+2,5");

  /* Franchissement de T3 : coupure VOLONTAIRE, pas une panne. */
  reglerTsec(57.6f);  rafraichirCapteurs();
  verifier(palier == PALIER_0, "T_sec >= T3+2,5 : 33% -> 0% (coupure volontaire)");
  verifier(!EV1 && !EV2 && !EV3, "palier 0% : TOUTES les EV fermees (§2 v2)");
  verifier(!V_H2, "vanne source H2 fermee au palier 0%");
  verifier(phase_combustion == PH_PURGE, "coupure = retour en PURGE (raison PALIER_0)");
  verifier(raison_purge == PURGE_PALIER_0, "raison de purge = PALIER_0 (pas ECHEC)");
  verifier(nb_echecs_allumage == 0, "coupure volontaire : aucun echec comptabilise");

  /* La flamme s'eteint reellement une fois le gaz coupe (le mock ne le fait
   * pas tout seul : le controleur de flamme physique reagirait, mais rien
   * n'emule cette inertie ici). Sans ce reglage, Flame resterait "vraie" en
   * continu et l'etape ALLUMAGE plus bas se refermerait instantanement sur
   * PH_PALIER_33 avant que le test ait pu l'observer. */
  reglerFlamme(false);

  /* Tant que T_sec reste haute, purge indefiniment prolongee, PAS de
   * rallumage inutile (AJOUT au cahier des charges). */
  avancer(purgeMs() + 5000);
  verifier(phase_combustion == PH_PURGE, "reste en veille gaz ferme tant que T_sec est haute");
  verifier(!Spark, "aucune tentative d'allumage tant que la demande de 33% n'est pas revenue");

  /* La demande de chauffe revient : reallumage direct au palier 33% (pas
   * 100%), apres une purge deja largement satisfaite. */
  reglerTsec(52.0f);  // < T3 - 2,5 = 52,5 -> presque ; on descend encore un peu
  rafraichirCapteurs();
  reglerTsec(50.0f);
  rafraichirCapteurs();
  verifier(phase_combustion == PH_ALLUMAGE, "la demande revient -> nouvel allumage");
  verifier(palier == PALIER_33, "reallumage au palier 33% (pas 100%)");
  verifier(EV3 && !EV1 && !EV2, "ouverture EV3 seule pour le reallumage a 33%");

  reglerFlamme(true);
  uneIteration();
  verifier(phase_combustion == PH_PALIER_33, "regulation reprend au palier 33%");

  /* Redescente complete : 33% -> 67% -> 100%. */
  reglerTsec(43.0f);  rafraichirCapteurs();
  verifier(palier == PALIER_33, "43,0 C >= T2-2,5 : pas de basculement");
  reglerTsec(42.4f);  rafraichirCapteurs();
  verifier(palier == PALIER_67, "T_sec < T2-2,5 : 33% -> 67%");
  reglerTsec(33.0f);  rafraichirCapteurs();
  verifier(palier == PALIER_67, "33,0 C >= T1-2,5 : pas de basculement");
  reglerTsec(32.4f);  rafraichirCapteurs();
  verifier(palier == PALIER_100, "T_sec < T1-2,5 : 67% -> 100%");
}

/* --------------------------------------------------------------------------
 *  CAS 3 — Echec d'allumage apres Temps_Allumage
 * ------------------------------------------------------------------------*/
static void cas_echec_allumage() {
  ouvrirCas("3. Echec d'allumage : pas de flamme sous Temps_Allumage -> PURGE");
  initBanc();

  appuyer(PIN_BTN_START);
  avancer(purgeMs() + 200);
  verifier(phase_combustion == PH_ALLUMAGE, "phase ALLUMAGE atteinte");

  avancer(allumageMs() - 800);
  verifier(phase_combustion == PH_ALLUMAGE, "toujours en allumage avant l'echeance");

  avancer(1200);
  verifier(phase_combustion == PH_PURGE, "echec -> retour PURGE");
  verifier(raison_purge == PURGE_ECHEC, "raison = ECHEC");
  verifier(!EV1 && !EV2 && !EV3, "fermeture totale des EV");
  verifier(!V_H2 && !V_But, "fermeture des vannes source");
  verifier(nb_echecs_allumage == 1, "1er echec comptabilise");
  verifier(etat_courant == ETAT_MODE_H2, "pas encore d'escalade (1 seul echec)");
}

/* --------------------------------------------------------------------------
 *  CAS 4 — Perte de flamme inattendue
 * ------------------------------------------------------------------------*/
static void cas_perte_flamme() {
  ouvrirCas("4. Perte de flamme inattendue : fermeture immediate des EV");
  initBanc();
  demarrerEtAllumer();
  verifier(EV1 && EV2 && EV3, "EV ouvertes avant la perte de flamme");

  reglerFlamme(false);
  uneIteration();
  verifier(!EV1 && !EV2 && !EV3, "EV refermees immediatement");
  verifier(!V_H2 && !V_But, "vannes source refermees immediatement");
  verifier(phase_combustion == PH_PURGE, "retour PURGE");
  verifier(raison_purge == PURGE_ECHEC, "raison = ECHEC (perte de flamme)");
  verifier(mockIO.sortie_num[PIN_EV1] == LOW, "broche EV1 physiquement retombee");
}

/* --------------------------------------------------------------------------
 *  CAS 5 — Echecs d'allumage REPETES -> ETAT_ERREUR_COMBUSTION (§13, AVIS n°3)
 * ------------------------------------------------------------------------*/
static void cas_echecs_repetes_escalade() {
  ouvrirCas("5. Echecs d'allumage repetes -> ERREUR_COMBUSTION (escalade)");
  initBanc();

  appuyer(PIN_BTN_START);
  for (uint16_t i = 0; i < MAX_ECHECS_AVANT_ALARME; i++) {
    avancer(purgeMs() + 200);
    avancer(allumageMs() + 200);   // pas de flamme -> echec
  }
  verifier(nb_echecs_allumage >= MAX_ECHECS_AVANT_ALARME, "seuil d'echecs atteint");
  verifier(etat_courant == ETAT_ERREUR_COMBUSTION, "escalade vers ERREUR_COMBUSTION");
  verifier(raison_erreur_combustion == ERR_ALLUMAGE_REPETE, "raison = ALLUMAGE_REPETE");
  verifier(Buzzer, "buzzer actif en erreur de combustion");
  verifier(!EV1 && !EV2 && !EV3 && !V_H2 && !V_But, "gaz ferme en attente de decision");

  /* REESSAYER : repart sur une purge complete, meme source. */
  appuyer(PIN_BTN_OK);   // Choix_Erreur par defaut = REESSAYER
  verifier(phase_combustion == PH_PURGE, "REESSAYER relance une purge complete");
  verifier(etat_courant == ETAT_MODE_H2, "retour dans MODE_H2 (meme source)");
  verifier(nb_echecs_allumage == 0, "compteur d'echecs remis a zero");
}

/* --------------------------------------------------------------------------
 *  CAS 6 — Bascule H2 -> GPL reussie : palier CONSERVE
 * ------------------------------------------------------------------------*/
static void cas_bascule_h2_gpl_reussie() {
  ouvrirCas("6. Bascule H2 -> GPL reussie : palier conserve, purge complete");
  initBanc();
  Config.Mode_Auto = true;
  reglerTcap(20.0f);
  rafraichirCapteurs();

  demarrerEtAllumer();
  verifier(Source_Active == SRC_H2, "cycle demarre sur H2");

  reglerTsec(40.0f);
  rafraichirCapteurs();
  verifier(palier == PALIER_67, "regulation etablie au palier 67%");

  mockIO.entree_ana[PIN_PRESS_H2] = adcPression(0.5f);   // H2 epuise
  uneIteration();
  verifier(Source_Active == SRC_GPL, "bascule vers GPL");
  verifier(palier == PALIER_67, "PALIER CONSERVE a 67% (pas de retour a 100%)");
  verifier(raison_purge == PURGE_BASCULEMENT, "raison = BASCULEMENT");
  verifier(!EV1 && !EV2 && !EV3, "gaz ferme pendant la purge de bascule");

  /* Le gaz est coupe : la flamme reelle s'eteindrait. Le mock ne le fait pas
   * tout seul (Flame etait restee "vraie" depuis demarrerEtAllumer()) ; sans
   * cette ligne, la phase ALLUMAGE ci-dessous se refermerait sur
   * PH_PALIER_67 avant meme l'assertion suivante. */
  reglerFlamme(false);

  avancer(purgeMs() + 200);
  verifier(phase_combustion == PH_ALLUMAGE, "reallumage apres la purge de bascule");
  verifier(!V_H2 && V_But, "vanne source GPL ouverte, H2 fermee");
  verifier(!EV1 && EV2 && EV3, "reallumage AU PALIER 67% (EV2+EV3)");

  reglerFlamme(true);
  uneIteration();
  verifier(phase_combustion == PH_PALIER_67, "regulation reprend a 67%");
  verifier(etat_courant == ETAT_MODE_GPL, "etat principal = MODE_GPL");
}

/* --------------------------------------------------------------------------
 *  CAS 7 — Echec du basculement -> ERREUR_COMBUSTION des le 1er echec (§13)
 * ------------------------------------------------------------------------*/
static void cas_bascule_echouee() {
  ouvrirCas("7. Echec du basculement : escalade des le 1er echec (pas de boucle silencieuse)");
  initBanc();
  Config.Mode_Auto = true;
  reglerTcap(20.0f);
  rafraichirCapteurs();
  demarrerEtAllumer();

  mockIO.entree_ana[PIN_PRESS_H2] = adcPression(0.5f);
  uneIteration();
  verifier(Source_Active == SRC_GPL, "bascule declenchee vers GPL");

  /* Le GPL est simule indisponible : la flamme ne doit JAMAIS se confirmer.
   * Sans ce reglage, Flame resterait "vraie" depuis demarrerEtAllumer() et
   * l'allumage "reussirait" artificiellement, invalidant tout le scenario. */
  reglerFlamme(false);

  avancer(purgeMs() + 200);
  verifier(phase_combustion == PH_ALLUMAGE, "tentative d'allumage sur la nouvelle source");
  avancer(allumageMs() + 200);

  verifier(etat_courant == ETAT_ERREUR_COMBUSTION, "echec de bascule -> ERREUR_COMBUSTION direct");
  verifier(raison_erreur_combustion == ERR_BASCULEMENT, "raison = BASCULEMENT");
  verifier(Buzzer, "alarme sonore");
  verifier(!EV1 && !EV2 && !EV3 && !V_H2 && !V_But, "gaz ferme");

  /* Choix MANUEL : retour a l'attente, mode manuel force. */
  appuyer(PIN_BTN_MENU);   // REESSAYER -> MANUEL
  appuyer(PIN_BTN_OK);
  verifier(etat_courant == ETAT_ATTENTE_DEMARRAGE, "MANUEL -> ATTENTE_DEMARRAGE");
  verifier(!Config.Mode_Auto, "mode automatique desactive");
  verifier(!EV1 && !EV2 && !EV3 && !V_H2 && !V_But, "installation fermee au repos");
}

/* --------------------------------------------------------------------------
 *  CAS 8 — Retour automatique GPL -> H2 (AJOUT, AVIS n°5)
 * ------------------------------------------------------------------------*/
static void cas_retour_automatique_gpl_h2() {
  ouvrirCas("8. Retour automatique GPL -> H2 quand la pression est retablie");
  initBanc();
  Config.Mode_Auto = true;
  reglerTcap(20.0f);
  Config.Choix_Mode = 3;
  mockIO.entree_ana[PIN_PRESS_H2] = adcPression(0.5f);   // H2 indisponible au demarrage
  rafraichirCapteurs();

  appuyer(PIN_BTN_START);
  verifier(Source_Active == SRC_GPL, "demarrage direct sur GPL (H2 indisponible)");

  avancer(purgeMs() + 200);
  reglerFlamme(true);
  avancer(100);
  verifier(phase_combustion == PH_PALIER_100, "regulation etablie sur GPL");

  /* Pression H2 retablie, mais tout juste au seuil : pas de bascule (marge). */
  mockIO.entree_ana[PIN_PRESS_H2] = adcPression(2.1f);
  uneIteration();
  verifier(Source_Active == SRC_GPL, "pas de bascule : marge anti-court-cycle non franchie");

  /* Pression nettement retablie : bascule vers H2, palier conserve. */
  mockIO.entree_ana[PIN_PRESS_H2] = adcPression(3.0f);
  uneIteration();
  verifier(Source_Active == SRC_H2, "retour automatique vers H2 (priorite sur GPL)");
  verifier(raison_purge == PURGE_BASCULEMENT, "purge de bascule imposee");
}

/* --------------------------------------------------------------------------
 *  CAS 9 — Fin de cycle : H_sec <= H_fin, VERROUILLE par Temps_Min_Fin (§5)
 * ------------------------------------------------------------------------*/
static void cas_fin_de_cycle_verrouillee() {
  ouvrirCas("9. Fin de cycle H_sec<=H_fin, verrouillee par Temps_Min_Fin");
  initBanc();
  Config.Temps_Min_Fin = 120UL;   // 2 h
  demarrerEtAllumer();

  verifierEgalFloat(H_fin, 45.0f, "H_fin = H_produit_cible + H_amb = 10 + 35");

  /* H_sec deja sous H_fin, mais AVANT le temps minimal : ne doit PAS
   * terminer le cycle (§5 : "ne jamais terminer... avant 2 h"). */
  reglerHsec(20.0f);
  rafraichirCapteurs();
  verifier(etat_courant == ETAT_MODE_H2, "condition hygrometrique vraie mais Temps_Min_Fin non ecoule");

  /* On avance jusqu'a 1 min avant l'echeance de Temps_Min_Fin, calculee par
   * rapport a chrono_cycle reel (et non a une hypothese de temps ecoule nul :
   * demarrerEtAllumer() a deja consomme la duree de purge+allumage). */
  uint32_t echeance_ms = Config.Temps_Min_Fin * 60000UL;
  uint32_t ecoule_ms = t_boucle - chrono_cycle;
  verifier(ecoule_ms < echeance_ms, "pre-requis du test : Temps_Min_Fin pas deja ecoule");
  avancer(echeance_ms - ecoule_ms - 60000UL, 5000);
  rafraichirCapteurs();
  verifier(etat_courant == ETAT_MODE_H2, "toujours actif juste avant Temps_Min_Fin");

  /* Franchissement de Temps_Min_Fin : la demande d'arret apparait. */
  avancer(2UL * 60000UL, 5000);
  rafraichirCapteurs();
  verifier(etat_courant == ETAT_FIN_TEMPORISATION, "Temps_Min_Fin ecoule + H_sec<=H_fin -> FIN_TEMPORISATION");
}

/* --------------------------------------------------------------------------
 *  CAS 10 — FIN_TEMPORISATION : confirmation, annulation, arret auto (§6)
 * ------------------------------------------------------------------------*/
static void cas_fin_temporisation() {
  ouvrirCas("10. FIN_TEMPORISATION : confirmation operateur / report / arret auto");
  initBanc();
  Config.Temps_Min_Fin = TEMPS_MIN_FIN_MIN; // plancher de securite (deja teste au cas 9)
  Config.Temps_Arret_Auto = 60UL;           // 1 min pour le banc
  demarrerEtAllumer();

  avancer(Config.Temps_Min_Fin * 60000UL + 2000, 5000);
  reglerHsec(20.0f);
  rafraichirCapteurs();
  verifier(etat_courant == ETAT_FIN_TEMPORISATION, "demande d'arret affichee");
  verifier(EV1 || EV2 || EV3 || V_H2, "la regulation continue pendant la fenetre de confirmation");

  /* Report manuel (Menu) : la condition hygrometrique reste vraie, donc on
   * ne peut pas repartir en regulation normale (§6 : le report redonne
   * seulement une pleine fenetre de confirmation, voir le commentaire dans
   * pasFSM()). On verifie que le compte a rebours a bien ete relance. */
  avancer(40000);   // 40 s sur les 60 s de la fenetre
  uint32_t chrono_avant_report = chrono_arret_auto;
  appuyer(PIN_BTN_MENU);
  verifier(etat_courant == ETAT_FIN_TEMPORISATION, "le report reste en FIN_TEMPORISATION (condition toujours vraie)");
  verifier(chrono_arret_auto != chrono_avant_report, "le compte a rebours a ete relance par le report");

  /* On laisse ensuite expirer le delai complet : arret automatique. */
  avancer(Config.Temps_Arret_Auto * 1000UL + 2000, 5000);
  verifier(etat_courant == ETAT_SECHAGE_TERMINE, "arret automatique apres Temps_Arret_Auto");
  verifier(!arret_force_duree, "arret normal (pas de plafond de duree depasse)");

  /* Confirmation manuelle (OK) : verifiee separement, cycle independant. */
  initBanc();
  Config.Temps_Min_Fin = TEMPS_MIN_FIN_MIN;
  demarrerEtAllumer();
  avancer(Config.Temps_Min_Fin * 60000UL + 2000, 5000);
  reglerHsec(20.0f);
  rafraichirCapteurs();
  verifier(etat_courant == ETAT_FIN_TEMPORISATION, "demande d'arret affichee (2e cycle)");
  appuyer(PIN_BTN_OK);
  verifier(etat_courant == ETAT_SECHAGE_TERMINE, "confirmation operateur (OK) -> arret immediat");
}

/* --------------------------------------------------------------------------
 *  CAS 11 — PROLONGATION avec plafond Temps_Prolongation (AVIS n°2 : ajout
 *  demande par le cahier des charges v2, qui contredit la regle v1
 *  « sans nouvelle limite de temps ». Implemente tel que demande.)
 * ------------------------------------------------------------------------*/
static void cas_prolongation_plafonnee() {
  ouvrirCas("11. PROLONGATION plafonnee par Temps_Prolongation : arret force");
  initBanc();
  Config.Duree_Max_Cycle = 3UL;         // 3 min
  Config.Temps_Prolongation = 2UL;      // 2 min de rallonge max
  Config.Temps_Min_Fin = TEMPS_MIN_FIN_MIN;
  reglerTsec(30.0f);
  rafraichirCapteurs();

  demarrerEtAllumer();
  reglerHsec(80.0f);   // H_sec > H_fin tout du long : jamais de fin normale

  /* On avance juste assez pour depasser Duree_Max_Cycle (3 min) sans encore
   * atteindre le plafond Temps_Prolongation (2 min de plus, soit 5 min au
   * total depuis chrono_cycle). Le calcul part du temps reellement ecoule
   * (demarrerEtAllumer() a deja consomme purge + allumage). */
  uint32_t echeance_max_cycle_ms = Config.Duree_Max_Cycle * 60000UL;
  uint32_t ecoule_ms = t_boucle - chrono_cycle;
  verifier(ecoule_ms < echeance_max_cycle_ms, "pre-requis : Duree_Max_Cycle pas deja ecoulee");
  avancer(echeance_max_cycle_ms - ecoule_ms + 2000, 5000);
  verifier(etat_courant == ETAT_PROLONGATION, "passage en PROLONGATION apres Duree_Max_Cycle");
  verifier(!arret_force_duree, "pas encore d'arret force a ce stade");

  /* On avance ensuite jusqu'a depasser le plafond Temps_Prolongation. */
  avancer(Config.Temps_Prolongation * 60000UL + 5000, 5000);
  verifier(etat_courant == ETAT_SECHAGE_TERMINE, "plafond Temps_Prolongation atteint -> arret force");
  verifier(arret_force_duree, "indicateur d'arret force active (humidite non garantie)");
  verifier(!EV1 && !EV2 && !EV3 && !V_H2 && !V_But, "gaz ferme apres arret force");
}

/* --------------------------------------------------------------------------
 *  CAS 12 — URGENCE_ATEX prioritaire + les DEUX chemins de rearmement
 * ------------------------------------------------------------------------*/
static void cas_urgence_et_rearmement() {
  ouvrirCas("12. URGENCE_ATEX prioritaire + rearmement materiel ET par menu");
  initBanc();
  demarrerEtAllumer();
  verifier(EV1 && EV2 && EV3, "bruleur en fonctionnement avant l'alarme");

  mockIO.entree_ana[PIN_MQ8_H2] = 600;
  uneIteration();
  verifier(etat_courant == ETAT_URGENCE_ATEX, "MQ8 > seuil -> URGENCE_ATEX");
  verifier(!EV1 && !EV2 && !EV3 && !V_H2 && !V_But, "fermeture immediate de tout le gaz");
  verifier(Buzzer, "buzzer actif");
  verifier(mockIO.sortie_pwm[PIN_PWM_PURGE] == 255, "PWM purge physiquement a 255");

  appuyer(PIN_BTN_REARM);
  verifier(etat_courant == ETAT_URGENCE_ATEX, "rearmement refuse : fuite toujours presente");

  mockIO.entree_ana[PIN_MQ8_H2] = 50;
  uneIteration();
  appuyer(PIN_BTN_REARM);
  verifier(etat_courant == ETAT_ATTENTE_DEMARRAGE, "rearmement materiel -> ATTENTE_DEMARRAGE");

  /* Chemin 2 : menu LCD. */
  mockIO.entree_num[PIN_AU_URGENCE] = HIGH;
  uneIteration();
  verifier(etat_courant == ETAT_URGENCE_ATEX, "arret d'urgence -> URGENCE_ATEX");
  verifier(Menu_Index == MENU_REARMEMENT, "option Rearmement presentee d'office");
  appuyer(PIN_BTN_OK);
  verifier(etat_courant == ETAT_URGENCE_ATEX, "rearmement menu refuse : AU toujours enfonce");

  mockIO.entree_num[PIN_AU_URGENCE] = LOW;
  uneIteration();
  for (uint8_t i = 0; i < NB_CHAMPS_MENU; i++) appuyer(PIN_BTN_MENU);
  verifier(Menu_Index == MENU_REARMEMENT, "navigation menu : tour complet (17 champs)");
  appuyer(PIN_BTN_OK);
  verifier(etat_courant == ETAT_ATTENTE_DEMARRAGE, "rearmement par le menu -> ATTENTE_DEMARRAGE");
}

/* --------------------------------------------------------------------------
 *  CAS 13 — MODE_SOLAIRE strictement passif
 * ------------------------------------------------------------------------*/
static void cas_mode_solaire_passif() {
  ouvrirCas("13. MODE_SOLAIRE : aucune electrovanne, ventilateurs seuls");
  initBanc();
  Config.Mode_Auto = true;
  reglerTcap(70.0f);
  rafraichirCapteurs();

  appuyer(PIN_BTN_START);
  verifier(etat_courant == ETAT_MODE_SOLAIRE, "arbitrage auto -> solaire (priorite 1)");
  avancer(5000);
  verifier(!V_H2 && !V_But && !EV1 && !EV2 && !EV3, "aucune vanne, aucune EV");
  verifier(PWM_Purge == PWM_ARRET, "purge a 0");
  verifier(PWM_Distrib == PWM_DISTRIB_SOLAIRE, "distribution elevee");

  reglerTsec(48.0f);
  reglerTcap(30.0f);
  rafraichirCapteurs();
  verifier(etat_courant == ETAT_MODE_H2, "solaire indisponible -> combustion H2");
  verifier(palier == PALIER_100, "demarrage a froid : retour a 100%");
}

/* --------------------------------------------------------------------------
 *  CAS 14 — Verrou de securite : jamais V_H2 et V_But ensemble (§19)
 * ------------------------------------------------------------------------*/
static void cas_verrou_exclusion_mutuelle() {
  ouvrirCas("14. Verrou de securite : exclusion mutuelle V_H2 / V_But");
  initBanc();
  V_H2 = true;
  V_But = true;   // incoherence deliberement forcee
  ecrireSorties();
  verifier(!V_H2 && !V_But, "les deux vannes source sont refermees en cas d'incoherence");
  verifier(mockIO.sortie_num[PIN_V_H2] == LOW, "broche V_H2 physiquement basse");
  verifier(mockIO.sortie_num[PIN_V_BUT] == LOW, "broche V_But physiquement basse");
}

/* --------------------------------------------------------------------------
 *  CAS 15 — Menu LCD + persistance EEPROM (§7, §17)
 * ------------------------------------------------------------------------*/
static void cas_menu_et_eeprom() {
  ouvrirCas("15. Menu LCD : navigation/edition + sauvegarde et rechargement EEPROM");
  initBanc();

  appuyer(PIN_BTN_MENU);
  verifier(etat_courant == ETAT_CONFIG_MENU, "Btn_Menu -> CONFIG_MENU");

  while (Menu_Index != MENU_T_CIBLE) appuyer(PIN_BTN_MENU);
  float avant = Config.T_cible;
  appuyer(PIN_BTN_UP);
  appuyer(PIN_BTN_UP);
  verifierEgalFloat(Config.T_cible, avant + 2.0f * PAS_TEMPERATURE, "incrementation de T_cible");

  while (Menu_Index != MENU_PRESS_H2_MIN) appuyer(PIN_BTN_MENU);
  float avant_press = Config.Press_H2_Min;
  appuyer(PIN_BTN_UP);
  verifierEgalFloat(Config.Press_H2_Min, avant_press + PAS_PRESSION, "Press_H2_Min modifiable au menu (§7)");

  appuyer(PIN_BTN_OK);
  verifier(etat_courant == ETAT_ATTENTE_DEMARRAGE, "Btn_OK valide et quitte le menu");

  /* La configuration doit avoir ete sauvegardee en EEPROM : on simule un
   * redemarrage complet (chargerConfig() relit depuis l'EEPROM mock). */
  float t_cible_avant_redemarrage = Config.T_cible;
  float press_avant_redemarrage = Config.Press_H2_Min;
  chargerConfig();
  verifierEgalFloat(Config.T_cible, t_cible_avant_redemarrage, "T_cible persistee en EEPROM");
  verifierEgalFloat(Config.Press_H2_Min, press_avant_redemarrage, "Press_H2_Min persistee en EEPROM");
}

/* --------------------------------------------------------------------------
 *  CAS 16 — Arret propre par Btn_Stop
 * ------------------------------------------------------------------------*/
static void cas_arret_propre() {
  ouvrirCas("16. Btn_Stop : arret propre du cycle depuis la combustion");
  initBanc();
  demarrerEtAllumer();
  verifier(EV1 && EV2 && EV3, "bruleur en fonctionnement");

  appuyer(PIN_BTN_STOP);
  verifier(etat_courant == ETAT_SECHAGE_TERMINE, "arret ordonne -> SECHAGE_TERMINE");
  verifier(!EV1 && !EV2 && !EV3 && !V_H2 && !V_But, "gaz ferme");
}

/* --------------------------------------------------------------------------
 *  main
 * ------------------------------------------------------------------------*/
int main() {
  printf("\n=== BANC DE TESTS — SECHOIR SOLAIRE HYBRIDE v2 (FSM) ===\n\n");

  cas_demarrage_a_froid();
  cas_hysteresis_quatre_niveaux();
  cas_echec_allumage();
  cas_perte_flamme();
  cas_echecs_repetes_escalade();
  cas_bascule_h2_gpl_reussie();
  cas_bascule_echouee();
  cas_retour_automatique_gpl_h2();
  cas_fin_de_cycle_verrouillee();
  cas_fin_temporisation();
  cas_prolongation_plafonnee();
  cas_urgence_et_rearmement();
  cas_mode_solaire_passif();
  cas_verrou_exclusion_mutuelle();
  cas_menu_et_eeprom();
  cas_arret_propre();

  printf("\n-----------------------------------------------------\n");
  printf("Verifications : %d   Echecs : %d\n", nb_verif, nb_echecs);
  printf("Resultat      : %s\n", nb_echecs == 0 ? "TOUS LES TESTS PASSENT" : "ECHEC");
  printf("-----------------------------------------------------\n\n");
  return nb_echecs == 0 ? 0 : 1;
}
