/* ============================================================================
 *  SÉCHOIR SOLAIRE HYBRIDE  —  Solaire / Hydrogène / GPL
 *  Commande de température par machine à états finis (FSM)
 *  Cible matérielle : Arduino Mega 2560
 *
 *  Ce fichier suit la structure imposée par le cahier des charges (§10) :
 *    1. Includes                      7.  setup()
 *    2. Broches                       8.  loop()
 *    3. Constantes de réglage (§9.6)  9.  lireEntrees()
 *    4. enum EtatFSM (§7.1)           10. appliquerPalier()
 *    5. enum PhaseCombustion (§7.2)   11. gererCombustion(bool utiliseH2)
 *    6. Variables (§9)                12. pasFSM()
 *                                     13. ecrireSorties()
 *                                     14. gererIHM()
 *
 *  Les références «§x» renvoient au cahier des charges du mémoire.
 *  Les points signalés «INCOHÉRENCE n°x» sont détaillés dans docs/FSM_SECHOIR.md.
 * ==========================================================================*/

/* --------------------------------------------------------------------------
 * 1. INCLUDES
 * ------------------------------------------------------------------------*/
#include <Arduino.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DHT.h>
#include <LiquidCrystal_I2C.h>

/* --------------------------------------------------------------------------
 * 2. DÉFINITION DES BROCHES (Arduino Mega 2560)
 * ------------------------------------------------------------------------*/

/* --- Entrées capteurs --- */
const uint8_t PIN_ONEWIRE     = 22;  // Bus DS18B20 : T_sec (index 0) + T_cap (index 1)
const uint8_t PIN_DHT_AMB     = 24;  // DHT22 entrée d'air extérieur  -> T_amb / H_amb
const uint8_t PIN_DHT_EXTR    = 26;  // DHT22 gaine d'extraction      -> H_extr
const uint8_t PIN_FLAMME      = 28;  // Contrôleur de flamme (sortie TOR)

/* --- Entrées opérateur (contacts vers 0 V + INPUT_PULLUP, sauf AU) --- */
const uint8_t PIN_BTN_MENU    = 30;
const uint8_t PIN_BTN_UP      = 32;
const uint8_t PIN_BTN_DOWN    = 34;
const uint8_t PIN_BTN_OK      = 36;
const uint8_t PIN_BTN_START   = 38;
const uint8_t PIN_BTN_STOP    = 40;
const uint8_t PIN_AU_URGENCE  = 42;  // Arrêt d'urgence : contact NC (§7.4, voir note sécurité)
const uint8_t PIN_BTN_REARM   = 44;  // Réarmement matériel (§7.4, chemin 1)

/* --- Sorties TOR (modules relais / drivers) --- */
const uint8_t PIN_V_H2        = 23;  // Électrovanne SOURCE hydrogène
const uint8_t PIN_V_BUT       = 25;  // Électrovanne SOURCE butane
const uint8_t PIN_EV1         = 27;  // Électrovanne COMBUSTIBLE 1 (rampe brûleur)
const uint8_t PIN_EV2         = 29;  // Électrovanne COMBUSTIBLE 2
const uint8_t PIN_EV3         = 31;  // Électrovanne COMBUSTIBLE 3 (plancher 33 %)
const uint8_t PIN_SPARK       = 33;  // Module d'allumage haute tension
const uint8_t PIN_BUZZER      = 35;  // Alarme sonore

/* --- Sorties PWM (broches PWM du Mega) --- */
const uint8_t PIN_PWM_PURGE   = 2;   // Ventilateurs de purge
const uint8_t PIN_PWM_DISTRIB = 3;   // Ventilateurs de distribution
const uint8_t PIN_PWM_EXTRACT = 4;   // Ventilateurs d'extraction

/* --- Entrées analogiques --- */
const uint8_t PIN_MQ8_H2      = A0;  // Détecteur H2 (partie haute)
const uint8_t PIN_MQ6_BUT     = A1;  // Détecteur GPL (proximité du sol)
const uint8_t PIN_PRESS_H2    = A2;  // Transmetteur 4-20 mA sur shunt 250 Ohm

/* --- Indices des sondes sur le bus OneWire --- */
const uint8_t IDX_SONDE_T_SEC = 0;
const uint8_t IDX_SONDE_T_CAP = 1;

/* --- Adresse et format de l'afficheur --- */
const uint8_t LCD_ADRESSE     = 0x27;
const uint8_t LCD_COLONNES    = 16;
const uint8_t LCD_LIGNES      = 2;

/* --------------------------------------------------------------------------
 * 3. CONSTANTES DE RÉGLAGE (§9.6)
 *    TOUS les seuils et temporisations sont regroupés ici (§7.5).
 * ------------------------------------------------------------------------*/

/* --- Régulation à trois paliers (§4) --- */
const float    H_HYST              = 5.0f;        // Bande d'hystérésis totale (± 2,5 °C)
const float    DEMI_HYST           = H_HYST / 2.0f;
const float    ECART_MIN_SEUILS    = 3.0f;        // Écart T_cible - T_init minimal exploitable

/* --- Temporisations de combustion (§7.2) --- */
const uint32_t PURGE_DUREE         = 120000UL;    // 120 s de purge obligatoire
const uint32_t ALLUMAGE_TIMEOUT    = 4000UL;      // 4 s max de confirmation de flamme

/* --- Détection de fuite ATEX (§7.4) — À CALIBRER SUR LE PROTOTYPE ---
 *  Procédure : relever la valeur analogRead() au repos, air propre, après
 *  24 h de préchauffage des MQ, puis fixer le seuil à cette valeur + 150.  */
const int      SEUIL_MQ8           = 350;         // Fuite H2
const int      SEUIL_MQ6           = 350;         // Fuite GPL

/* --- Chaîne de mesure de pression H2 (transmetteur 4-20 mA / shunt 250 Ohm) --- */
const float    PRESS_H2_PLEINE_ECHELLE = 10.0f;   // bar à 20 mA
const int      ADC_PRESS_4MA       = 204;         // 1,00 V  -> 4 mA
const int      ADC_PRESS_20MA      = 1023;        // 5,00 V  -> 20 mA
const int      ADC_PRESS_DEFAUT    = 180;         // en dessous : boucle 4-20 mA coupée
const float    SEUIL_PRESS_H2_MIN  = 2.0f;        // bar : sous ce seuil -> bascule GPL

/* --- Disponibilité de la source solaire (§2, priorité 1) --- */
const float    SEUIL_T_CAP_SOLAIRE_ON  = 55.0f;   // Plaque capteur assez chaude -> solaire
const float    SEUIL_T_CAP_SOLAIRE_OFF = 45.0f;   // Plaque refroidie -> passage en combustion

/* --- Fin de cycle (§5) --- */
const float    H_FIN_MAX           = 95.0f;       // Garde-fou, voir INCOHÉRENCE n°2

/* --- Sécurité surchauffe — AJOUT signalé, voir INCOHÉRENCE n°3 --- */
const float    T_SEC_MAX_SECURITE  = 90.0f;       // °C dans la chambre : arrêt propre

/* --- Divers --- */
const uint32_t PERIODE_CAPTEURS    = 1000UL;      // DS18B20 + DHT22 : 1 lecture/s
const uint32_t PERIODE_LCD         = 400UL;       // Rafraîchissement de l'afficheur
const uint32_t ANTI_REBOND         = 30UL;        // Anti-rebond des boutons
const uint32_t DUREE_REFROIDISSEMENT = 300000UL;  // 5 min de ventilation après séchage
const uint32_t DUREE_MAX_CYCLE_PLAFOND = 1440UL;  // 24 h : borne du menu (anti-débordement)

/* --- Câblage logique --- */
const bool     RELAIS_ACTIF_BAS    = false;       // true si modules relais actifs à l'état bas
const int      NIVEAU_FLAMME_PRESENTE = HIGH;     // Niveau du contrôleur de flamme si flamme

/* --- Consignes de ventilation PWM par état (§8) — ajustables ici uniquement --- */
const uint8_t PWM_ARRET             = 0;
const uint8_t PWM_PURGE_BALAYAGE    = 255;  // PURGE : maximal
const uint8_t PWM_PURGE_COMBUSTION  = 60;   // Paliers : faible
const uint8_t PWM_PURGE_URGENCE     = 255;  // URGENCE_ATEX : 255
const uint8_t PWM_DISTRIB_SOLAIRE   = 220;  // MODE_SOLAIRE : élevé
const uint8_t PWM_EXTRACT_SOLAIRE   = 150;  // MODE_SOLAIRE : moyen
const uint8_t PWM_DISTRIB_PURGE     = 60;   // PURGE : faible
const uint8_t PWM_EXTRACT_PURGE     = 200;  // PURGE : élevé
const uint8_t PWM_EXTRACT_REFROID   = 90;   // SECHAGE_TERMINE : faible
/* Distribution / extraction « selon palier » : indice 0 = 100 %, 1 = 67 %, 2 = 33 % */
const uint8_t PWM_DISTRIB_PAR_PALIER[3] = {255, 210, 165};
const uint8_t PWM_EXTRACT_PAR_PALIER[3] = {200, 165, 130};

/* --- Pas de réglage du menu (§3.3) --- */
const float    PAS_TEMPERATURE      = 1.0f;
const float    PAS_HUMIDITE         = 1.0f;
const uint32_t PAS_DUREE            = 15UL;   // minutes

/* --------------------------------------------------------------------------
 * 4. enum EtatFSM — ÉTATS PRINCIPAUX (§7.1)
 *
 *    Pour AJOUTER un état : ajouter une valeur ici, un `case` dans pasFSM()
 *    et un libellé dans nomEtat(). Rien d'autre à modifier.
 * ------------------------------------------------------------------------*/
enum EtatFSM : uint8_t {
  ETAT_ATTENTE_DEMARRAGE = 0, // État initial : tout fermé, ventilateurs à l'arrêt
  ETAT_CONFIG_MENU,           // Saisie des paramètres (LCD + 4 boutons)
  ETAT_MODE_SOLAIRE,          // Passif : AUCUNE électrovanne, ventilateurs seuls (§12)
  ETAT_MODE_H2,               // Combustion hydrogène (priorité 2)
  ETAT_MODE_GPL,              // Combustion GPL, secours (priorité 3)
  ETAT_PROLONGATION,          // Duree_Max_Cycle dépassée sans atteindre H_fin (§6)
  ETAT_SECHAGE_TERMINE,       // H_extr <= H_fin atteint
  ETAT_URGENCE_ATEX,          // État PARALLÈLE et PRIORITAIRE (§7.4)
  NB_ETATS
};

/* --------------------------------------------------------------------------
 * 5. enum PhaseCombustion — SOUS-PHASES DE MODE_H2 / MODE_GPL (§7.2)
 *    Ces sous-phases sont STRICTEMENT IDENTIQUES pour H2 et GPL : elles sont
 *    traitées par la fonction unique gererCombustion() (§2, §12).
 * ------------------------------------------------------------------------*/
enum PhaseCombustion : uint8_t {
  PH_PURGE = 0,     // 120 s, gaz fermé, Spark = 0
  PH_ALLUMAGE,      // <= 4 s : ouverture au palier courant + Spark
  PH_PALIER_100,    // EV1 + EV2 + EV3
  PH_PALIER_67,     // EV2 + EV3
  PH_PALIER_33      // EV3 seule — PLANCHER, il n'existe pas de palier 0 % (§4.1)
};

/* Indices de palier (§9.5) — jamais de valeur « arrêt » */
const uint8_t PALIER_100 = 0;
const uint8_t PALIER_67  = 1;
const uint8_t PALIER_33  = 2;

/* Sources d'énergie (§9.5, Source_Active) */
const uint8_t SRC_SOLAIRE = 0;
const uint8_t SRC_H2      = 1;
const uint8_t SRC_GPL     = 2;

/* Champs du menu LCD (§3.3, §7.4) */
enum ChampMenu : uint8_t {
  MENU_MODE_AUTO = 0,
  MENU_CHOIX_MODE,
  MENU_T_CIBLE,
  MENU_T_INIT,
  MENU_H_PRODUIT,
  MENU_DUREE_MAX,
  MENU_REARMEMENT,   // §7.4 chemin 2 : réarmement par le menu
  NB_CHAMPS_MENU
};

/* Identifiants internes des boutons (anti-rebond + détection de front) */
enum IdxBouton : uint8_t {
  B_MENU = 0, B_UP, B_DOWN, B_OK, B_START, B_STOP, B_REARM, NB_BOUTONS
};

/* --------------------------------------------------------------------------
 * 6. VARIABLES (§9)
 * ------------------------------------------------------------------------*/

/* --- 9.1 Entrées capteurs --- */
float    T_sec    = 20.0f;   // °C — chambre de séchage : GRANDEUR RÉGULÉE
float    H_extr   = 90.0f;   // %  — air extrait : critère de fin de cycle
float    T_amb    = 20.0f;   // °C — air extérieur
float    H_amb    = 50.0f;   // %  — air extérieur : entre dans H_fin
float    T_cap    = 20.0f;   // °C — plaque capteur solaire / brûleur
bool     Flame    = false;   // Confirmation de flamme
int      MQ8_H2   = 0;       // Niveau de détection H2
int      MQ6_But  = 0;       // Niveau de détection GPL
float    Press_H2 = 0.0f;    // bar — pression du réservoir H2

/* --- 9.2 Entrées opérateur (niveaux logiques « appuyé = true ») --- */
bool Btn_Menu = false, Btn_Up = false, Btn_Down = false, Btn_OK = false;
bool Btn_Start = false, Btn_Stop = false, Btn_Rearm = false;
bool AU_Urgence = false;

/* --- 9.3 Consignes saisies au menu --- */
float    T_cible          = 55.0f;
float    T_init           = 25.0f;   // Mode manuel uniquement ; en auto, lue au capteur
float    H_produit_cible  = 10.0f;
uint32_t Duree_Max_Cycle  = 600UL;   // minutes
bool     Mode_Auto        = true;
uint8_t  Choix_Mode       = 2;       // 1 = Solaire, 2 = H2, 3 = GPL (mode manuel)

/* --- 9.4 Sorties actionneurs --- */
bool    V_H2 = false, V_But = false;
bool    EV1 = false, EV2 = false, EV3 = false;
bool    Spark = false, Buzzer = false;
uint8_t PWM_Purge = 0, PWM_Distrib = 0, PWM_Extract = 0;
int     Etat_LCD = ETAT_ATTENTE_DEMARRAGE;

/* --- 9.5 Variables internes de la FSM --- */
EtatFSM         etat_courant     = ETAT_ATTENTE_DEMARRAGE;
PhaseCombustion phase_combustion = PH_PURGE;
uint8_t         palier           = PALIER_100;   // 0 = 100 %, 1 = 67 %, 2 = 33 %
float           T1 = 35.0f, T2 = 45.0f;
float           H_fin = 60.0f;                   // Recalculé à chaque itération (§5)
uint32_t        chrono_purge = 0;                // Horodatage de début de purge
uint32_t        chrono_allumage = 0;             // Horodatage de début d'allumage
uint32_t        chrono_cycle = 0;                // Horodatage de début de cycle
uint8_t         Source_Active = SRC_SOLAIRE;
uint8_t         Menu_Index = MENU_MODE_AUTO;

/* --- Variables de service --- */
static uint32_t t_boucle = 0;                    // millis() figé pour toute l'itération
static uint32_t t_dernier_capteur = 0;
static uint32_t t_dernier_lcd = 0;
static uint32_t chrono_refroidissement = 0;
static uint16_t nb_echecs_allumage = 0;
static bool     demande_rearm_ihm = false;       // §7.4 chemin 2
static bool     quitter_menu = false;
static bool     niveau_prec[NB_BOUTONS];
static uint32_t t_front[NB_BOUTONS];
static bool     front[NB_BOUTONS];

/* --- Objets matériels --- */
OneWire            busOneWire(PIN_ONEWIRE);
DallasTemperature  sondes(&busOneWire);
DHT                dhtAmb(PIN_DHT_AMB, DHT22);
DHT                dhtExtr(PIN_DHT_EXTR, DHT22);
LiquidCrystal_I2C  lcd(LCD_ADRESSE, LCD_COLONNES, LCD_LIGNES);

/* --- Prototypes (le fichier doit rester compilable hors IDE Arduino) --- */
void lireEntrees();
void appliquerPalier();
void gererCombustion(bool utiliseH2);
void pasFSM();
void ecrireSorties();
void gererIHM();

/* --------------------------------------------------------------------------
 * OUTILS
 * ------------------------------------------------------------------------*/

static inline bool estNaN(float v) { return v != v; }

/* Consomme un front de bouton : un appui ne peut déclencher qu'une seule
 * action, même si pasFSM() et gererIHM() s'intéressent au même bouton. */
static bool frontPris(uint8_t idx) {
  if (!front[idx]) return false;
  front[idx] = false;
  return true;
}

static const char* nomEtat(EtatFSM e) {
  switch (e) {
    case ETAT_ATTENTE_DEMARRAGE: return "ATTENTE";
    case ETAT_CONFIG_MENU:       return "MENU";
    case ETAT_MODE_SOLAIRE:      return "SOLAIRE";
    case ETAT_MODE_H2:           return "MODE H2";
    case ETAT_MODE_GPL:          return "MODE GPL";
    case ETAT_PROLONGATION:      return "PROLONG.";
    case ETAT_SECHAGE_TERMINE:   return "TERMINE";
    case ETAT_URGENCE_ATEX:      return "URGENCE";
    default:                     return "?";
  }
}

static const char* nomPhase(PhaseCombustion p) {
  switch (p) {
    case PH_PURGE:      return "PURGE";
    case PH_ALLUMAGE:   return "ALLUM";
    case PH_PALIER_100: return "100%";
    case PH_PALIER_67:  return "67%";
    case PH_PALIER_33:  return "33%";
    default:            return "?";
  }
}

/* Écriture TOR tenant compte de la polarité des modules relais. */
static inline void ecrireTOR(uint8_t pin, bool actif) {
  digitalWrite(pin, (actif != RELAIS_ACTIF_BAS) ? HIGH : LOW);
}

/* Formatage d'un flottant au dixième, sans %f (indisponible sur AVR). */
static void fmt1(char* buf, uint8_t taille, float v) {
  long d = (long)(v * 10.0f + (v >= 0.0f ? 0.5f : -0.5f));
  snprintf(buf, taille, "%ld.%ld", d / 10, labs(d % 10));
}

/* Transition d'état centralisée : un seul point de journalisation. */
static void changerEtat(EtatFSM nouvel_etat) {
  if (nouvel_etat == etat_courant) return;
  etat_courant = nouvel_etat;
  if (nouvel_etat == ETAT_URGENCE_ATEX) {
    Menu_Index = MENU_REARMEMENT;   // §7.4 : l'option de réarmement est présentée d'office
  }
  if (nouvel_etat == ETAT_SECHAGE_TERMINE) {
    chrono_refroidissement = t_boucle;
  }
  Serial.print("FSM -> ");
  Serial.println(nomEtat(nouvel_etat));
}

/* §7.3 règle 1 : fermeture complète du circuit gaz. */
static void fermerGaz() {
  V_H2 = false;
  V_But = false;
  EV1 = false;
  EV2 = false;
  EV3 = false;
  Spark = false;
}

/* §7.3 règle 1 : toute (re)mise en gaz est précédée de 120 s de purge. */
static void armerPurge() {
  fermerGaz();
  phase_combustion = PH_PURGE;
  chrono_purge = t_boucle;
  PWM_Purge   = PWM_PURGE_BALAYAGE;
  PWM_Distrib = PWM_DISTRIB_PURGE;
  PWM_Extract = PWM_EXTRACT_PURGE;
}

/* phase_combustion est DÉRIVÉE de `palier` dans les états de régulation :
 * une seule source de vérité (voir INCOHÉRENCE n°1). */
static PhaseCombustion phaseDuPalier(uint8_t p) {
  if (p == PALIER_100) return PH_PALIER_100;
  if (p == PALIER_67)  return PH_PALIER_67;
  return PH_PALIER_33;
}

/* §4.3 — Découpage en trois de l'intervalle [T_init ; T_cible]. */
static void calculerSeuils() {
  if (T_cible <= T_init + ECART_MIN_SEUILS) {
    /* INCOHÉRENCE n°4 : consigne trop proche (ou en dessous) de la température
     * initiale -> les trois paliers se confondent. On rabat T1 = T2 = T_cible :
     * le brûleur démarre à 100 % puis tombe au plancher 33 % une fois la
     * consigne franchie, au lieu de produire des seuils inversés. */
    T1 = T_cible;
    T2 = T_cible;
  } else {
    T1 = T_init + (T_cible - T_init) / 3.0f;
    T2 = T_init + 2.0f * (T_cible - T_init) / 3.0f;
  }
}

/* §4.2 — Hystérésis de 5 °C MÉMORISÉE PAR LE PALIER COURANT.
 * Un seul franchissement de seuil par itération : c'est ce qui fait de la
 * modulation une véritable machine à états et non trois tests indépendants. */
static void majPalier() {
  switch (palier) {
    case PALIER_100:
      if (T_sec >= T1 + DEMI_HYST) palier = PALIER_67;     // fermeture EV1
      break;
    case PALIER_67:
      if (T_sec >= T2 + DEMI_HYST)      palier = PALIER_33; // fermeture EV2
      else if (T_sec < T1 - DEMI_HYST)  palier = PALIER_100; // réouverture EV1
      break;
    case PALIER_33:
      if (T_sec < T2 - DEMI_HYST) palier = PALIER_67;       // réouverture EV2
      /* Pas d'échappatoire vers le haut : 33 % est le PLANCHER (§4.1, §12). */
      break;
    default:
      palier = PALIER_100;
      break;
  }
}

/* §7.4 — Cause d'urgence encore présente ? */
static bool causeUrgencePresente() {
  return (MQ8_H2 > SEUIL_MQ8) || (MQ6_But > SEUIL_MQ6) || AU_Urgence;
}

/* Les états dans lesquels un cycle de séchage est effectivement en cours. */
static bool cycleEnCours() {
  return etat_courant == ETAT_MODE_SOLAIRE || etat_courant == ETAT_MODE_H2 ||
         etat_courant == ETAT_MODE_GPL     || etat_courant == ETAT_PROLONGATION;
}

/* --------------------------------------------------------------------------
 * 7. setup()
 * ------------------------------------------------------------------------*/
static void lireCapteursLents();

void setup() {
  Serial.begin(115200);

  /* Sorties TOR : toutes inactives AVANT tout autre traitement. */
  const uint8_t sorties_tor[] = {PIN_V_H2, PIN_V_BUT, PIN_EV1, PIN_EV2,
                                 PIN_EV3, PIN_SPARK, PIN_BUZZER};
  for (uint8_t i = 0; i < sizeof(sorties_tor); i++) {
    pinMode(sorties_tor[i], OUTPUT);
    ecrireTOR(sorties_tor[i], false);
  }
  pinMode(PIN_PWM_PURGE, OUTPUT);
  pinMode(PIN_PWM_DISTRIB, OUTPUT);
  pinMode(PIN_PWM_EXTRACT, OUTPUT);
  analogWrite(PIN_PWM_PURGE, 0);
  analogWrite(PIN_PWM_DISTRIB, 0);
  analogWrite(PIN_PWM_EXTRACT, 0);

  /* Entrées opérateur : contacts vers 0 V. */
  pinMode(PIN_BTN_MENU,  INPUT_PULLUP);
  pinMode(PIN_BTN_UP,    INPUT_PULLUP);
  pinMode(PIN_BTN_DOWN,  INPUT_PULLUP);
  pinMode(PIN_BTN_OK,    INPUT_PULLUP);
  pinMode(PIN_BTN_START, INPUT_PULLUP);
  pinMode(PIN_BTN_STOP,  INPUT_PULLUP);
  pinMode(PIN_BTN_REARM, INPUT_PULLUP);
  /* Arrêt d'urgence : contact NC câblé vers 0 V. Repos = LOW, appui OU rupture
   * de fil = HIGH -> l'alarme se déclenche aussi sur défaut de câblage. */
  pinMode(PIN_AU_URGENCE, INPUT_PULLUP);
  pinMode(PIN_FLAMME, INPUT);

  for (uint8_t i = 0; i < NB_BOUTONS; i++) {
    niveau_prec[i] = false;
    t_front[i] = 0;
    front[i] = false;
  }

  /* Capteurs. */
  sondes.begin();
  sondes.setResolution(12);
  sondes.setWaitForConversion(false);   // conversion asynchrone : loop() non bloquée
  sondes.requestTemperatures();
  dhtAmb.begin();
  dhtExtr.begin();

  /* Afficheur. */
  Wire.begin();
  lcd.init();
  lcd.backlight();
  lcd.clear();

  delay(800);                 // 1re conversion DS18B20 (12 bits : 750 ms)
  t_boucle = millis();
  lireCapteursLents();
  t_dernier_capteur = t_boucle;
  H_fin = H_produit_cible + H_amb;

  etat_courant = ETAT_ATTENTE_DEMARRAGE;
  phase_combustion = PH_PURGE;
  palier = PALIER_100;
  Serial.println("Sechoir hybride : initialisation terminee");
}

/* --------------------------------------------------------------------------
 * 8. loop() — séquence imposée par §10
 * ------------------------------------------------------------------------*/
void loop() {
  lireEntrees();     // acquisition + recalcul de H_fin (§5)
  pasFSM();          // machine à états (§7)
  ecrireSorties();   // application physique des sorties
  gererIHM();        // menu LCD + 4 boutons + réarmement (§3.3, §7.4)
}

/* --------------------------------------------------------------------------
 * 9. lireEntrees()
 * ------------------------------------------------------------------------*/

/* Conversion du transmetteur 4-20 mA en bar.
 * Un courant inférieur à 4 mA signale une boucle coupée : on renvoie 0 bar,
 * ce qui provoque la bascule vers le GPL (comportement fail-safe). */
static float convertirPression(int brut) {
  if (brut < ADC_PRESS_DEFAUT) return 0.0f;
  if (brut < ADC_PRESS_4MA) brut = ADC_PRESS_4MA;
  float p = (float)(brut - ADC_PRESS_4MA) * PRESS_H2_PLEINE_ECHELLE /
            (float)(ADC_PRESS_20MA - ADC_PRESS_4MA);
  return p;
}

/* Lecture d'un bouton à contact vers 0 V : appuyé = niveau bas. */
static bool lireBouton(uint8_t pin) {
  return digitalRead(pin) == LOW;
}

/* Détection de front montant avec temporisation anti-rebond. */
static bool detecterFront(uint8_t idx, bool niveau) {
  if (niveau == niveau_prec[idx]) return false;
  if ((t_boucle - t_front[idx]) < ANTI_REBOND) return false;  // rebond ignoré
  t_front[idx] = t_boucle;
  niveau_prec[idx] = niveau;
  return niveau;                                              // front montant seul
}

/* Capteurs lents : DS18B20 (750 ms de conversion) et DHT22 (2 s mini). */
static void lireCapteursLents() {
  float v;
  v = sondes.getTempCByIndex(IDX_SONDE_T_SEC);
  if (v > -100.0f) T_sec = v;                 // -127 = sonde déconnectée
  v = sondes.getTempCByIndex(IDX_SONDE_T_CAP);
  if (v > -100.0f) T_cap = v;
  sondes.requestTemperatures();               // conversion suivante

  v = dhtAmb.readTemperature(); if (!estNaN(v)) T_amb  = v;
  v = dhtAmb.readHumidity();    if (!estNaN(v)) H_amb  = v;
  v = dhtExtr.readHumidity();   if (!estNaN(v)) H_extr = v;
}

void lireEntrees() {
  t_boucle = millis();

  /* --- Entrées de sécurité : lues à CHAQUE itération, sans filtrage --- */
  Flame      = (digitalRead(PIN_FLAMME) == NIVEAU_FLAMME_PRESENTE);
  AU_Urgence = (digitalRead(PIN_AU_URGENCE) == HIGH);   // contact NC, voir setup()
  MQ8_H2     = analogRead(PIN_MQ8_H2);
  MQ6_But    = analogRead(PIN_MQ6_BUT);
  Press_H2   = convertirPression(analogRead(PIN_PRESS_H2));

  /* --- Boutons + fronts --- */
  Btn_Menu  = lireBouton(PIN_BTN_MENU);
  Btn_Up    = lireBouton(PIN_BTN_UP);
  Btn_Down  = lireBouton(PIN_BTN_DOWN);
  Btn_OK    = lireBouton(PIN_BTN_OK);
  Btn_Start = lireBouton(PIN_BTN_START);
  Btn_Stop  = lireBouton(PIN_BTN_STOP);
  Btn_Rearm = lireBouton(PIN_BTN_REARM);
  front[B_MENU]  = detecterFront(B_MENU,  Btn_Menu);
  front[B_UP]    = detecterFront(B_UP,    Btn_Up);
  front[B_DOWN]  = detecterFront(B_DOWN,  Btn_Down);
  front[B_OK]    = detecterFront(B_OK,    Btn_OK);
  front[B_START] = detecterFront(B_START, Btn_Start);
  front[B_STOP]  = detecterFront(B_STOP,  Btn_Stop);
  front[B_REARM] = detecterFront(B_REARM, Btn_Rearm);

  /* --- Capteurs lents --- */
  if ((t_boucle - t_dernier_capteur) >= PERIODE_CAPTEURS) {
    t_dernier_capteur = t_boucle;
    lireCapteursLents();
  }

  /* --- §5 : H_fin est RECALCULÉ À CHAQUE ITÉRATION, jamais figé --- */
  H_fin = H_produit_cible + H_amb;
  if (H_fin > H_FIN_MAX) H_fin = H_FIN_MAX;   // garde-fou, INCOHÉRENCE n°2
}

/* --------------------------------------------------------------------------
 * 10. appliquerPalier() — palier -> état des trois électrovannes combustible
 *     §4.1 : 100 % = EV1+EV2+EV3 | 67 % = EV2+EV3 | 33 % = EV3 seule.
 *     EV3 reste ouverte dans tous les cas : il n'existe pas de palier 0 %.
 * ------------------------------------------------------------------------*/
void appliquerPalier() {
  EV1 = (palier == PALIER_100);
  EV2 = (palier == PALIER_100 || palier == PALIER_67);
  EV3 = true;
}

/* --------------------------------------------------------------------------
 * 11. gererCombustion(bool utiliseH2)
 *     FONCTION UNIQUE PARTAGÉE par MODE_H2 et MODE_GPL (§2, §12).
 *     Seule la vanne source diffère : V_H2 <-> V_But.
 * ------------------------------------------------------------------------*/
void gererCombustion(bool utiliseH2) {
  switch (phase_combustion) {

    /* ---- PURGE (120 s) : gaz fermé, Spark = 0, balayage maximal (§7.2) ---- */
    case PH_PURGE:
      fermerGaz();
      PWM_Purge   = PWM_PURGE_BALAYAGE;
      PWM_Distrib = PWM_DISTRIB_PURGE;
      PWM_Extract = PWM_EXTRACT_PURGE;
      if ((t_boucle - chrono_purge) >= PURGE_DUREE) {
        phase_combustion = PH_ALLUMAGE;
        chrono_allumage = t_boucle;
      }
      break;

    /* ---- ALLUMAGE (<= 4 s) : ouverture AU PALIER COURANT + Spark (§7.2) ----
     * Le palier courant est conservé : sur une bascule H2 -> GPL à 67 %, le
     * brûleur se rallume à 67 % et non à 100 % (§7.3 règle 3). */
    case PH_ALLUMAGE:
      V_H2  = utiliseH2;
      V_But = !utiliseH2;
      appliquerPalier();
      Spark = true;
      PWM_Purge   = PWM_PURGE_COMBUSTION;
      PWM_Distrib = PWM_DISTRIB_PAR_PALIER[palier];
      PWM_Extract = PWM_EXTRACT_PAR_PALIER[palier];
      if (Flame) {
        Spark = false;
        phase_combustion = phaseDuPalier(palier);
      } else if ((t_boucle - chrono_allumage) >= ALLUMAGE_TIMEOUT) {
        nb_echecs_allumage++;
        Serial.println("Echec d'allumage : retour PURGE");
        armerPurge();                     // fermeture totale puis nouvelle purge
      }
      break;

    /* ---- PALIERS : régulation par hystérésis (§4) ---- */
    case PH_PALIER_100:
    case PH_PALIER_67:
    case PH_PALIER_33:
      /* §7.3 règle 2 : la flamme est VÉRIFIÉE D'ABORD. L'état des
       * électrovannes n'est appliqué qu'ensuite — jamais avant. Une perte de
       * flamme referme donc le gaz dans l'itération même où elle est vue. */
      if (!Flame) {
        Serial.println("Perte de flamme : fermeture immediate");
        armerPurge();
        break;
      }
      majPalier();                        // hystérésis 5 °C mémorisée (§4.2)
      V_H2  = utiliseH2;
      V_But = !utiliseH2;
      appliquerPalier();
      Spark = false;
      PWM_Purge   = PWM_PURGE_COMBUSTION;
      PWM_Distrib = PWM_DISTRIB_PAR_PALIER[palier];
      PWM_Extract = PWM_EXTRACT_PAR_PALIER[palier];
      phase_combustion = phaseDuPalier(palier);
      break;
  }
}

/* --------------------------------------------------------------------------
 * 12. pasFSM() — MACHINE À ÉTATS PRINCIPALE (§7)
 * ------------------------------------------------------------------------*/

/* MODE_SOLAIRE : purement passif, aucune électrovanne (§2, §12). */
static void etapeSolaire() {
  fermerGaz();
  PWM_Purge   = PWM_ARRET;
  PWM_Distrib = PWM_DISTRIB_SOLAIRE;
  PWM_Extract = PWM_EXTRACT_SOLAIRE;
  /* Brûleur à l'arrêt : la purge reste armée, de sorte qu'un futur passage en
   * combustion disposera bien de ses 120 s complètes (§7.3 règle 1). */
  phase_combustion = PH_PURGE;
  chrono_purge = t_boucle;
}

/* Arbitrage automatique des trois sources (§2). Renvoie true si Source_Active
 * a changé. En mode manuel, l'opérateur impose la source : aucun arbitrage. */
static bool arbitrageSource() {
  if (!Mode_Auto) return false;

  /* Priorité 1 : retour au solaire dès qu'il redevient disponible. */
  if (Source_Active != SRC_SOLAIRE && T_cap >= SEUIL_T_CAP_SOLAIRE_ON) {
    fermerGaz();
    Source_Active = SRC_SOLAIRE;
    return true;
  }

  /* Solaire insuffisant -> combustion. DÉMARRAGE À FROID : palier 100 % (§7.3 règle 3). */
  if (Source_Active == SRC_SOLAIRE && T_cap < SEUIL_T_CAP_SOLAIRE_OFF) {
    Source_Active = (Press_H2 >= SEUIL_PRESS_H2_MIN) ? SRC_H2 : SRC_GPL;
    palier = PALIER_100;
    armerPurge();
    return true;
  }

  /* Réservoir H2 épuisé -> GPL de secours.
   * Le PALIER COURANT EST CONSERVÉ (§7.3 règle 3) mais la purge de 120 s
   * reste obligatoire avant la remise en gaz (§7.3 règle 1). */
  if (Source_Active == SRC_H2 && Press_H2 < SEUIL_PRESS_H2_MIN) {
    Serial.println("Pression H2 basse : bascule GPL, palier conserve");
    armerPurge();
    Source_Active = SRC_GPL;
    return true;
  }

  return false;
}

static void synchroniserEtatSurSource() {
  if (Source_Active == SRC_SOLAIRE)  changerEtat(ETAT_MODE_SOLAIRE);
  else if (Source_Active == SRC_H2)  changerEtat(ETAT_MODE_H2);
  else                               changerEtat(ETAT_MODE_GPL);
}

/* Cœur de régulation commun à MODE_SOLAIRE / MODE_H2 / MODE_GPL / PROLONGATION.
 * `synchroniser` est faux en PROLONGATION : la source peut y changer sans que
 * l'état principal quitte PROLONGATION (§6). */
static void etapeRegulation(bool synchroniser) {
  arbitrageSource();
  if (synchroniser) synchroniserEtatSurSource();
  if (Source_Active == SRC_SOLAIRE) etapeSolaire();
  else                              gererCombustion(Source_Active == SRC_H2);
}

/* Démarrage d'un cycle depuis ATTENTE_DEMARRAGE, CONFIG_MENU ou SECHAGE_TERMINE. */
static void demarrerCycle() {
  if (Mode_Auto) T_init = T_sec;      // §4.3 : en auto, T_init est lue au capteur
  calculerSeuils();

  palier = PALIER_100;                // §4.1 : le démarrage est TOUJOURS à 100 %
  chrono_cycle = t_boucle;
  nb_echecs_allumage = 0;

  if (Mode_Auto) {
    if (T_cap >= SEUIL_T_CAP_SOLAIRE_ON)        Source_Active = SRC_SOLAIRE;
    else if (Press_H2 >= SEUIL_PRESS_H2_MIN)    Source_Active = SRC_H2;
    else                                        Source_Active = SRC_GPL;
  } else {
    if (Choix_Mode == 1)      Source_Active = SRC_SOLAIRE;
    else if (Choix_Mode == 2) Source_Active = SRC_H2;
    else                      Source_Active = SRC_GPL;
  }

  armerPurge();                       // §7.3 règle 1 : purge avant toute mise en gaz
  synchroniserEtatSurSource();
}

/* §7.4 — Toutes les sorties forcées en position de repli. */
static void appliquerUrgence() {
  fermerGaz();
  PWM_Purge   = PWM_PURGE_URGENCE;    // 255
  PWM_Distrib = PWM_ARRET;            // 0
  PWM_Extract = PWM_PURGE_URGENCE;    // 255
  Buzzer = true;
}

void pasFSM() {

  /* ===== switch principal : UN CASE PAR ÉTAT (§7.5) ===== */
  switch (etat_courant) {

    /* --- État initial : tout fermé, ventilateurs à l'arrêt (§8) --- */
    case ETAT_ATTENTE_DEMARRAGE:
      fermerGaz();
      PWM_Purge = PWM_ARRET;
      PWM_Distrib = PWM_ARRET;
      PWM_Extract = PWM_ARRET;
      Buzzer = false;
      if (frontPris(B_MENU)) { Menu_Index = MENU_MODE_AUTO; changerEtat(ETAT_CONFIG_MENU); }
      else if (frontPris(B_START)) { demarrerCycle(); }
      break;

    /* --- Saisie des paramètres : le menu lui-même est traité par gererIHM() --- */
    case ETAT_CONFIG_MENU:
      fermerGaz();
      PWM_Purge = PWM_ARRET;
      PWM_Distrib = PWM_ARRET;
      PWM_Extract = PWM_ARRET;
      if (quitter_menu) { quitter_menu = false; changerEtat(ETAT_ATTENTE_DEMARRAGE); }
      else if (frontPris(B_START)) { demarrerCycle(); }
      break;

    /* --- Solaire : passif, aucune électrovanne (§2, §12) --- */
    case ETAT_MODE_SOLAIRE:
      Source_Active = SRC_SOLAIRE;
      etapeRegulation(true);
      break;

    /* --- Combustion hydrogène : PURGE -> ALLUMAGE -> PALIERS --- */
    case ETAT_MODE_H2:
      Source_Active = SRC_H2;
      etapeRegulation(true);
      break;

    /* --- Combustion GPL (secours) : logique STRICTEMENT identique à H2 --- */
    case ETAT_MODE_GPL:
      Source_Active = SRC_GPL;
      etapeRegulation(true);
      break;

    /* --- Prolongation : régulation normale, sans nouvelle limite de temps (§6) ---
     * La source mémorisée dans Source_Active est reprise telle quelle : ni
     * nouvelle purge, ni retour à 100 %, le cycle se poursuit sans rupture. */
    case ETAT_PROLONGATION:
      etapeRegulation(false);
      break;

    /* --- Séchage terminé : gaz fermé, extraction faible pour refroidir (§8) --- */
    case ETAT_SECHAGE_TERMINE:
      fermerGaz();
      PWM_Purge = PWM_ARRET;
      PWM_Distrib = PWM_ARRET;
      PWM_Extract = PWM_EXTRACT_REFROID;
      if (frontPris(B_START)) demarrerCycle();
      else if (frontPris(B_MENU)) { Menu_Index = MENU_MODE_AUTO; changerEtat(ETAT_CONFIG_MENU); }
      else if ((t_boucle - chrono_refroidissement) >= DUREE_REFROIDISSEMENT) {
        changerEtat(ETAT_ATTENTE_DEMARRAGE);
      }
      break;

    /* --- Urgence ATEX : repli permanent + double chemin de réarmement (§7.4) --- */
    case ETAT_URGENCE_ATEX:
      appliquerUrgence();
      if (frontPris(B_REARM) || demande_rearm_ihm) {
        demande_rearm_ihm = false;
        if (causeUrgencePresente()) {
          Serial.println("Rearmement refuse : cause toujours presente");
        } else {
          Buzzer = false;
          palier = PALIER_100;
          phase_combustion = PH_PURGE;
          Menu_Index = MENU_MODE_AUTO;
          changerEtat(ETAT_ATTENTE_DEMARRAGE);
        }
      }
      break;

    default:
      changerEtat(ETAT_ATTENTE_DEMARRAGE);
      break;
  }

  /* ===== TRANSITIONS PRIORITAIRES — hors du switch, APRÈS lui (§7.5) =====
   * Elles s'appliquent depuis n'importe quel état et écrasent, si besoin, les
   * sorties calculées ci-dessus. ecrireSorties() n'étant appelée qu'ensuite,
   * rien d'incohérent n'atteint jamais les actionneurs. */

  /* 1. URGENCE_ATEX — priorité absolue, évaluée à chaque itération (§7.4). */
  if (causeUrgencePresente()) {
    if (etat_courant != ETAT_URGENCE_ATEX) {
      Serial.println("URGENCE ATEX : fuite gaz ou arret d'urgence");
      changerEtat(ETAT_URGENCE_ATEX);
    }
    appliquerUrgence();
    return;                       // aucune autre transition ne peut s'appliquer
  }

  /* 2. Arrêt propre demandé par l'opérateur (Btn_Stop). */
  if (cycleEnCours() && frontPris(B_STOP)) {
    fermerGaz();
    changerEtat(ETAT_SECHAGE_TERMINE);
    return;
  }

  /* 3. AJOUT SÉCURITÉ — surchauffe de la chambre (INCOHÉRENCE n°3).
   * Le plancher 33 % interdit toute coupure par la régulation : sans ce
   * garde-fou, une sonde en défaut laisserait le brûleur chauffer sans limite. */
  if (cycleEnCours() && T_sec >= T_SEC_MAX_SECURITE) {
    Serial.println("Surchauffe chambre : arret du brûleur");
    fermerGaz();
    changerEtat(ETAT_SECHAGE_TERMINE);
    return;
  }

  /* 4. Fin de cycle hygrométrique (§5) : H_extr <= H_fin. */
  if (cycleEnCours() && H_extr <= H_fin) {
    fermerGaz();
    changerEtat(ETAT_SECHAGE_TERMINE);
    return;
  }

  /* 5. Durée maximale dépassée sans avoir atteint H_fin -> PROLONGATION (§6).
   * La prolongation n'a elle-même AUCUNE limite de temps : elle dure jusqu'à
   * ce que la condition hygrométrique ci-dessus soit vérifiée. */
  if (cycleEnCours() && etat_courant != ETAT_PROLONGATION &&
      (t_boucle - chrono_cycle) >= (Duree_Max_Cycle * 60000UL)) {
    changerEtat(ETAT_PROLONGATION);
  }
}

/* --------------------------------------------------------------------------
 * 13. ecrireSorties() — seul point d'accès aux actionneurs
 * ------------------------------------------------------------------------*/
void ecrireSorties() {
  ecrireTOR(PIN_V_H2,   V_H2);
  ecrireTOR(PIN_V_BUT,  V_But);
  ecrireTOR(PIN_EV1,    EV1);
  ecrireTOR(PIN_EV2,    EV2);
  ecrireTOR(PIN_EV3,    EV3);
  ecrireTOR(PIN_SPARK,  Spark);
  ecrireTOR(PIN_BUZZER, Buzzer);

  analogWrite(PIN_PWM_PURGE,   PWM_Purge);
  analogWrite(PIN_PWM_DISTRIB, PWM_Distrib);
  analogWrite(PIN_PWM_EXTRACT, PWM_Extract);

  Etat_LCD = (int)etat_courant;
}

/* --------------------------------------------------------------------------
 * 14. gererIHM() — menu LCD + 4 boutons + réarmement (§3.3, §7.4)
 * ------------------------------------------------------------------------*/

static void modifierChampMenu(int sens) {
  switch (Menu_Index) {
    case MENU_MODE_AUTO:
      Mode_Auto = !Mode_Auto;
      break;
    case MENU_CHOIX_MODE:
      if (sens > 0) Choix_Mode = (Choix_Mode >= 3) ? 1 : (uint8_t)(Choix_Mode + 1);
      else          Choix_Mode = (Choix_Mode <= 1) ? 3 : (uint8_t)(Choix_Mode - 1);
      break;
    case MENU_T_CIBLE:
      T_cible += sens * PAS_TEMPERATURE;
      if (T_cible < 30.0f) T_cible = 30.0f;
      if (T_cible > 90.0f) T_cible = 90.0f;
      break;
    case MENU_T_INIT:
      T_init += sens * PAS_TEMPERATURE;
      if (T_init < 0.0f)  T_init = 0.0f;
      if (T_init > 60.0f) T_init = 60.0f;
      break;
    case MENU_H_PRODUIT:
      H_produit_cible += sens * PAS_HUMIDITE;
      if (H_produit_cible < 1.0f)  H_produit_cible = 1.0f;
      if (H_produit_cible > 50.0f) H_produit_cible = 50.0f;
      break;
    case MENU_DUREE_MAX:
      if (sens > 0) Duree_Max_Cycle += PAS_DUREE;
      else          Duree_Max_Cycle = (Duree_Max_Cycle > PAS_DUREE)
                                    ? Duree_Max_Cycle - PAS_DUREE : PAS_DUREE;
      if (Duree_Max_Cycle > DUREE_MAX_CYCLE_PLAFOND) Duree_Max_Cycle = DUREE_MAX_CYCLE_PLAFOND;
      break;
    default:
      break;                        // MENU_REARMEMENT : rien à incrémenter
  }
}

/* Construit les deux lignes de l'afficheur en fonction de l'état courant. */
static void composerLCD(char* l0, char* l1, uint8_t taille) {
  char a[10], b[10];

  if (etat_courant == ETAT_CONFIG_MENU || etat_courant == ETAT_URGENCE_ATEX) {
    if (etat_courant == ETAT_URGENCE_ATEX) snprintf(l0, taille, "!! URGENCE ATEX");
    else                                   snprintf(l0, taille, "Configuration");

    switch (Menu_Index) {
      case MENU_MODE_AUTO:
        snprintf(l1, taille, "Mode: %s", Mode_Auto ? "AUTO" : "MANUEL");
        break;
      case MENU_CHOIX_MODE:
        snprintf(l1, taille, "Source: %s",
                 Choix_Mode == 1 ? "SOL" : (Choix_Mode == 2 ? "H2" : "GPL"));
        break;
      case MENU_T_CIBLE:
        fmt1(a, sizeof(a), T_cible);
        snprintf(l1, taille, "T cible: %sC", a);
        break;
      case MENU_T_INIT:
        fmt1(a, sizeof(a), T_init);
        snprintf(l1, taille, "T init: %sC", a);
        break;
      case MENU_H_PRODUIT:
        fmt1(a, sizeof(a), H_produit_cible);
        snprintf(l1, taille, "H prod: %s%%", a);
        break;
      case MENU_DUREE_MAX:
        snprintf(l1, taille, "Duree: %lu min", (unsigned long)Duree_Max_Cycle);
        break;
      case MENU_REARMEMENT:
        snprintf(l1, taille, "REARMER? (OK)");
        break;
      default:
        l1[0] = '\0';
        break;
    }
    return;
  }

  switch (etat_courant) {
    case ETAT_MODE_H2:
    case ETAT_MODE_GPL:
    case ETAT_PROLONGATION:
      fmt1(a, sizeof(a), T_sec);
      fmt1(b, sizeof(b), T_cible);
      snprintf(l0, taille, "%s %s/%s", nomEtat(etat_courant), a, b);
      fmt1(a, sizeof(a), H_extr);
      fmt1(b, sizeof(b), H_fin);
      snprintf(l1, taille, "%s H%s/%s", nomPhase(phase_combustion), a, b);
      break;
    case ETAT_MODE_SOLAIRE:
      fmt1(a, sizeof(a), T_sec);
      fmt1(b, sizeof(b), T_cible);
      snprintf(l0, taille, "SOLAIRE %s/%s", a, b);
      fmt1(a, sizeof(a), H_extr);
      fmt1(b, sizeof(b), H_fin);
      snprintf(l1, taille, "H %s/%s", a, b);
      break;
    default:
      snprintf(l0, taille, "%s", nomEtat(etat_courant));
      fmt1(a, sizeof(a), T_sec);
      fmt1(b, sizeof(b), H_extr);
      snprintf(l1, taille, "T%sC H%s%%", a, b);
      break;
  }
}

void gererIHM() {
  /* --- Navigation et édition : actives en configuration et en urgence --- */
  if (etat_courant == ETAT_CONFIG_MENU || etat_courant == ETAT_URGENCE_ATEX) {
    if (frontPris(B_MENU)) Menu_Index = (uint8_t)((Menu_Index + 1) % NB_CHAMPS_MENU);
    if (frontPris(B_UP))   modifierChampMenu(+1);
    if (frontPris(B_DOWN)) modifierChampMenu(-1);
    if (frontPris(B_OK)) {
      if (Menu_Index == MENU_REARMEMENT) {
        /* §7.4 chemin 2 : la demande est latchée ici et arbitrée par pasFSM(),
         * qui seul décide si la cause a disparu. */
        demande_rearm_ihm = true;
      } else if (etat_courant == ETAT_CONFIG_MENU) {
        quitter_menu = true;
      }
    }
  }

  /* --- Afficheur --- */
  if ((t_boucle - t_dernier_lcd) >= PERIODE_LCD) {
    t_dernier_lcd = t_boucle;
    char l0[LCD_COLONNES + 1];
    char l1[LCD_COLONNES + 1];
    l0[0] = '\0';
    l1[0] = '\0';
    composerLCD(l0, l1, LCD_COLONNES + 1);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(l0);
    lcd.setCursor(0, 1);
    lcd.print(l1);
  }
}
