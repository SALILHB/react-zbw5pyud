/* ============================================================================
 *  BANC DE TESTS — Séchoir solaire hybride (§11.4)
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

#include "../sechoir_hybride/sechoir_hybride.ino"

#include <cstdio>

/* --------------------------------------------------------------------------
 *  Micro-cadre de test
 * ------------------------------------------------------------------------*/
static int nb_verif = 0;
static int nb_echecs = 0;
static const char* cas_courant = "";

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

static void ouvrirCas(const char* nom) {
  cas_courant = nom;
  printf("  %s\n", nom);
}

/* --------------------------------------------------------------------------
 *  Pilotage du banc
 * ------------------------------------------------------------------------*/

/* Fait tourner loop() en avançant l'horloge par pas de `pas` ms. */
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

/* Une seule itération de loop(). */
static void uneIteration() { avancer(20, 20); }

/* Laisse le temps à la lecture périodique des capteurs lents de s'appliquer. */
static void rafraichirCapteurs() { avancer(1200); }

static void appuyer(uint8_t pin) {
  mockIO.entree_num[pin] = LOW;
  avancer(60);
  mockIO.entree_num[pin] = HIGH;
  avancer(60);
}

/* Valeur ADC correspondant à une pression donnée (transmetteur 4-20 mA). */
static int adcPression(float bar) {
  if (bar <= 0.0f) return ADC_PRESS_4MA;
  return ADC_PRESS_4MA + (int)(bar / PRESS_H2_PLEINE_ECHELLE *
                               (float)(ADC_PRESS_20MA - ADC_PRESS_4MA));
}

static void reglerTsec(float valeur) { mock_ds18b20[IDX_SONDE_T_SEC] = valeur; }
static void reglerTcap(float valeur) { mock_ds18b20[IDX_SONDE_T_CAP] = valeur; }
static void reglerHextr(float valeur) { mock_dht_hum[PIN_DHT_EXTR] = valeur; }
static void reglerHamb(float valeur)  { mock_dht_hum[PIN_DHT_AMB] = valeur; }
static void reglerFlamme(bool presente) {
  mockIO.entree_num[PIN_FLAMME] = presente ? NIVEAU_FLAMME_PRESENTE
                                           : !NIVEAU_FLAMME_PRESENTE;
}

/* Remet le banc et la carte dans un état connu, puis applique les consignes
 * de référence du cahier des charges (§4.3 : 25 °C -> 55 °C). */
static void initBanc() {
  mockIO.reinitialiser();
  Serial.lignes.clear();

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
  reglerHextr(90.0f);

  setup();

  Mode_Auto       = false;                    // mode manuel par défaut au banc
  Choix_Mode      = 2;                        // H2
  T_cible         = 55.0f;
  T_init          = 25.0f;
  H_produit_cible = 10.0f;
  Duree_Max_Cycle = 600UL;
}

/* Démarre un cycle et amène le brûleur jusqu'au palier (purge + allumage). */
static void demarrerEtAllumer() {
  appuyer(PIN_BTN_START);
  avancer(PURGE_DUREE + 200);                 // purge intégrale -> ALLUMAGE
  reglerFlamme(true);
  avancer(100);
}

/* --------------------------------------------------------------------------
 *  CAS 1 — Démarrage à froid : PURGE -> ALLUMAGE -> PALIER 100 % (§7.2, §4.1)
 * ------------------------------------------------------------------------*/
static void cas_demarrage_a_froid() {
  ouvrirCas("1. Demarrage a froid : purge 120 s -> allumage -> palier 100 %");
  initBanc();

  appuyer(PIN_BTN_START);
  verifier(etat_courant == ETAT_MODE_H2, "entree en MODE_H2");
  verifier(phase_combustion == PH_PURGE, "phase initiale = PURGE");
  verifier(palier == PALIER_100, "demarrage toujours au palier 100 %");
  verifier(!V_H2 && !V_But, "vannes source fermees pendant la purge");
  verifier(!EV1 && !EV2 && !EV3, "aucune EV ouverte pendant la purge");
  verifier(!Spark, "Spark inactif pendant la purge");
  verifier(PWM_Purge == PWM_PURGE_BALAYAGE, "ventilation de purge maximale (§8)");

  /* Aucune ouverture de gaz avant l'echeance des 120 s. */
  avancer(PURGE_DUREE - 2000);
  verifier(phase_combustion == PH_PURGE, "toujours en purge a 118 s");
  verifier(!EV1 && !EV2 && !EV3, "gaz toujours ferme a 118 s (§7.3 regle 1)");

  avancer(2500);
  verifier(phase_combustion == PH_ALLUMAGE, "passage en ALLUMAGE a 120 s");
  verifier(V_H2 && !V_But, "vanne source H2 ouverte, GPL fermee");
  verifier(EV1 && EV2 && EV3, "ouverture au palier courant : 3 EV");
  verifier(Spark, "Spark actif pendant l'allumage");

  reglerFlamme(true);
  uneIteration();
  verifier(phase_combustion == PH_PALIER_100, "flamme confirmee -> PALIER_100");
  verifier(!Spark, "Spark coupe des la confirmation de flamme");
  verifier(EV1 && EV2 && EV3, "palier 100 % : EV1+EV2+EV3");
  verifierEgalFloat(T1, 35.0f, "seuil T1 (§4.3)");
  verifierEgalFloat(T2, 45.0f, "seuil T2 (§4.3)");
}

/* --------------------------------------------------------------------------
 *  CAS 2 — Les quatre transitions d'hysteresis + non-basculement (§4.2)
 *  T_init = 25, T_cible = 55 -> T1 = 35, T2 = 45
 *  Seuils effectifs : 32,5 / 37,5 / 42,5 / 47,5
 * ------------------------------------------------------------------------*/
static void cas_hysteresis() {
  ouvrirCas("2. Hysteresis 5 C : 4 transitions + non-basculement dans la bande");
  initBanc();
  demarrerEtAllumer();
  verifier(palier == PALIER_100, "point de depart : palier 100 %");

  reglerTsec(37.0f);  rafraichirCapteurs();
  verifier(palier == PALIER_100, "37,0 C < T1+2,5 : PAS de basculement");
  verifier(EV1 && EV2 && EV3, "EV1 toujours ouverte dans la bande");

  reglerTsec(37.5f);  rafraichirCapteurs();
  verifier(palier == PALIER_67, "T_sec >= T1+2,5 : 100 % -> 67 % (fermeture EV1)");
  verifier(!EV1 && EV2 && EV3, "palier 67 % : EV2+EV3");

  reglerTsec(47.0f);  rafraichirCapteurs();
  verifier(palier == PALIER_67, "47,0 C < T2+2,5 : PAS de basculement");

  reglerTsec(47.5f);  rafraichirCapteurs();
  verifier(palier == PALIER_33, "T_sec >= T2+2,5 : 67 % -> 33 % (fermeture EV2)");
  verifier(!EV1 && !EV2 && EV3, "palier 33 % : EV3 seule");

  reglerTsec(43.0f);  rafraichirCapteurs();
  verifier(palier == PALIER_33, "43,0 C >= T2-2,5 : PAS de basculement");

  reglerTsec(42.4f);  rafraichirCapteurs();
  verifier(palier == PALIER_67, "T_sec < T2-2,5 : 33 % -> 67 % (reouverture EV2)");
  verifier(!EV1 && EV2 && EV3, "palier 67 % retrouve");

  reglerTsec(33.0f);  rafraichirCapteurs();
  verifier(palier == PALIER_67, "33,0 C >= T1-2,5 : PAS de basculement");

  reglerTsec(32.4f);  rafraichirCapteurs();
  verifier(palier == PALIER_100, "T_sec < T1-2,5 : 67 % -> 100 % (reouverture EV1)");
  verifier(EV1 && EV2 && EV3, "palier 100 % retrouve");
}

/* --------------------------------------------------------------------------
 *  CAS 3 — Plancher a 33 % : jamais d'arret total (§4.1, §12)
 * ------------------------------------------------------------------------*/
static void cas_plancher_33() {
  ouvrirCas("3. Plancher 33 % : aucune extinction meme tres au-dessus de la consigne");
  initBanc();
  demarrerEtAllumer();

  reglerTsec(80.0f);                        // 25 C au-dessus de T_cible
  for (int i = 0; i < 10; i++) {
    rafraichirCapteurs();
    verifier(palier == PALIER_33, "palier bloque a 33 %, pas de valeur d'arret");
    verifier(EV3, "EV3 reste ouverte : il n'existe pas de palier 0 %");
    verifier(V_H2, "vanne source toujours ouverte");
    verifier(phase_combustion == PH_PALIER_33, "phase PALIER_33 maintenue");
  }
  verifier(etat_courant == ETAT_MODE_H2, "aucune extinction / rallumage cyclique");
}

/* --------------------------------------------------------------------------
 *  CAS 4 — Echec d'allumage apres 4 s (§7.2)
 * ------------------------------------------------------------------------*/
static void cas_echec_allumage() {
  ouvrirCas("4. Echec d'allumage : pas de flamme sous 4 s -> fermeture -> PURGE");
  initBanc();

  appuyer(PIN_BTN_START);
  avancer(PURGE_DUREE + 200);
  verifier(phase_combustion == PH_ALLUMAGE, "phase ALLUMAGE atteinte");

  avancer(3000);                             // 3,2 s : encore dans le delai
  verifier(phase_combustion == PH_ALLUMAGE, "toujours en allumage avant 4 s");
  verifier(EV1 && EV2 && EV3, "EV ouvertes pendant la tentative d'allumage");

  avancer(1200);                             // au-dela de 4 s
  verifier(phase_combustion == PH_PURGE, "echec -> retour PURGE");
  verifier(!EV1 && !EV2 && !EV3, "fermeture totale des EV");
  verifier(!V_H2 && !V_But, "fermeture des vannes source");
  verifier(!Spark, "Spark coupe");
  verifier(nb_echecs_allumage == 1, "echec comptabilise");
  verifier(PWM_Purge == PWM_PURGE_BALAYAGE, "nouvelle purge a ventilation maximale");

  /* La nouvelle purge dure a nouveau 120 s pleines. */
  avancer(PURGE_DUREE - 5000);
  verifier(phase_combustion == PH_PURGE, "purge complete re-imposee avant nouvel essai");
}

/* --------------------------------------------------------------------------
 *  CAS 5 — Perte de flamme inattendue (§7.3 regle 2)
 * ------------------------------------------------------------------------*/
static void cas_perte_flamme() {
  ouvrirCas("5. Perte de flamme : fermeture des EV dans l'iteration meme");
  initBanc();
  demarrerEtAllumer();
  verifier(phase_combustion == PH_PALIER_100, "brûleur en regulation");
  verifier(EV1 && EV2 && EV3, "EV ouvertes avant la perte de flamme");

  reglerFlamme(false);
  uneIteration();                            // UNE SEULE iteration de loop()
  verifier(!EV1 && !EV2 && !EV3, "EV refermees immediatement (pas un cycle de retard)");
  verifier(!V_H2 && !V_But, "vannes source refermees immediatement");
  verifier(!Spark, "Spark inactif");
  verifier(phase_combustion == PH_PURGE, "retour PURGE");
  verifier(mockIO.sortie_num[PIN_EV1] == LOW, "broche EV1 physiquement retombee");
  verifier(mockIO.sortie_num[PIN_EV3] == LOW, "broche EV3 physiquement retombee");
}

/* --------------------------------------------------------------------------
 *  CAS 6 — Bascule H2 -> GPL : palier CONSERVE, purge obligatoire (§7.3)
 * ------------------------------------------------------------------------*/
static void cas_bascule_h2_gpl() {
  ouvrirCas("6. Bascule H2 -> GPL : palier conserve + purge de 120 s");
  initBanc();
  Mode_Auto = true;                          // l'arbitrage n'existe qu'en auto
  reglerTcap(20.0f);                         // solaire indisponible
  rafraichirCapteurs();

  demarrerEtAllumer();
  verifier(etat_courant == ETAT_MODE_H2, "cycle demarre sur H2");
  verifier(Source_Active == SRC_H2, "Source_Active = H2");

  reglerTsec(40.0f);                         // amene le brûleur au palier 67 %
  rafraichirCapteurs();
  verifier(palier == PALIER_67, "regulation etablie au palier 67 %");

  /* Reservoir H2 epuise. */
  mockIO.entree_ana[PIN_PRESS_H2] = adcPression(0.5f);
  uneIteration();
  verifier(etat_courant == ETAT_MODE_GPL, "bascule vers MODE_GPL");
  verifier(Source_Active == SRC_GPL, "Source_Active = GPL");
  verifier(palier == PALIER_67, "PALIER CONSERVE a 67 % (pas de retour a 100 %)");
  verifier(phase_combustion == PH_PURGE, "purge obligatoire avant remise en gaz");
  verifier(!EV1 && !EV2 && !EV3, "gaz ferme pendant la purge de bascule");
  verifier(!V_H2 && !V_But, "les deux vannes source fermees");

  reglerFlamme(false);
  avancer(PURGE_DUREE + 200);
  verifier(phase_combustion == PH_ALLUMAGE, "reallumage apres 120 s");
  verifier(!V_H2 && V_But, "vanne source GPL ouverte, H2 fermee");
  verifier(!EV1 && EV2 && EV3, "reallumage AU PALIER 67 % (EV2+EV3)");

  reglerFlamme(true);
  uneIteration();
  verifier(phase_combustion == PH_PALIER_67, "reprise de la regulation a 67 %");
  verifier(palier == PALIER_67, "palier toujours 67 % apres reallumage");
}

/* --------------------------------------------------------------------------
 *  CAS 7 — Fin de cycle sur H_extr <= H_fin, H_fin recalcule (§5)
 * ------------------------------------------------------------------------*/
static void cas_fin_de_cycle() {
  ouvrirCas("7. Fin de cycle : H_extr <= H_fin, avec H_fin recalcule en continu");
  initBanc();
  demarrerEtAllumer();

  verifierEgalFloat(H_fin, 45.0f, "H_fin = H_produit_cible + H_amb = 10 + 35");

  /* H_amb varie : H_fin doit suivre immediatement. */
  reglerHamb(20.0f);
  rafraichirCapteurs();
  verifierEgalFloat(H_fin, 30.0f, "H_fin recalcule apres variation de H_amb");

  reglerHextr(44.0f);
  rafraichirCapteurs();
  verifier(etat_courant == ETAT_MODE_H2, "44 % > H_fin (30 %) : le cycle continue");

  reglerHamb(35.0f);                         // H_fin redevient 45 %
  rafraichirCapteurs();
  verifierEgalFloat(H_fin, 45.0f, "H_fin revenu a 45 %");
  verifier(etat_courant == ETAT_SECHAGE_TERMINE, "H_extr (44) <= H_fin (45) : cycle termine");
  verifier(!EV1 && !EV2 && !EV3, "gaz ferme en fin de cycle");
  verifier(!V_H2 && !V_But, "vannes source fermees");
  verifier(PWM_Extract == PWM_EXTRACT_REFROID, "extraction faible pour refroidir (§8)");
  verifier(PWM_Distrib == PWM_ARRET, "distribution a l'arret (§8)");
}

/* --------------------------------------------------------------------------
 *  CAS 8 — PROLONGATION sur depassement de Duree_Max_Cycle (§6)
 * ------------------------------------------------------------------------*/
static void cas_prolongation() {
  ouvrirCas("8. Prolongation automatique, sans nouvelle limite de temps");
  initBanc();
  Duree_Max_Cycle = 3UL;                     // 3 min pour le banc
  reglerTsec(30.0f);
  rafraichirCapteurs();

  demarrerEtAllumer();                       // ~121 s consommees par la purge
  verifier(etat_courant == ETAT_MODE_H2, "cycle en cours avant l'echeance");
  verifier(phase_combustion == PH_PALIER_100, "regulation etablie");

  avancer(70000);                            // depasse les 180 s de cycle
  verifier(etat_courant == ETAT_PROLONGATION, "passage automatique en PROLONGATION");
  verifier(Source_Active == SRC_H2, "source memorisee pour la prolongation");
  verifier(phase_combustion == PH_PALIER_100, "aucune nouvelle purge : regulation poursuivie");
  verifier(EV1 && EV2 && EV3, "electrovannes toujours pilotees par le palier");
  verifier(V_H2, "vanne source toujours ouverte");

  avancer(400000);                           // tres au-dela : aucune limite
  verifier(etat_courant == ETAT_PROLONGATION, "la prolongation n'a pas de duree propre");

  reglerHextr(40.0f);                        // H_fin = 45 %
  rafraichirCapteurs();
  verifier(etat_courant == ETAT_SECHAGE_TERMINE, "la prolongation s'acheve sur H_fin");
}

/* --------------------------------------------------------------------------
 *  CAS 9 — URGENCE_ATEX prioritaire + les DEUX chemins de rearmement (§7.4)
 * ------------------------------------------------------------------------*/
static void cas_urgence_et_rearmement() {
  ouvrirCas("9. URGENCE_ATEX prioritaire + rearmement materiel ET par menu");
  initBanc();
  demarrerEtAllumer();
  verifier(EV1 && EV2 && EV3, "brûleur en fonctionnement avant l'alarme");

  /* --- Declenchement sur fuite H2 --- */
  mockIO.entree_ana[PIN_MQ8_H2] = 600;
  uneIteration();
  verifier(etat_courant == ETAT_URGENCE_ATEX, "MQ8 > seuil -> URGENCE_ATEX");
  verifier(!EV1 && !EV2 && !EV3, "fermeture immediate de toutes les EV");
  verifier(!V_H2 && !V_But, "fermeture des vannes source");
  verifier(!Spark, "Spark = 0");
  verifier(Buzzer, "buzzer actif");
  verifier(PWM_Purge == 255, "purge a 255 (§8)");
  verifier(PWM_Distrib == 0, "distribution a 0 (§8)");
  verifier(PWM_Extract == 255, "extraction a 255 (§8)");
  verifier(mockIO.sortie_pwm[PIN_PWM_PURGE] == 255, "PWM purge physiquement a 255");
  verifier(mockIO.sortie_num[PIN_BUZZER] == HIGH, "buzzer physiquement actif");

  /* --- Rearmement refuse tant que la cause persiste --- */
  appuyer(PIN_BTN_REARM);
  verifier(etat_courant == ETAT_URGENCE_ATEX, "rearmement refuse : fuite toujours presente");

  /* --- Chemin 1 : bouton de rearmement materiel --- */
  mockIO.entree_ana[PIN_MQ8_H2] = 50;
  uneIteration();
  verifier(etat_courant == ETAT_URGENCE_ATEX, "l'etat d'urgence n'est pas quitte tout seul");
  appuyer(PIN_BTN_REARM);
  verifier(etat_courant == ETAT_ATTENTE_DEMARRAGE, "rearmement materiel -> ATTENTE_DEMARRAGE");
  verifier(!Buzzer, "buzzer coupe apres rearmement");

  /* --- Declenchement sur arret d'urgence, puis chemin 2 : menu LCD --- */
  mockIO.entree_num[PIN_AU_URGENCE] = HIGH;  // contact NC ouvert = AU enfonce
  uneIteration();
  verifier(etat_courant == ETAT_URGENCE_ATEX, "arret d'urgence -> URGENCE_ATEX");
  verifier(Menu_Index == MENU_REARMEMENT, "l'option Rearmement est presentee d'office");

  appuyer(PIN_BTN_OK);
  verifier(etat_courant == ETAT_URGENCE_ATEX, "rearmement menu refuse : AU toujours enfonce");

  mockIO.entree_num[PIN_AU_URGENCE] = LOW;   // AU deverrouille
  uneIteration();
  /* Tour complet du menu : on verifie au passage la navigation. */
  for (uint8_t i = 0; i < NB_CHAMPS_MENU; i++) appuyer(PIN_BTN_MENU);
  verifier(Menu_Index == MENU_REARMEMENT, "navigation menu : tour complet");
  appuyer(PIN_BTN_OK);
  verifier(etat_courant == ETAT_ATTENTE_DEMARRAGE, "rearmement par le menu -> ATTENTE_DEMARRAGE");
  verifier(!Buzzer, "buzzer coupe");
  verifier(!EV1 && !EV2 && !EV3 && !V_H2 && !V_But, "installation fermee au repos");
}

/* --------------------------------------------------------------------------
 *  CAS 10 — MODE_SOLAIRE strictement passif (§2, §12)
 * ------------------------------------------------------------------------*/
static void cas_mode_solaire_passif() {
  ouvrirCas("10. MODE_SOLAIRE : aucune electrovanne, ventilateurs seuls");
  initBanc();
  Mode_Auto = true;
  reglerTcap(70.0f);                         // capteur solaire chaud
  rafraichirCapteurs();

  appuyer(PIN_BTN_START);
  verifier(etat_courant == ETAT_MODE_SOLAIRE, "arbitrage auto -> solaire (priorite 1)");
  avancer(5000);
  verifier(!V_H2 && !V_But, "aucune vanne source");
  verifier(!EV1 && !EV2 && !EV3, "aucune electrovanne de combustible");
  verifier(!Spark, "aucun allumage");
  verifier(PWM_Purge == PWM_ARRET, "purge a 0 (§8)");
  verifier(PWM_Distrib == PWM_DISTRIB_SOLAIRE, "distribution elevee (§8)");
  verifier(PWM_Extract == PWM_EXTRACT_SOLAIRE, "extraction moyenne (§8)");

  /* Le soleil disparait : passage en combustion, DEMARRAGE A FROID a 100 %. */
  reglerTsec(48.0f);                         // au-dessus de T2+2,5 : piege a regression
  reglerTcap(30.0f);
  rafraichirCapteurs();
  verifier(etat_courant == ETAT_MODE_H2, "solaire indisponible -> combustion H2");
  verifier(palier == PALIER_100, "demarrage a froid : retour a 100 % (§7.3 regle 3)");
  verifier(phase_combustion == PH_PURGE, "purge de 120 s avant mise en gaz");
}

/* --------------------------------------------------------------------------
 *  CAS 11 — Menu LCD : navigation, edition, validation (§3.3)
 * ------------------------------------------------------------------------*/
static void cas_menu_lcd() {
  ouvrirCas("11. Menu LCD : navigation, edition d'une consigne, validation");
  initBanc();

  appuyer(PIN_BTN_MENU);
  verifier(etat_courant == ETAT_CONFIG_MENU, "Btn_Menu -> CONFIG_MENU");
  verifier(Menu_Index == MENU_MODE_AUTO, "premier champ selectionne");

  appuyer(PIN_BTN_MENU);
  appuyer(PIN_BTN_MENU);
  verifier(Menu_Index == MENU_T_CIBLE, "navigation jusqu'a T_cible");

  float avant = T_cible;
  appuyer(PIN_BTN_UP);
  appuyer(PIN_BTN_UP);
  verifierEgalFloat(T_cible, avant + 2.0f * PAS_TEMPERATURE, "incrementation de T_cible");
  appuyer(PIN_BTN_DOWN);
  verifierEgalFloat(T_cible, avant + PAS_TEMPERATURE, "decrementation de T_cible");

  appuyer(PIN_BTN_OK);
  verifier(etat_courant == ETAT_ATTENTE_DEMARRAGE, "Btn_OK valide et quitte le menu");
  verifier(!EV1 && !EV2 && !EV3, "aucune sortie gaz pendant la configuration");
  verifier(PWM_Distrib == PWM_ARRET, "ventilateurs a l'arret en attente (§8)");
}

/* --------------------------------------------------------------------------
 *  CAS 12 — Arret propre par Btn_Stop
 * ------------------------------------------------------------------------*/
static void cas_arret_propre() {
  ouvrirCas("12. Btn_Stop : arret propre du cycle depuis la combustion");
  initBanc();
  demarrerEtAllumer();
  verifier(EV1 && EV2 && EV3, "brûleur en fonctionnement");

  appuyer(PIN_BTN_STOP);
  verifier(etat_courant == ETAT_SECHAGE_TERMINE, "arret ordonne -> SECHAGE_TERMINE");
  verifier(!EV1 && !EV2 && !EV3, "gaz ferme");
  verifier(!V_H2 && !V_But, "vannes source fermees");
}

/* --------------------------------------------------------------------------
 *  main
 * ------------------------------------------------------------------------*/
int main() {
  printf("\n=== BANC DE TESTS — SECHOIR SOLAIRE HYBRIDE (FSM) ===\n\n");

  cas_demarrage_a_froid();
  cas_hysteresis();
  cas_plancher_33();
  cas_echec_allumage();
  cas_perte_flamme();
  cas_bascule_h2_gpl();
  cas_fin_de_cycle();
  cas_prolongation();
  cas_urgence_et_rearmement();
  cas_mode_solaire_passif();
  cas_menu_lcd();
  cas_arret_propre();

  printf("\n-----------------------------------------------------\n");
  printf("Verifications : %d   Echecs : %d\n", nb_verif, nb_echecs);
  printf("Resultat      : %s\n", nb_echecs == 0 ? "TOUS LES TESTS PASSENT" : "ECHEC");
  printf("-----------------------------------------------------\n\n");
  (void)cas_courant;
  return nb_echecs == 0 ? 0 : 1;
}
