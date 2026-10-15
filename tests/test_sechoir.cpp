/* ============================================================================
 *  BANC DE TESTS — Séchoir solaire hybride v3
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
/* 2,5 s : couvre au moins une lecture capteurs (1 s) PUIS une comparaison de
 * régulation (Periode_Regul = 1 s au banc), quel que soit leur déphasage. */
static void rafraichirCapteurs() { avancer(2500); }

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
/* V3 : T_cap n'est plus lue sur la plaque mais estimée par T_amb + DeltaT_Sol.
 * Régler « T_cap » revient donc à régler T_amb en conséquence. */
static void reglerTamb(float valeur) { mock_dht_temp[PIN_DHT_AMB] = valeur; }
static void reglerTcap(float valeur) { reglerTamb(valeur - Config.DeltaT_Sol); }
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
  mock_ds18b20[IDX_SONDE_T_CAP] = 20.0f;      // sonde plaque : affichage seul en V3
  reglerTamb(25.0f);                          // T_cap estimée = 25 + 20 = 45 < ON : pas de solaire
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
  Config.Prolong_Defaut  = 30UL;              // min
  Config.Temps_Min_Fin   = 120UL;             // min (2 h)
  Config.Temps_Reponse   = 300UL;             // s
  Config.Temps_Purge     = 120UL;             // s
  Config.Temps_Allumage  = 4UL;               // s
  Config.Hhyst           = 5.0f;
  Config.Periode_Regul   = 1UL;               // s
  Config.Regul_Auto      = true;
  Config.Press_H2_Min    = 2.0f;
  Config.DeltaT_Sol      = 20.0f;             // T_cap = T_amb + 20
  Config.Marge_Sol       = 0.0f;              // ON = T_cible = 55
  Config.Hyst_Sol        = 5.0f;              // OFF = 50
  Config.Seuil_Chaud     = 40.0f;             // CHAUD si T_sec >= 40 (+/- 2,5)
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
  reglerTsec(52.6f);  // juste au-dessus de T3 - 2,5 = 52,5 : toujours en veille
  rafraichirCapteurs();
  verifier(phase_combustion == PH_PURGE, "52,6 C >= T3-2,5 : reste en veille");
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
  verifier(etat_courant == ETAT_MODE_H2, "1re perte : relance autorisee (pas d'urgence)");
  verifier(nb_pertes_flamme == 1, "1re perte comptabilisee");
  verifier(nb_echecs_allumage == 0, "une perte de flamme n'est pas un echec d'allumage");

  /* Relance : purge complete puis rallumage au meme palier. */
  avancer(purgeMs() + 200);
  verifier(phase_combustion == PH_ALLUMAGE, "relance : nouvel allumage apres purge");
  reglerFlamme(true);
  uneIteration();
  verifier(phase_combustion == PH_PALIER_100, "flamme retablie");

  /* 2e perte dans le meme cycle : verrouillage en URGENCE. */
  reglerFlamme(false);
  uneIteration();
  verifier(etat_courant == ETAT_URGENCE_ATEX, "2e perte de flamme -> URGENCE");
  verifier(cause_urgence == URG_PERTE_FLAMME, "cause affichee = PERTE FLAMME");
  verifier(!EV1 && !EV2 && !EV3 && !V_H2 && !V_But, "gaz ferme");
  appuyer(PIN_BTN_REARM);
  verifier(etat_courant == ETAT_ATTENTE_DEMARRAGE, "rearmement accepte (flamme eteinte, aucune fuite)");
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
 *  CAS 7 — Echec apres bascule : MEME regle de 3 essais que partout (V3)
 * ------------------------------------------------------------------------*/
static void cas_bascule_echouee() {
  ouvrirCas("7. Echec apres bascule : 3 essais (purge entre chaque) puis ERREUR");
  initBanc();
  Config.Mode_Auto = true;
  reglerTcap(20.0f);
  rafraichirCapteurs();
  demarrerEtAllumer();

  mockIO.entree_ana[PIN_PRESS_H2] = adcPression(0.5f);
  uneIteration();
  verifier(Source_Active == SRC_GPL, "bascule declenchee vers GPL");
  verifier(nb_echecs_allumage == 0, "la nouvelle source dispose d'un jeu complet d'essais");

  /* Le GPL est simule indisponible : la flamme ne doit JAMAIS se confirmer. */
  reglerFlamme(false);

  avancer(purgeMs() + 200);
  verifier(phase_combustion == PH_ALLUMAGE, "1er essai sur la nouvelle source");
  avancer(allumageMs() + 200);
  verifier(etat_courant == ETAT_MODE_GPL, "1er echec apres bascule : PAS d'erreur immediate (V3)");
  verifier(phase_combustion == PH_PURGE, "purge complete avant le 2e essai");

  for (uint16_t i = 1; i < MAX_ECHECS_AVANT_ALARME; i++) {
    avancer(purgeMs() + 200);
    avancer(allumageMs() + 200);
  }
  verifier(etat_courant == ETAT_ERREUR_COMBUSTION, "3e echec -> ERREUR_COMBUSTION");
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
  verifier(etat_courant == ETAT_DEMANDE_PROLONGATION, "Temps_Min_Fin ecoule + H_sec<=H_fin -> DEMANDE_PROLONGATION");
  verifier(motif_demande == MOTIF_HUMIDITE, "motif = HUMIDITE (critere prioritaire)");
}

/* --------------------------------------------------------------------------
 *  CAS 10 — DEMANDE_PROLONGATION apres humidite atteinte (V3)
 * ------------------------------------------------------------------------*/
static void cas_demande_prolongation() {
  ouvrirCas("10. Humidite atteinte -> demande de prolongation / sans reponse -> TERMINE");
  initBanc();
  Config.Temps_Min_Fin = 10UL;
  Config.Temps_Reponse = 60UL;              // 1 min pour le banc
  Config.Prolong_Defaut = 5UL;
  demarrerEtAllumer();

  avancer(Config.Temps_Min_Fin * 60000UL + 2000, 5000);
  reglerHsec(20.0f);
  rafraichirCapteurs();
  verifier(etat_courant == ETAT_DEMANDE_PROLONGATION, "humidite atteinte -> DEMANDE_PROLONGATION");
  verifier(motif_demande == MOTIF_HUMIDITE, "motif = HUMIDITE");
  verifier(EV1 || EV2 || EV3 || V_H2, "la regulation continue pendant la question");
  verifier(duree_prolongation_min == 5UL, "duree proposee = Prolong_Defaut");

  /* UP : +5 min, et le delai de reponse repart (operateur present). */
  avancer(40000);
  appuyer(PIN_BTN_UP);
  verifier(duree_prolongation_min == 10UL, "UP : duree proposee +5 min");
  avancer(40000);
  verifier(etat_courant == ETAT_DEMANDE_PROLONGATION, "le delai de reponse a ete relance par UP");

  /* OK : prolongation de 10 min ; l'humidite deja atteinte ne la coupe pas. */
  appuyer(PIN_BTN_OK);
  verifier(etat_courant == ETAT_PROLONGATION, "OK -> PROLONGATION");
  avancer(5UL * 60000UL, 5000);
  verifier(etat_courant == ETAT_PROLONGATION, "humidite deja atteinte : les N min choisies sont respectees");
  avancer(5UL * 60000UL + 5000, 5000);
  verifier(etat_courant == ETAT_DEMANDE_PROLONGATION, "fin des N min -> on redemande");
  verifier(motif_demande == MOTIF_FIN_PROLONGATION, "motif = FIN_PROLONGATION");

  /* Aucune reponse pendant Temps_Reponse : fin du sechage, bruleur a 0 %.
   * Pas fin (500 ms) : le mock ne simule pas l'extinction physique de la
   * flamme apres fermeture du gaz, il ne faut donc pas depasser le delai
   * DELAI_FLAMME_PARASITE_MS avant de verifier. */
  avancer(chrono_demande + Config.Temps_Reponse * 1000UL + 1000 - t_boucle, 500);
  verifier(etat_courant == ETAT_SECHAGE_TERMINE, "sans reponse -> SECHAGE_TERMINE");
  verifier(!EV1 && !EV2 && !EV3 && !V_H2 && !V_But, "gaz ferme (0 %)");

  /* STOP pendant la question : fin immediate (cycle independant). */
  initBanc();
  Config.Temps_Min_Fin = 10UL;
  demarrerEtAllumer();
  avancer(Config.Temps_Min_Fin * 60000UL + 2000, 5000);
  reglerHsec(20.0f);
  rafraichirCapteurs();
  verifier(etat_courant == ETAT_DEMANDE_PROLONGATION, "demande affichee (2e cycle)");
  appuyer(PIN_BTN_STOP);
  verifier(etat_courant == ETAT_SECHAGE_TERMINE, "STOP -> fin immediate");
}

/* --------------------------------------------------------------------------
 *  CAS 11 — Duree max atteinte -> demande ; humidite atteinte PENDANT la
 *  prolongation -> on redemande tout de suite (V3)
 * ------------------------------------------------------------------------*/
static void cas_duree_max_puis_humidite() {
  ouvrirCas("11. Duree max -> demande ; humidite atteinte en prolongation -> redemande");
  initBanc();
  Config.Duree_Max_Cycle = 15UL;        // minimum autorise
  Config.Temps_Min_Fin = 0UL;
  Config.Prolong_Defaut = 30UL;
  demarrerEtAllumer();
  reglerHsec(80.0f);                    // H_sec > H_fin : humidite non atteinte
  rafraichirCapteurs();

  uint32_t echeance_ms = Config.Duree_Max_Cycle * 60000UL;
  uint32_t ecoule_ms = t_boucle - chrono_cycle;
  verifier(ecoule_ms < echeance_ms, "pre-requis : Duree_Max_Cycle pas deja ecoulee");
  avancer(echeance_ms - ecoule_ms - 60000UL, 5000);
  verifier(etat_courant == ETAT_MODE_H2, "avant Duree_Max_Cycle : fonctionnement normal");
  avancer(2UL * 60000UL, 5000);
  verifier(etat_courant == ETAT_DEMANDE_PROLONGATION, "Duree_Max_Cycle -> DEMANDE_PROLONGATION");
  verifier(motif_demande == MOTIF_DUREE_MAX, "motif = DUREE MAX");

  appuyer(PIN_BTN_OK);
  verifier(etat_courant == ETAT_PROLONGATION, "prolongation acceptee (30 min)");
  avancer(60000UL, 5000);
  verifier(etat_courant == ETAT_PROLONGATION, "toujours en prolongation");

  reglerHsec(20.0f);
  rafraichirCapteurs();
  verifier(etat_courant == ETAT_DEMANDE_PROLONGATION, "humidite atteinte en prolongation -> redemande");
  verifier(motif_demande == MOTIF_HUMIDITE, "motif = HUMIDITE ATTEINTE");
  verifier(V_H2 || EV3, "la combustion n'a jamais ete interrompue");
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
  verifier(cause_urgence == URG_FUITE_H2, "cause affichee = FUITE H2");
  reglerFlamme(false);   // gaz coupe : la flamme reelle s'eteint

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
  verifier(cause_urgence == URG_ARRET_URGENCE, "cause affichee = ARRET URGENCE");
  appuyer(PIN_BTN_OK);
  verifier(etat_courant == ETAT_URGENCE_ATEX, "rearmement menu refuse : AU toujours enfonce");

  mockIO.entree_num[PIN_AU_URGENCE] = LOW;
  uneIteration();
  for (uint8_t i = 0; i < NB_CHAMPS_MENU; i++) appuyer(PIN_BTN_MENU);
  verifier(Menu_Index == MENU_REARMEMENT, "navigation menu : tour complet de tous les champs");
  appuyer(PIN_BTN_OK);
  verifier(etat_courant == ETAT_ATTENTE_DEMARRAGE, "rearmement par le menu -> ATTENTE_DEMARRAGE");
}

/* --------------------------------------------------------------------------
 *  CAS 13 — MODE_SOLAIRE strictement passif, retour combustion en regime
 *  CHAUD (palier selon T_sec), retour solaire avec post-purge (V3)
 * ------------------------------------------------------------------------*/
static void cas_mode_solaire_passif() {
  ouvrirCas("13. Solaire passif ; bascule combustion en regime CHAUD ; post-purge");
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

  /* Le soleil a chauffe la chambre a 48 C (regime CHAUD), puis faiblit. */
  reglerTsec(48.0f);
  rafraichirCapteurs();
  verifier(regime_chaud, "T_sec 48 >= repere 40 + 2,5 : regime CHAUD");
  reglerTcap(52.0f);
  rafraichirCapteurs();
  verifier(etat_courant == ETAT_MODE_SOLAIRE, "CHAUD : solaire garde entre OFF (50) et ON (55)");
  reglerTcap(30.0f);
  rafraichirCapteurs();
  verifier(etat_courant == ETAT_MODE_H2, "sous OFF -> combustion H2");
  verifier(palier == PALIER_33, "CHAUD : palier d'allumage selon T_sec (48 C -> 33 %), pas 100 %");

  /* Allumage puis retour du soleil : extinction normale + post-purge. */
  avancer(purgeMs() + 200);
  reglerFlamme(true);
  uneIteration();
  verifier(phase_combustion == PH_PALIER_33, "combustion etablie a 33 %");
  reglerTcap(60.0f);
  rafraichirCapteurs();
  reglerFlamme(false);                  // la flamme s'eteint une fois le gaz coupe
  verifier(etat_courant == ETAT_MODE_SOLAIRE, "T_cap >= ON -> retour au solaire");
  verifier(!V_H2 && !V_But && !EV1 && !EV2 && !EV3, "gaz ferme (extinction normale)");
  verifier(PWM_Purge == PWM_PURGE_BALAYAGE, "post-purge en cours apres extinction");
  avancer(purgeMs() + 1000);
  verifier(PWM_Purge == PWM_ARRET, "post-purge terminee : ventilateurs solaires");
  verifier(etat_courant == ETAT_MODE_SOLAIRE, "pas d'urgence : extinction normale");
}

/* --------------------------------------------------------------------------
 *  CAS 17 — Repere FROID / CHAUD au demarrage (V3)
 * ------------------------------------------------------------------------*/
static void cas_repere_froid_chaud() {
  ouvrirCas("17. Repere FROID/CHAUD : seuil solaire ON (froid) ou OFF (chaud)");
  initBanc();
  Config.Mode_Auto = true;
  reglerTsec(25.0f);
  reglerTcap(52.0f);                    // entre OFF (50) et ON (55)
  rafraichirCapteurs();
  appuyer(PIN_BTN_START);
  verifier(!regime_chaud, "chambre a 25 C : regime FROID");
  verifier(etat_courant == ETAT_MODE_H2, "FROID : 52 < ON -> combustion (pas de zone morte)");
  verifier(palier == PALIER_100, "FROID : allumage a 100 %");

  initBanc();
  Config.Mode_Auto = true;
  reglerTsec(45.0f);
  reglerTcap(52.0f);
  rafraichirCapteurs();
  appuyer(PIN_BTN_START);
  verifier(regime_chaud, "chambre a 45 C : regime CHAUD");
  verifier(etat_courant == ETAT_MODE_SOLAIRE, "CHAUD : 52 >= OFF -> solaire accepte");

  /* La chambre refroidit sous le repere : retour en FROID, le solaire
   * faible (52 < ON) ne suffit plus -> combustion. */
  reglerTsec(37.0f);
  rafraichirCapteurs();
  verifier(!regime_chaud, "T_sec < 40 - 2,5 : regime FROID");
  verifier(etat_courant == ETAT_MODE_H2, "FROID + soleil sous ON -> combustion");
}

/* --------------------------------------------------------------------------
 *  CAS 18 — Flamme vue alors que le gaz est ferme -> URGENCE (V3)
 * ------------------------------------------------------------------------*/
static void cas_flamme_parasite() {
  ouvrirCas("18. Flamme detectee gaz ferme : URGENCE apres le delai d'extinction");
  initBanc();
  reglerFlamme(true);                   // gaz ferme (ATTENTE) mais flamme vue
  avancer(DELAI_FLAMME_PARASITE_MS - 1000);
  verifier(etat_courant == ETAT_ATTENTE_DEMARRAGE, "avant le delai : pas d'urgence");
  reglerFlamme(false);
  avancer(2000);
  reglerFlamme(true);
  avancer(DELAI_FLAMME_PARASITE_MS - 1000);
  verifier(etat_courant == ETAT_ATTENTE_DEMARRAGE, "chrono remis a zero quand la flamme disparait");
  avancer(2000);
  verifier(etat_courant == ETAT_URGENCE_ATEX, "flamme persistante gaz ferme -> URGENCE");
  verifier(cause_urgence == URG_FLAMME_PARASITE, "cause = FLAMME PARASITE");

  appuyer(PIN_BTN_REARM);
  verifier(etat_courant == ETAT_URGENCE_ATEX, "rearmement refuse tant que la flamme est vue");
  reglerFlamme(false);
  uneIteration();
  appuyer(PIN_BTN_REARM);
  verifier(etat_courant == ETAT_ATTENTE_DEMARRAGE, "rearmement accepte flamme eteinte");
}

/* --------------------------------------------------------------------------
 *  CAS 19 — Grandeurs FIXES (fonctionnement degrade) (V3)
 * ------------------------------------------------------------------------*/
static void cas_valeurs_fixes() {
  ouvrirCas("19. Grandeurs AUTO / FIXE : la valeur saisie remplace la mesure");
  initBanc();
  reglerHsec(90.0f);
  reglerTamb(25.0f);
  rafraichirCapteurs();
  verifierEgalFloat(H_sec, 90.0f, "AUTO : H_sec mesuree");
  verifierEgalFloat(T_cap, 45.0f, "AUTO : T_cap = T_amb mesuree + DeltaT_Sol");

  Config.Fixe_H_sec = true;  Config.Val_H_sec = 20.0f;
  Config.Fixe_T_amb = true;  Config.Val_T_amb = 40.0f;
  Config.Fixe_Press = true;  Config.Val_Press = 0.0f;
  rafraichirCapteurs();
  verifierEgalFloat(H_sec, 20.0f, "FIXE : H_sec = valeur saisie malgre le capteur");
  verifierEgalFloat(T_amb, 40.0f, "FIXE : T_amb = valeur saisie");
  verifierEgalFloat(T_cap, 60.0f, "T_cap estimee sur la T_amb fixe");
  verifierEgalFloat(Press_H2, 0.0f, "FIXE : pression saisie");
}

/* --------------------------------------------------------------------------
 *  CAS 20 — Palier IMPOSE (remplace un T_sec fixe, interdit) (V3)
 * ------------------------------------------------------------------------*/
static void cas_palier_impose() {
  ouvrirCas("20. Palier impose : pas de regulation par T_sec, surchauffe active");
  initBanc();
  Config.Regul_Auto = false;
  Config.Palier_Impose = PALIER_33;
  appuyer(PIN_BTN_START);
  verifier(palier == PALIER_33, "allumage au palier impose");
  avancer(purgeMs() + 200);
  reglerFlamme(true);
  uneIteration();
  verifier(phase_combustion == PH_PALIER_33, "combustion au palier impose 33 %");

  reglerTsec(60.0f);                    // au-dessus de T3 : en auto on couperait
  rafraichirCapteurs();
  verifier(palier == PALIER_33, "palier impose maintenu malgre T_sec > T_cible");

  reglerTsec(91.0f);
  rafraichirCapteurs();
  verifier(etat_courant == ETAT_SECHAGE_TERMINE, "securite surchauffe toujours active");
  verifier(!EV1 && !EV2 && !EV3 && !V_H2 && !V_But, "gaz ferme");
}

/* --------------------------------------------------------------------------
 *  CAS 21 — T_cible modifiee en cours de cycle + Periode_Regul (V3)
 * ------------------------------------------------------------------------*/
static void cas_tcible_en_cycle_et_periode() {
  ouvrirCas("21. T_cible modifiable en cycle (UP/DOWN) ; periode de regulation");
  initBanc();
  demarrerEtAllumer();
  verifierEgalFloat(T3, 55.0f, "T3 initial = T_cible 55");
  appuyer(PIN_BTN_UP);
  verifierEgalFloat(Config.T_cible, 56.0f, "UP en cycle : T_cible +1");
  verifierEgalFloat(T3, 56.0f, "seuils recalcules immediatement");
  verifierEgalFloat(Config.T_init, 25.0f, "T_init inchangee (reference du demarrage)");

  Config.Periode_Regul = 10UL;
  avancer(10000);                       // aligne la prochaine comparaison
  reglerTsec(40.0f);
  avancer(3000);
  verifier(palier == PALIER_100, "entre deux comparaisons : palier inchange");
  avancer(8000);
  verifier(palier == PALIER_67, "a la comparaison suivante : 100 % -> 67 %");
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
/* --------------------------------------------------------------------------
 *  CAS 22 — Bascule H2 -> GPL PENDANT la veille a 0 % (trouve par la
 *  simulation de reference, tests/simulation_scenarios.cpp) : la veille est
 *  conservee, on ne rallume JAMAIS a 0 % (aucune EV ouverte + etincelle).
 * ------------------------------------------------------------------------*/
static void cas_bascule_pendant_veille() {
  ouvrirCas("22. Bascule H2 -> GPL pendant la veille 0 % : veille conservee");
  initBanc();
  Config.Mode_Auto = true;
  reglerTcap(20.0f);
  rafraichirCapteurs();
  demarrerEtAllumer();

  reglerTsec(58.0f);                   // >= T3 + Hhyst/2 : consigne atteinte
  avancer(6000);
  verifier(palier == PALIER_0 && phase_combustion == PH_PURGE, "veille a 0 % etablie");
  reglerFlamme(false);                 // gaz ferme : plus de flamme

  mockIO.entree_ana[PIN_PRESS_H2] = adcPression(0.5f);   // H2 epuise pendant la veille
  uneIteration();
  verifier(Source_Active == SRC_GPL, "bascule vers GPL");

  bool gaz_ouvert = false;
  for (int i = 0; i < 200; i++) {      // purge de bascule + 10 s, T_sec toujours haute
    avancer(purgeMs() / 150);
    gaz_ouvert = gaz_ouvert || V_H2 || V_But || Spark;
  }
  verifier(!gaz_ouvert, "aucune ouverture de gaz ni etincelle tant que T_sec >= T3 - Hhyst/2");
  verifier(phase_combustion == PH_PURGE, "le GPL reste en veille");

  reglerTsec(52.0f);                   // < T3 - Hhyst/2 : la demande revient
  rafraichirCapteurs();
  verifier(phase_combustion == PH_ALLUMAGE, "rallumage GPL quand la demande revient");
  verifier(V_But && !V_H2 && EV3 && !EV2 && !EV1, "rallumage au palier 33 %");
}

int main() {
  printf("\n=== BANC DE TESTS — SECHOIR SOLAIRE HYBRIDE v3 (FSM) ===\n\n");

  cas_demarrage_a_froid();
  cas_hysteresis_quatre_niveaux();
  cas_echec_allumage();
  cas_perte_flamme();
  cas_echecs_repetes_escalade();
  cas_bascule_h2_gpl_reussie();
  cas_bascule_echouee();
  cas_retour_automatique_gpl_h2();
  cas_fin_de_cycle_verrouillee();
  cas_demande_prolongation();
  cas_duree_max_puis_humidite();
  cas_urgence_et_rearmement();
  cas_mode_solaire_passif();
  cas_verrou_exclusion_mutuelle();
  cas_menu_et_eeprom();
  cas_arret_propre();
  cas_repere_froid_chaud();
  cas_flamme_parasite();
  cas_valeurs_fixes();
  cas_palier_impose();
  cas_tcible_en_cycle_et_periode();
  cas_bascule_pendant_veille();

  printf("\n-----------------------------------------------------\n");
  printf("Verifications : %d   Echecs : %d\n", nb_verif, nb_echecs);
  printf("Resultat      : %s\n", nb_echecs == 0 ? "TOUS LES TESTS PASSENT" : "ECHEC");
  printf("-----------------------------------------------------\n\n");
  return nb_echecs == 0 ? 0 : 1;
}
