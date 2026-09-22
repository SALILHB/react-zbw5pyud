/* ============================================================================
 *  SÉCHOIR SOLAIRE HYBRIDE  —  Solaire / Hydrogène / GPL
 *  Commande de température par machine à états finis (FSM)
 *  Cible matérielle : Arduino Mega 2560
 *
 *  VERSION 2 — Révision du programme initial (voir git log) pour intégrer :
 *    - la modulation à QUATRE paliers 100/67/33/0 % (le 0 % coupe réellement
 *      le brûleur, ce n'est plus un plancher) ;
 *    - le calcul de T1/T2/T3 à partir de T_cible, saisi par l'opérateur ;
 *    - le critère de fin de cycle H_sec <= H_fin, verrouillé par une durée
 *      minimale Temps_Min_Fin, avec un état d'attente de confirmation
 *      FIN_TEMPORISATION avant l'arrêt automatique ;
 *    - une structure de configuration centralisée `Config`, sauvegardée en
 *      EEPROM ;
 *    - une séquence de basculement H2<->GPL avec état d'erreur dédié et
 *      menu de reprise (REESSAYER / MANUEL / AUTOMATIQUE) ;
 *    - un écran 20x4 I2C (remplace le 16x2 — même bus, même bibliothèque).
 *
 *  Ce fichier suit la structure imposée par le cahier des charges (§10 v1,
 *  §18 v2) :
 *    1. Includes                      7.  setup()
 *    2. Broches                       8.  loop()
 *    3. Constantes de réglage         9.  lireEntrees()
 *    4. enum EtatFSM                  10. appliquerPalier()
 *    5. enum PhaseCombustion          11. gererCombustion(bool utiliseH2)
 *    6. Variables + Config + EEPROM   12. pasFSM()
 *                                     13. ecrireSorties()
 *                                     14. gererIHM()
 *
 *  Les références «§x» renvoient au cahier des charges v2 (le second prompt).
 *  Les références «§x v1» renvoient au cahier des charges d'origine.
 *  Les points signalés «AVIS n°x» sont des remarques critiques / décisions
 *  d'interprétation prises faute de spécification univoque ; ils sont
 *  détaillés dans docs/FSM_SECHOIR.md.
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
#include <EEPROM.h>

/* --------------------------------------------------------------------------
 * 2. DÉFINITION DES BROCHES (Arduino Mega 2560)
 * ------------------------------------------------------------------------*/

/* --- Entrées capteurs --- */
const uint8_t PIN_ONEWIRE     = 22;  // Bus DS18B20 : T_sec (index 0) + T_cap (index 1)
const uint8_t PIN_DHT_AMB     = 24;  // DHT22 entrée d'air extérieur  -> T_amb / H_amb
const uint8_t PIN_DHT_EXTR    = 26;  // DHT22 gaine d'extraction      -> H_sec
const uint8_t PIN_FLAMME      = 28;  // Contrôleur de flamme (sortie TOR)

/* --- Entrées opérateur (contacts vers 0 V + INPUT_PULLUP, sauf AU) --- */
const uint8_t PIN_BTN_MENU    = 30;
const uint8_t PIN_BTN_UP      = 32;
const uint8_t PIN_BTN_DOWN    = 34;
const uint8_t PIN_BTN_OK      = 36;
const uint8_t PIN_BTN_START   = 38;
const uint8_t PIN_BTN_STOP    = 40;
const uint8_t PIN_AU_URGENCE  = 42;  // Arrêt d'urgence : contact NC (voir note sécurité setup())
const uint8_t PIN_BTN_REARM   = 44;  // Réarmement matériel (chemin 1, §15)

/* --- Sorties TOR (modules relais / drivers) --- */
const uint8_t PIN_V_H2        = 23;  // Électrovanne SOURCE hydrogène
const uint8_t PIN_V_BUT       = 25;  // Électrovanne SOURCE butane
const uint8_t PIN_EV1         = 27;  // Électrovanne de rampe V_Fl_1
const uint8_t PIN_EV2         = 29;  // Électrovanne de rampe V_Fl_2
const uint8_t PIN_EV3         = 31;  // Électrovanne de rampe V_Fl_3
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

/* --- Adresse et format de l'afficheur —
 * ÉCRAN 20x4 I2C (remplace le 16x2 d'origine). Même bus I2C (SDA/SCL/GND/VCC
 * inchangés), même bibliothèque LiquidCrystal_I2C : seul le module physique
 * change. L'adresse I2C peut différer selon le fournisseur (0x27 ou 0x3F) —
 * à vérifier avec un scanner I2C au premier branchement. */
const uint8_t LCD_ADRESSE     = 0x27;
const uint8_t LCD_COLONNES    = 20;
const uint8_t LCD_LIGNES      = 4;

/* --------------------------------------------------------------------------
 * 3. CONSTANTES DE RÉGLAGE
 *    Les valeurs de PROCÉDÉ (T_cible, hystérésis, temporisations, seuils...)
 *    sont dans la structure `Config` (section 6) et modifiables au menu.
 *    Ici : uniquement les valeurs par défaut, les bornes de saisie, les pas
 *    d'incrément et les constantes purement techniques (câblage, PWM, etc.)
 *    qui n'ont pas vocation à être touchées par l'opérateur.
 * ------------------------------------------------------------------------*/

/* --- Valeurs par défaut de Config (utilisées si l'EEPROM est vierge/invalide) --- */
const float    DEFAUT_T_CIBLE            = 55.0f;   // °C
const float    DEFAUT_H_INITIAL          = 80.0f;   // %
const float    DEFAUT_H_PRODUIT_CIBLE    = 10.0f;   // %
const bool     DEFAUT_MODE_AUTO          = true;
const uint8_t  DEFAUT_CHOIX_MODE         = 2;        // 1=Solaire,2=H2,3=GPL
const uint32_t DEFAUT_DUREE_MAX_CYCLE    = 600UL;    // min
const uint32_t DEFAUT_TEMPS_PROLONGATION = 180UL;    // min — plafond forcé (voir AVIS n°2)
const float    DEFAUT_HHYST              = 5.0f;     // °C
const uint32_t DEFAUT_TEMPS_MIN_FIN      = 120UL;    // min (2 h, §5)
const uint32_t DEFAUT_TEMPS_ARRET_AUTO   = 300UL;    // s (5 min, §6)
const uint32_t DEFAUT_TEMPS_PURGE        = 120UL;    // s (§8)
const uint32_t DEFAUT_TEMPS_ALLUMAGE     = 4UL;      // s
const float    DEFAUT_PRESS_H2_MIN       = 2.0f;     // bar
const int      DEFAUT_SEUIL_MQ8          = 350;
const int      DEFAUT_SEUIL_MQ6          = 350;

/* --- Bornes de saisie (menu) — AVIS n°4 : les temporisations de sécurité
 * (purge, allumage, seuils de fuite) sont rendues modifiables comme demandé,
 * mais bornées ici à une plage sûre : un opérateur ne peut pas les régler à
 * une valeur qui annule leur fonction de sécurité (ex. purge quasi nulle). */
const float    T_CIBLE_MIN = 30.0f,  T_CIBLE_MAX = 90.0f;
const float    T_INIT_MIN  = 0.0f,   T_INIT_MAX  = 60.0f;
const float    H_INITIAL_MIN = 0.0f, H_INITIAL_MAX = 100.0f;
const float    H_PRODUIT_MIN = 1.0f, H_PRODUIT_MAX = 50.0f;
const uint32_t DUREE_MAX_CYCLE_MIN = 15UL,  DUREE_MAX_CYCLE_MAX = 1440UL;   // min
const uint32_t TEMPS_PROLONGATION_MIN = 15UL, TEMPS_PROLONGATION_MAX = 720UL; // min
const float    HHYST_MIN = 1.0f,  HHYST_MAX = 15.0f;                        // °C
const uint32_t TEMPS_MIN_FIN_MIN = 10UL, TEMPS_MIN_FIN_MAX = 1440UL;        // min
const uint32_t TEMPS_ARRET_AUTO_MIN = 30UL, TEMPS_ARRET_AUTO_MAX = 1800UL;  // s
const uint32_t TEMPS_PURGE_MIN = 60UL,   TEMPS_PURGE_MAX = 300UL;           // s — PLANCHER DE SÉCURITÉ
const uint32_t TEMPS_ALLUMAGE_MIN = 2UL, TEMPS_ALLUMAGE_MAX = 10UL;         // s
const float    PRESS_H2_MIN_PLANCHER = 0.5f, PRESS_H2_MIN_PLAFOND = 8.0f;   // bar
const int      SEUIL_MQ_MIN = 100, SEUIL_MQ_MAX = 900;                     // sur 1023

/* --- Pas de réglage du menu --- */
const float    PAS_TEMPERATURE       = 1.0f;    // °C
const float    PAS_HUMIDITE          = 1.0f;    // %
const uint32_t PAS_DUREE_MIN         = 15UL;    // minutes
const float    PAS_HHYST             = 0.5f;    // °C
const uint32_t PAS_TEMPS_SEC         = 10UL;    // secondes (purge)
const uint32_t PAS_TEMPS_ALLUMAGE    = 1UL;     // secondes
const uint32_t PAS_TEMPS_ARRET_AUTO  = 30UL;    // secondes
const float    PAS_PRESSION          = 0.5f;    // bar
const int      PAS_SEUIL_MQ          = 10;

/* --- Écart minimal exploitable pour T_init/T_cible (garde-fou calcul seuils) --- */
const float    ECART_MIN_SEUILS      = 3.0f;    // °C

/* --- Nombre d'échecs d'allumage consécutifs avant escalade opérateur.
 * AJOUT (AVIS n°3) : le cahier des charges ne demande l'écran d'erreur avec
 * menu MANUEL/AUTOMATIQUE/RÉESSAYER que pour un échec de BASCULEMENT (§13).
 * Pour ne pas boucler indéfiniment purge->allumage->échec sur un simple
 * défaut d'allumeur (gaspillage de gaz, usure du Spark), la même escalade
 * est déclenchée après ce nombre d'échecs consécutifs, quelle qu'en soit
 * la cause. Un basculement raté déclenche l'escalade dès le 1er échec. */
const uint16_t MAX_ECHECS_AVANT_ALARME = 3;

/* --- Marge anti-court-cycle pour le retour automatique GPL -> H2 (AJOUT,
 * voir AVIS n°5) : évite un aller-retour de source pile au seuil. --- */
const float    MARGE_RETOUR_H2 = 0.5f;   // bar, ajoutée à Press_H2_Min

/* --- Confirmation avant de considérer le palier 0 % comme « durable »
 * (critère secondaire de fin de cycle, §6). AVIS n°6 dans le doc. --- */
const uint32_t CONFIRMATION_PALIER_0_MS = 60000UL;   // 60 s

/* --- Disponibilité de la source solaire (priorité 1) --- */
const float    SEUIL_T_CAP_SOLAIRE_ON  = 55.0f;   // Plaque capteur assez chaude -> solaire
const float    SEUIL_T_CAP_SOLAIRE_OFF = 45.0f;   // Plaque refroidie -> passage en combustion

/* --- Garde-fous numériques --- */
const float    H_FIN_MAX           = 95.0f;       // H_fin ne peut jamais dépasser cette valeur
const float    T_SEC_MAX_SECURITE  = 90.0f;       // °C : arrêt de sécurité indépendant du palier 0%

/* --- Chaîne de mesure de pression H2 (transmetteur 4-20 mA / shunt 250 Ohm) --- */
const float    PRESS_H2_PLEINE_ECHELLE = 10.0f;   // bar à 20 mA
const int      ADC_PRESS_4MA       = 204;         // 1,00 V  -> 4 mA
const int      ADC_PRESS_20MA      = 1023;        // 5,00 V  -> 20 mA
const int      ADC_PRESS_DEFAUT    = 180;         // en dessous : boucle 4-20 mA coupée

/* --- Divers --- */
const uint32_t PERIODE_CAPTEURS    = 1000UL;      // DS18B20 + DHT22 : 1 lecture/s
const uint32_t PERIODE_LCD         = 400UL;       // Rafraîchissement de l'afficheur
const uint32_t ANTI_REBOND         = 30UL;        // Anti-rebond des boutons
const uint32_t DUREE_REFROIDISSEMENT = 300000UL;  // 5 min de ventilation après séchage

/* --- Câblage logique --- */
const bool     RELAIS_ACTIF_BAS    = false;       // true si modules relais actifs à l'état bas
const int      NIVEAU_FLAMME_PRESENTE = HIGH;     // Niveau du contrôleur de flamme si flamme

/* --- Consignes de ventilation PWM par état/phase — ajustables ici uniquement --- */
const uint8_t PWM_ARRET             = 0;
const uint8_t PWM_PURGE_BALAYAGE    = 255;  // PURGE : maximal
const uint8_t PWM_PURGE_COMBUSTION  = 60;   // Paliers actifs : faible
const uint8_t PWM_PURGE_URGENCE     = 255;  // URGENCE_ATEX / ERREUR_COMBUSTION : 255
const uint8_t PWM_DISTRIB_SOLAIRE   = 220;  // MODE_SOLAIRE : élevé
const uint8_t PWM_EXTRACT_SOLAIRE   = 150;  // MODE_SOLAIRE : moyen
const uint8_t PWM_DISTRIB_PURGE     = 60;   // PURGE : faible
const uint8_t PWM_EXTRACT_PURGE     = 200;  // PURGE : élevé
const uint8_t PWM_EXTRACT_REFROID   = 90;   // SECHAGE_TERMINE : faible
/* Distribution / extraction « selon palier » : uniquement pour 100/67/33 %
 * (le 0 % est piloté par le profil PURGE, cf. §11 gererCombustion). */
const uint8_t PWM_DISTRIB_PAR_PALIER[3] = {255, 210, 165};
const uint8_t PWM_EXTRACT_PAR_PALIER[3] = {200, 165, 130};

/* --------------------------------------------------------------------------
 * 4. enum EtatFSM — ÉTATS PRINCIPAUX
 *
 *    Pour AJOUTER un état : ajouter une valeur ici, un `case` dans pasFSM()
 *    et un libellé dans nomEtat(). Rien d'autre à modifier.
 *
 *    Hiérarchie conceptuelle (§1 v2) :
 *      FONCTIONNEMENT_NORMAL
 *        ATTENTE_DEMARRAGE, CONFIG_MENU, MODE_SOLAIRE, MODE_H2, MODE_GPL,
 *        PROLONGATION, FIN_TEMPORISATION, SECHAGE_TERMINE,
 *        ERREUR_COMBUSTION
 *      URGENCE_ATEX          <- parallèle, prioritaire, hors hiérarchie
 * ------------------------------------------------------------------------*/
enum EtatFSM : uint8_t {
  ETAT_ATTENTE_DEMARRAGE = 0,  // État initial : tout fermé, ventilateurs à l'arrêt
  ETAT_CONFIG_MENU,            // Saisie des paramètres (LCD + 4 boutons)
  ETAT_MODE_SOLAIRE,           // Passif : AUCUNE électrovanne, ventilateurs seuls
  ETAT_MODE_H2,                // Combustion hydrogène (priorité 2)
  ETAT_MODE_GPL,                // Combustion GPL, secours (priorité 3)
  ETAT_PROLONGATION,           // Duree_Max_Cycle dépassée sans atteindre H_fin
  ETAT_FIN_TEMPORISATION,      // Consigne atteinte : confirmation d'arrêt (§6)
  ETAT_SECHAGE_TERMINE,        // Cycle terminé (normal ou forcé)
  ETAT_ERREUR_COMBUSTION,      // Échec de basculement ou échecs d'allumage répétés (§13)
  ETAT_URGENCE_ATEX,           // État PARALLÈLE et PRIORITAIRE (§15)
  NB_ETATS
};

/* --------------------------------------------------------------------------
 * 5. enum PhaseCombustion — SOUS-PHASES DE MODE_H2 / MODE_GPL
 *    Ces sous-phases sont STRICTEMENT IDENTIQUES pour H2 et GPL : elles sont
 *    traitées par la fonction unique gererCombustion() (§2 v1, §12 v1).
 *
 *    Le palier 0 % n'a PAS de phase dédiée : couper le gaz revient à
 *    retourner en PH_PURGE (voir armerPurge(PURGE_PALIER_0) et le
 *    traitement particulier en fin de purge dans gererCombustion()). C'est
 *    volontaire : la coupure à 0 % doit de toute façon repasser par une
 *    purge complète avant toute réouverture (§8), donc réutiliser la même
 *    sous-phase évite de dupliquer la logique de purge.
 * ------------------------------------------------------------------------*/
enum PhaseCombustion : uint8_t {
  PH_PURGE = 0,     // Gaz fermé, Spark = 0, ventilation de purge
  PH_ALLUMAGE,      // Ouverture au palier courant + Spark, jusqu'à Temps_Allumage
  PH_PALIER_100,    // EV1 + EV2 + EV3
  PH_PALIER_67,     // EV2 + EV3
  PH_PALIER_33      // EV3 seule
};

/* Indices de palier — 0 % est un véritable arrêt du brûleur (§2 v2) */
const uint8_t PALIER_100 = 0;
const uint8_t PALIER_67  = 1;
const uint8_t PALIER_33  = 2;
const uint8_t PALIER_0   = 3;

/* Raison d'entrée en purge — détermine le comportement à l'échéance des
 * Temps_Purge secondes (voir gererCombustion(), case PH_PURGE). */
enum RaisonPurge : uint8_t {
  PURGE_DEMARRAGE = 0,  // Démarrage de cycle à froid : palier forcé à 100 %
  PURGE_ECHEC,          // Échec d'allumage ou perte de flamme inattendue
  PURGE_BASCULEMENT,    // Changement de source H2<->GPL : palier CONSERVÉ
  PURGE_PALIER_0        // Coupure volontaire (consigne atteinte) : réévaluée en fin de purge
};

/* Raison d'entrée en ETAT_ERREUR_COMBUSTION (§13) */
enum RaisonErreurCombustion : uint8_t {
  ERR_BASCULEMENT = 0,      // Le basculement de source a échoué
  ERR_ALLUMAGE_REPETE       // Échecs d'allumage répétés (hors basculement)
};

/* Choix de l'opérateur en ETAT_ERREUR_COMBUSTION (§13) */
enum ChoixErreur : uint8_t {
  CHOIX_ERR_REESSAYER = 0,
  CHOIX_ERR_MANUEL,
  CHOIX_ERR_AUTOMATIQUE,
  NB_CHOIX_ERREUR
};

/* Sources d'énergie (Source_Active) */
const uint8_t SRC_SOLAIRE = 0;
const uint8_t SRC_H2      = 1;
const uint8_t SRC_GPL     = 2;

/* Champs du menu LCD (§7 v2 : liste complète des paramètres opérateur) */
enum ChampMenu : uint8_t {
  MENU_MODE_AUTO = 0,
  MENU_CHOIX_MODE,
  MENU_T_CIBLE,
  MENU_T_INIT,
  MENU_H_INITIAL,
  MENU_H_PRODUIT,
  MENU_DUREE_MAX,
  MENU_TEMPS_PROLONGATION,
  MENU_HHYST,
  MENU_TEMPS_PURGE,
  MENU_TEMPS_ALLUMAGE,
  MENU_TEMPS_MIN_FIN,
  MENU_TEMPS_ARRET_AUTO,
  MENU_PRESS_H2_MIN,
  MENU_SEUIL_MQ8,
  MENU_SEUIL_MQ6,
  MENU_REARMEMENT,     // Chemin 2 de réarmement ATEX (§15)
  NB_CHAMPS_MENU
};

/* Identifiants internes des boutons (anti-rebond + détection de front) */
enum IdxBouton : uint8_t {
  B_MENU = 0, B_UP, B_DOWN, B_OK, B_START, B_STOP, B_REARM, NB_BOUTONS
};

/* --------------------------------------------------------------------------
 * 6. VARIABLES
 * ------------------------------------------------------------------------*/

/* --- Entrées capteurs --- */
float    T_sec    = 20.0f;   // °C — chambre de séchage : GRANDEUR RÉGULÉE
float    H_sec    = 90.0f;   // %  — air extrait de la chambre : critère de fin de cycle
float    T_amb    = 20.0f;   // °C — air extérieur
float    H_amb    = 50.0f;   // %  — air extérieur : entre dans H_fin
float    T_cap    = 20.0f;   // °C — plaque capteur solaire / brûleur
bool     Flame    = false;   // Confirmation de flamme
int      MQ8_H2   = 0;       // Niveau de détection H2
int      MQ6_But  = 0;       // Niveau de détection GPL
float    Press_H2 = 0.0f;    // bar — pression du réservoir H2

/* --- Entrées opérateur (niveaux logiques « appuyé = true ») --- */
bool Btn_Menu = false, Btn_Up = false, Btn_Down = false, Btn_OK = false;
bool Btn_Start = false, Btn_Stop = false, Btn_Rearm = false;
bool AU_Urgence = false;

/* --------------------------------------------------------------------------
 * 6bis. STRUCTURE DE CONFIGURATION CENTRALISÉE (§17 v2)
 *   Tous les paramètres modifiables par l'opérateur sont regroupés ici et
 *   sauvegardés en EEPROM. Aucune valeur de réglage « magique » ne doit
 *   apparaître ailleurs dans le code : tout passe par Config.*
 * ------------------------------------------------------------------------*/
struct Configuration {
  uint16_t sentinelle;          // Marqueur de validité EEPROM (voir chargerConfig())

  float    T_cible;             // °C — consigne de séchage
  float    T_init;              // °C — mode manuel uniquement ; en auto, lue au capteur
  float    H_initial;           // % — humidité initiale du produit (saisie, §5 — affichage/journal, voir AVIS n°1)
  float    H_produit_cible;     // % — humidité résiduelle visée du produit
  bool     Mode_Auto;           // true = automatique, false = manuel
  uint8_t  Choix_Mode;          // 1 = Solaire, 2 = H2, 3 = GPL (mode manuel)
  uint32_t Duree_Max_Cycle;     // min — avant passage en PROLONGATION
  uint32_t Temps_Prolongation;  // min — plafond forcé de la prolongation (AVIS n°2)

  float    Hhyst;               // °C — bande d'hystérésis totale des paliers

  uint32_t Temps_Min_Fin;       // min — durée minimale avant d'autoriser H_sec<=H_fin
  uint32_t Temps_Arret_Auto;    // s — délai de confirmation en FIN_TEMPORISATION

  uint32_t Temps_Purge;         // s — durée de purge obligatoire
  uint32_t Temps_Allumage;      // s — délai max de confirmation de flamme
  float    Press_H2_Min;        // bar — pression H2 minimale (bascule GPL en dessous)

  int      Seuil_MQ8;           // Seuil de détection de fuite H2
  int      Seuil_MQ6;           // Seuil de détection de fuite GPL
};

const uint16_t CONFIG_SENTINELLE = 0xC0DE;  // Change si la struct change de forme -> réinit
const int      EEPROM_ADR_CONFIG = 0;

Configuration Config;

/* Bornage défensif : quelle que soit la provenance de la valeur (EEPROM
 * corrompue, saisie menu), on ne laisse jamais Config sortir des plages
 * sûres. Appelé après chargement EEPROM et après chaque édition menu. */
static void bornerConfig() {
  if (Config.T_cible < T_CIBLE_MIN) Config.T_cible = T_CIBLE_MIN;
  if (Config.T_cible > T_CIBLE_MAX) Config.T_cible = T_CIBLE_MAX;
  if (Config.T_init < T_INIT_MIN) Config.T_init = T_INIT_MIN;
  if (Config.T_init > T_INIT_MAX) Config.T_init = T_INIT_MAX;
  if (Config.H_initial < H_INITIAL_MIN) Config.H_initial = H_INITIAL_MIN;
  if (Config.H_initial > H_INITIAL_MAX) Config.H_initial = H_INITIAL_MAX;
  if (Config.H_produit_cible < H_PRODUIT_MIN) Config.H_produit_cible = H_PRODUIT_MIN;
  if (Config.H_produit_cible > H_PRODUIT_MAX) Config.H_produit_cible = H_PRODUIT_MAX;
  if (Config.Choix_Mode < 1 || Config.Choix_Mode > 3) Config.Choix_Mode = DEFAUT_CHOIX_MODE;
  if (Config.Duree_Max_Cycle < DUREE_MAX_CYCLE_MIN) Config.Duree_Max_Cycle = DUREE_MAX_CYCLE_MIN;
  if (Config.Duree_Max_Cycle > DUREE_MAX_CYCLE_MAX) Config.Duree_Max_Cycle = DUREE_MAX_CYCLE_MAX;
  if (Config.Temps_Prolongation < TEMPS_PROLONGATION_MIN) Config.Temps_Prolongation = TEMPS_PROLONGATION_MIN;
  if (Config.Temps_Prolongation > TEMPS_PROLONGATION_MAX) Config.Temps_Prolongation = TEMPS_PROLONGATION_MAX;
  if (Config.Hhyst < HHYST_MIN) Config.Hhyst = HHYST_MIN;
  if (Config.Hhyst > HHYST_MAX) Config.Hhyst = HHYST_MAX;
  if (Config.Temps_Min_Fin < TEMPS_MIN_FIN_MIN) Config.Temps_Min_Fin = TEMPS_MIN_FIN_MIN;
  if (Config.Temps_Min_Fin > TEMPS_MIN_FIN_MAX) Config.Temps_Min_Fin = TEMPS_MIN_FIN_MAX;
  if (Config.Temps_Arret_Auto < TEMPS_ARRET_AUTO_MIN) Config.Temps_Arret_Auto = TEMPS_ARRET_AUTO_MIN;
  if (Config.Temps_Arret_Auto > TEMPS_ARRET_AUTO_MAX) Config.Temps_Arret_Auto = TEMPS_ARRET_AUTO_MAX;
  if (Config.Temps_Purge < TEMPS_PURGE_MIN) Config.Temps_Purge = TEMPS_PURGE_MIN;      // plancher de sécurité
  if (Config.Temps_Purge > TEMPS_PURGE_MAX) Config.Temps_Purge = TEMPS_PURGE_MAX;
  if (Config.Temps_Allumage < TEMPS_ALLUMAGE_MIN) Config.Temps_Allumage = TEMPS_ALLUMAGE_MIN;
  if (Config.Temps_Allumage > TEMPS_ALLUMAGE_MAX) Config.Temps_Allumage = TEMPS_ALLUMAGE_MAX;
  if (Config.Press_H2_Min < PRESS_H2_MIN_PLANCHER) Config.Press_H2_Min = PRESS_H2_MIN_PLANCHER;
  if (Config.Press_H2_Min > PRESS_H2_MIN_PLAFOND) Config.Press_H2_Min = PRESS_H2_MIN_PLAFOND;
  if (Config.Seuil_MQ8 < SEUIL_MQ_MIN) Config.Seuil_MQ8 = SEUIL_MQ_MIN;
  if (Config.Seuil_MQ8 > SEUIL_MQ_MAX) Config.Seuil_MQ8 = SEUIL_MQ_MAX;
  if (Config.Seuil_MQ6 < SEUIL_MQ_MIN) Config.Seuil_MQ6 = SEUIL_MQ_MIN;
  if (Config.Seuil_MQ6 > SEUIL_MQ_MAX) Config.Seuil_MQ6 = SEUIL_MQ_MAX;
}

static void chargerDefautsConfig() {
  Config.sentinelle         = CONFIG_SENTINELLE;
  Config.T_cible            = DEFAUT_T_CIBLE;
  Config.T_init             = 25.0f;
  Config.H_initial          = DEFAUT_H_INITIAL;
  Config.H_produit_cible    = DEFAUT_H_PRODUIT_CIBLE;
  Config.Mode_Auto          = DEFAUT_MODE_AUTO;
  Config.Choix_Mode         = DEFAUT_CHOIX_MODE;
  Config.Duree_Max_Cycle    = DEFAUT_DUREE_MAX_CYCLE;
  Config.Temps_Prolongation = DEFAUT_TEMPS_PROLONGATION;
  Config.Hhyst              = DEFAUT_HHYST;
  Config.Temps_Min_Fin      = DEFAUT_TEMPS_MIN_FIN;
  Config.Temps_Arret_Auto   = DEFAUT_TEMPS_ARRET_AUTO;
  Config.Temps_Purge        = DEFAUT_TEMPS_PURGE;
  Config.Temps_Allumage     = DEFAUT_TEMPS_ALLUMAGE;
  Config.Press_H2_Min       = DEFAUT_PRESS_H2_MIN;
  Config.Seuil_MQ8          = DEFAUT_SEUIL_MQ8;
  Config.Seuil_MQ6          = DEFAUT_SEUIL_MQ6;
}

/* Charge la config EEPROM si elle est valide (sentinelle correcte), sinon
 * repart des valeurs par défaut. Un bornage systématique protège contre une
 * EEPROM partiellement corrompue (bits retournés) qui garderait la bonne
 * sentinelle mais des champs aberrants. */
static void chargerConfig() {
  EEPROM.get(EEPROM_ADR_CONFIG, Config);
  if (Config.sentinelle != CONFIG_SENTINELLE) {
    chargerDefautsConfig();
  }
  bornerConfig();
}

static void sauverConfig() {
  Config.sentinelle = CONFIG_SENTINELLE;
  EEPROM.put(EEPROM_ADR_CONFIG, Config);
}

/* --- Sorties actionneurs --- */
bool    V_H2 = false, V_But = false;
bool    EV1 = false, EV2 = false, EV3 = false;
bool    Spark = false, Buzzer = false;
uint8_t PWM_Purge = 0, PWM_Distrib = 0, PWM_Extract = 0;
int     Etat_LCD = ETAT_ATTENTE_DEMARRAGE;

/* --- Variables internes de la FSM --- */
EtatFSM         etat_courant     = ETAT_ATTENTE_DEMARRAGE;
PhaseCombustion phase_combustion = PH_PURGE;
RaisonPurge     raison_purge     = PURGE_DEMARRAGE;
uint8_t         palier           = PALIER_100;   // 0=100%, 1=67%, 2=33%, 3=0%
float           T1 = 35.0f, T2 = 45.0f, T3 = 55.0f;
float           H_fin = 60.0f;                   // Recalculé à chaque itération (§5)
uint32_t        chrono_purge = 0;                // Horodatage de début de purge
uint32_t        chrono_allumage = 0;             // Horodatage de début d'allumage
uint32_t        chrono_cycle = 0;                // Horodatage de début de cycle
uint32_t        chrono_prolongation = 0;         // Horodatage d'entrée en PROLONGATION
uint32_t        chrono_arret_auto = 0;           // Horodatage d'entrée en FIN_TEMPORISATION
uint32_t        chrono_palier0 = 0;              // Horodatage d'entrée au palier 0 % (confirmation)
uint8_t         Source_Active = SRC_SOLAIRE;
uint8_t         Menu_Index = MENU_MODE_AUTO;
RaisonErreurCombustion raison_erreur_combustion = ERR_ALLUMAGE_REPETE;
uint8_t         Choix_Erreur = CHOIX_ERR_REESSAYER;
bool            arret_force_duree = false;       // Affichage : Temps_Prolongation dépassé (§6bis)

/* --- Variables de service --- */
static uint32_t t_boucle = 0;                    // millis() figé pour toute l'itération
static uint32_t t_dernier_capteur = 0;
static uint32_t t_dernier_lcd = 0;
static uint32_t chrono_refroidissement = 0;
static uint16_t nb_echecs_allumage = 0;
static bool     demande_rearm_ihm = false;       // Chemin 2 de réarmement ATEX
static bool     quitter_menu = false;
static uint8_t  page_affichage = 0;               // Page de télémesure active (Btn_Menu en régulation)
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
static inline float demiHyst() { return Config.Hhyst / 2.0f; }

/* Consomme un front de bouton : un appui ne peut déclencher qu'une seule
 * action, même si pasFSM() et gererIHM() s'intéressent au même bouton. */
static bool frontPris(uint8_t idx) {
  if (!front[idx]) return false;
  front[idx] = false;
  return true;
}

static const char* nomEtat(EtatFSM e) {
  switch (e) {
    case ETAT_ATTENTE_DEMARRAGE:   return "ATTENTE";
    case ETAT_CONFIG_MENU:         return "MENU";
    case ETAT_MODE_SOLAIRE:        return "SOLAIRE";
    case ETAT_MODE_H2:             return "H2";
    case ETAT_MODE_GPL:            return "GPL";
    case ETAT_PROLONGATION:        return "PROLONG.";
    case ETAT_FIN_TEMPORISATION:   return "FIN TEMPO";
    case ETAT_SECHAGE_TERMINE:     return "TERMINE";
    case ETAT_ERREUR_COMBUSTION:   return "ERREUR";
    case ETAT_URGENCE_ATEX:        return "URGENCE";
    default:                       return "?";
  }
}

static const char* nomSource(uint8_t src) {
  if (src == SRC_SOLAIRE) return "SOLAIRE";
  if (src == SRC_H2)      return "H2";
  return "GPL";
}

/* Libellé du palier — 0 % s'affiche distinctement de PURGE, même si les
 * deux partagent phase_combustion == PH_PURGE (voir §5). */
static const char* nomPalierOuPhase() {
  if (phase_combustion == PH_PURGE) {
    return (raison_purge == PURGE_PALIER_0) ? "0% (veille)" : "PURGE";
  }
  switch (phase_combustion) {
    case PH_ALLUMAGE:   return "ALLUMAGE";
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
    Menu_Index = MENU_REARMEMENT;   // L'option de réarmement est présentée d'office
  }
  if (nouvel_etat == ETAT_SECHAGE_TERMINE) {
    chrono_refroidissement = t_boucle;
  }
  if (nouvel_etat == ETAT_PROLONGATION) {
    chrono_prolongation = t_boucle;
  }
  if (nouvel_etat == ETAT_FIN_TEMPORISATION) {
    chrono_arret_auto = t_boucle;
  }
  Serial.print("FSM -> ");
  Serial.println(nomEtat(nouvel_etat));
}

/* Fermeture complète du circuit gaz — priorité de sécurité n°4 (§19). */
static void fermerGaz() {
  V_H2 = false;
  V_But = false;
  EV1 = false;
  EV2 = false;
  EV3 = false;
  Spark = false;
}

/* Toute (re)mise en gaz est précédée d'une purge (§8). `raison` détermine ce
 * qui se passe à l'échéance de Config.Temps_Purge (voir gererCombustion()). */
static void armerPurge(RaisonPurge raison) {
  fermerGaz();
  raison_purge = raison;
  phase_combustion = PH_PURGE;
  chrono_purge = t_boucle;
  PWM_Purge   = PWM_PURGE_BALAYAGE;
  PWM_Distrib = PWM_DISTRIB_PURGE;
  PWM_Extract = PWM_EXTRACT_PURGE;
}

/* phase_combustion est DÉRIVÉE de `palier` pour les trois paliers actifs :
 * une seule source de vérité. Le palier 0 % n'a pas d'équivalent ici (voir
 * enum PhaseCombustion) ; on retombe sur PH_PURGE par sécurité si jamais
 * appelée avec PALIER_0 (ne devrait pas arriver en fonctionnement normal). */
static PhaseCombustion phaseDuPalier(uint8_t p) {
  if (p == PALIER_100) return PH_PALIER_100;
  if (p == PALIER_67)  return PH_PALIER_67;
  if (p == PALIER_33)  return PH_PALIER_33;
  return PH_PURGE;
}

/* Calcul de T1/T2/T3 à partir de T_cible et T_init (§3 v2 : équivalent du
 * calculateTemperatureThresholds() demandé, adapté au style du projet).
 * Découpage en tiers de [T_init ; T_cible] pour T1/T2 (comme en v1) ; T3 est
 * fixé à T_cible elle-même : c'est la lecture la plus fidèle de la règle
 * « quand T_sec atteint la consigne, on peut couper » (§2 v2). Voir
 * AVIS n°6 dans docs/FSM_SECHOIR.md pour la justification de ce choix, qui
 * n'était pas totalement explicite dans le cahier des charges. */
static void calculerSeuils() {
  if (Config.T_cible <= Config.T_init + ECART_MIN_SEUILS) {
    /* Consigne trop proche (ou en dessous) de la température initiale : les
     * seuils se confondent. On rabat T1 = T2 = T3 = T_cible : le brûleur
     * démarre à 100 % puis coupe dès que la consigne est franchie, au lieu
     * de produire des seuils inversés ou incohérents. */
    T1 = Config.T_cible;
    T2 = Config.T_cible;
    T3 = Config.T_cible;
  } else {
    float delta = Config.T_cible - Config.T_init;
    T1 = Config.T_init + delta / 3.0f;
    T2 = Config.T_init + 2.0f * delta / 3.0f;
    T3 = Config.T_cible;
  }
}

/* Hystérésis mémorisée par le palier courant, sur QUATRE niveaux (§2, §4 v2).
 * Un seul franchissement de seuil par itération : c'est ce qui fait de la
 * modulation une véritable machine à états et non des comparaisons
 * indépendantes recalculées à chaque pas.
 *
 * Sens des transitions (§4 v2, repris tel quel) :
 *   Tsec < Ti - Hhyst/2  -> palier supérieur (plus de puissance : on a froid)
 *   Tsec > Ti + Hhyst/2  -> palier inférieur (moins de puissance : ça chauffe)
 *
 * IMPORTANT : cette fonction ne fait QUE calculer le palier demandé par la
 * régulation. Le passage effectif à 0 % (fermeture de gaz + purge) est
 * traité par l'appelant (gererCombustion()), qui doit distinguer une
 * coupure volontaire d'une perte de flamme — voir §7.3 v1 règle 2, toujours
 * en vigueur : la vérification de flamme précède toujours l'action sur les
 * électrovannes. */
static void majPalier() {
  float demi = demiHyst();
  switch (palier) {
    case PALIER_100:
      if (T_sec >= T1 + demi) palier = PALIER_67;
      break;
    case PALIER_67:
      if (T_sec >= T2 + demi)      palier = PALIER_33;
      else if (T_sec < T1 - demi)  palier = PALIER_100;
      break;
    case PALIER_33:
      if (T_sec >= T3 + demi)      palier = PALIER_0;
      else if (T_sec < T2 - demi)  palier = PALIER_67;
      break;
    case PALIER_0:
      /* Filet de sécurité : normalement on ne repasse par ce switch qu'une
       * fois la flamme rallumée (palier déjà remonté à 33 % par
       * gererCombustion()), mais on couvre le cas quand même. */
      if (T_sec < T3 - demi) palier = PALIER_33;
      break;
    default:
      palier = PALIER_100;
      break;
  }
}

/* Cause d'urgence encore présente ? (§15) */
static bool causeUrgencePresente() {
  return (MQ8_H2 > Config.Seuil_MQ8) || (MQ6_But > Config.Seuil_MQ6) || AU_Urgence;
}

/* États dans lesquels un cycle de séchage est effectivement en cours (pour
 * les transitions de fin de cycle / prolongation / arrêt propre). */
static bool cycleEnCours() {
  return etat_courant == ETAT_MODE_SOLAIRE || etat_courant == ETAT_MODE_H2 ||
         etat_courant == ETAT_MODE_GPL     || etat_courant == ETAT_PROLONGATION ||
         etat_courant == ETAT_FIN_TEMPORISATION;
}

/* Entrée dans l'état d'erreur de combustion (§13) : arrêt sûr + alarme +
 * menu de reprise. Ni le basculement raté ni les échecs répétés ne doivent
 * continuer à boucler tout seuls indéfiniment. */
static void entrerErreurCombustion(RaisonErreurCombustion raison) {
  fermerGaz();
  raison_erreur_combustion = raison;
  Choix_Erreur = CHOIX_ERR_REESSAYER;
  Buzzer = true;
  changerEtat(ETAT_ERREUR_COMBUSTION);
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

  /* Configuration opérateur : chargée depuis l'EEPROM (ou valeurs par
   * défaut si absente/invalide). §7 v2. */
  chargerConfig();

  /* Capteurs. */
  sondes.begin();
  sondes.setResolution(12);
  sondes.setWaitForConversion(false);   // conversion asynchrone : loop() non bloquée
  sondes.requestTemperatures();
  dhtAmb.begin();
  dhtExtr.begin();

  /* Afficheur 20x4 I2C. */
  Wire.begin();
  lcd.init();
  lcd.backlight();
  lcd.clear();

  delay(800);                 // 1re conversion DS18B20 (12 bits : 750 ms)
  t_boucle = millis();
  lireCapteursLents();
  t_dernier_capteur = t_boucle;
  H_fin = Config.H_produit_cible + H_amb;

  etat_courant = ETAT_ATTENTE_DEMARRAGE;
  phase_combustion = PH_PURGE;
  raison_purge = PURGE_DEMARRAGE;
  palier = PALIER_100;
  Serial.println("Sechoir hybride v2 : initialisation terminee");
}

/* --------------------------------------------------------------------------
 * 8. loop()
 * ------------------------------------------------------------------------*/
void loop() {
  lireEntrees();     // acquisition + recalcul de H_fin
  pasFSM();          // machine à états
  ecrireSorties();   // application physique des sorties
  gererIHM();        // menu LCD + 4 boutons + réarmement
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

  v = dhtAmb.readTemperature(); if (!estNaN(v)) T_amb = v;
  v = dhtAmb.readHumidity();    if (!estNaN(v)) H_amb = v;
  v = dhtExtr.readHumidity();   if (!estNaN(v)) H_sec = v;
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

  /* --- H_fin est RECALCULÉ À CHAQUE ITÉRATION, jamais figé (§5) --- */
  H_fin = Config.H_produit_cible + H_amb;
  if (H_fin > H_FIN_MAX) H_fin = H_FIN_MAX;
}

/* --------------------------------------------------------------------------
 * 10. appliquerPalier() — palier -> état des trois électrovannes de rampe
 *     100 % = EV1+EV2+EV3 | 67 % = EV2+EV3 | 33 % = EV3 seule | 0 % = aucune.
 *     Le 0 % ferme réellement tout : ce n'est plus un plancher (§2 v2).
 * ------------------------------------------------------------------------*/
void appliquerPalier() {
  EV1 = (palier == PALIER_100);
  EV2 = (palier == PALIER_100 || palier == PALIER_67);
  EV3 = (palier != PALIER_0);
}

/* --------------------------------------------------------------------------
 * 11. gererCombustion(bool utiliseH2)
 *     FONCTION UNIQUE PARTAGÉE par MODE_H2 et MODE_GPL. Seule la vanne
 *     source diffère : V_H2 <-> V_But. Ne jamais dupliquer cette logique
 *     entre les deux modes.
 * ------------------------------------------------------------------------*/
void gererCombustion(bool utiliseH2) {
  switch (phase_combustion) {

    /* ---- PURGE : gaz fermé, Spark = 0, balayage maximal (§8, §9, §10) ---- */
    case PH_PURGE: {
      fermerGaz();
      PWM_Purge   = PWM_PURGE_BALAYAGE;
      PWM_Distrib = PWM_DISTRIB_PURGE;
      PWM_Extract = PWM_EXTRACT_PURGE;

      bool purge_ecoulee = (t_boucle - chrono_purge) >= (Config.Temps_Purge * 1000UL);
      if (!purge_ecoulee) break;

      if (raison_purge == PURGE_PALIER_0) {
        /* Coupure volontaire (consigne atteinte) : la purge de sécurité est
         * acquise depuis longtemps si T_sec est resté haut ; on ne la
         * réarme PAS à chaque itération (le ventilateur tourne en continu
         * depuis l'entrée en purge, donc le balayage n'a fait que
         * s'allonger — jamais raccourcir). On ne rouvre le gaz QUE si la
         * régulation redemande réellement le palier 33 % : ceci évite un
         * allumage pour rien si la température est retombée puis remontée
         * entre-temps (AJOUT au cahier des charges, voir AVIS n°3). */
        if (T_sec >= T3 - demiHyst()) {
          break;   // toujours en régime « cible atteinte » : on reste en veille
        }
        palier = PALIER_33;   // la demande est revenue : on peut rallumer au plancher haut
      }

      phase_combustion = PH_ALLUMAGE;
      chrono_allumage = t_boucle;
      break;
    }

    /* ---- ALLUMAGE : ouverture AU PALIER COURANT + Spark, jusqu'à
     * Config.Temps_Allumage (§9, §10). Le palier courant est déterminé par
     * la raison de la purge précédente :
     *   - PURGE_DEMARRAGE / PURGE_BASCULEMENT -> 100 % ou palier conservé
     *     (déjà positionné par demarrerCycle() / arbitrageSource()) ;
     *   - PURGE_ECHEC -> palier inchangé (nouvelle tentative au même niveau) ;
     *   - PURGE_PALIER_0 -> 33 % (repositionné juste au-dessus, cas PH_PURGE).
     * ---- */
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
        nb_echecs_allumage = 0;             // succès : on réinitialise le compteur
        phase_combustion = phaseDuPalier(palier);
      } else if ((t_boucle - chrono_allumage) >= (Config.Temps_Allumage * 1000UL)) {
        nb_echecs_allumage++;
        Serial.println("Echec d'allumage");
        if (raison_purge == PURGE_BASCULEMENT) {
          /* §13 : un échec de basculement n'est jamais bouclé silencieusement. */
          Serial.println("Echec du basculement de source -> ERREUR_COMBUSTION");
          entrerErreurCombustion(ERR_BASCULEMENT);
        } else if (nb_echecs_allumage >= MAX_ECHECS_AVANT_ALARME) {
          Serial.println("Echecs d'allumage repetes -> ERREUR_COMBUSTION");
          entrerErreurCombustion(ERR_ALLUMAGE_REPETE);
        } else {
          armerPurge(PURGE_ECHEC);          // fermeture totale puis nouvelle purge
        }
      }
      break;

    /* ---- PALIERS ACTIFS : régulation par hystérésis à 4 niveaux (§4 v2) ---- */
    case PH_PALIER_100:
    case PH_PALIER_67:
    case PH_PALIER_33:
      /* La flamme est VÉRIFIÉE D'ABORD. L'état des électrovannes n'est
       * appliqué qu'ensuite — jamais avant (§7.3 v1 règle 2, toujours en
       * vigueur ; §14 v2 : extinction de flamme = situation de sécurité). */
      if (!Flame) {
        nb_echecs_allumage++;
        Serial.println("Perte de flamme inattendue : fermeture immediate");
        if (nb_echecs_allumage >= MAX_ECHECS_AVANT_ALARME) {
          entrerErreurCombustion(ERR_ALLUMAGE_REPETE);
        } else {
          armerPurge(PURGE_ECHEC);
        }
        break;
      }

      {
        uint8_t palier_avant = palier;
        majPalier();
        if (palier == PALIER_0 && palier_avant != PALIER_0) {
          /* Consigne atteinte : coupure VOLONTAIRE, pas une panne. On ferme
           * tout et on impose la purge de sécurité avant toute réouverture,
           * exactement comme le demande §8 (« avant TOUTE ouverture de
           * gaz »), mais sans déclencher d'alarme ni d'échec comptabilisé. */
          Serial.println("Consigne atteinte : coupure du bruleur (palier 0%)");
          chrono_palier0 = t_boucle;
          armerPurge(PURGE_PALIER_0);
          break;
        }
      }

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
 * 12. pasFSM() — MACHINE À ÉTATS PRINCIPALE
 * ------------------------------------------------------------------------*/

/* MODE_SOLAIRE : purement passif, aucune électrovanne. */
static void etapeSolaire() {
  fermerGaz();
  PWM_Purge   = PWM_ARRET;
  PWM_Distrib = PWM_DISTRIB_SOLAIRE;
  PWM_Extract = PWM_EXTRACT_SOLAIRE;
  /* Brûleur à l'arrêt : la purge reste armée, de sorte qu'un futur passage en
   * combustion disposera bien de ses Temps_Purge secondes complètes. */
  phase_combustion = PH_PURGE;
  raison_purge = PURGE_DEMARRAGE;
  chrono_purge = t_boucle;
}

/* Arbitrage automatique des trois sources. Renvoie true si Source_Active a
 * changé. En mode manuel, l'opérateur impose la source : aucun arbitrage.
 * Ne jamais ouvrir H2 et GPL simultanément (§19) : ce n'est de toute façon
 * jamais possible ici puisque armerPurge() ferme systématiquement les deux
 * vannes source avant tout changement de Source_Active. */
static bool arbitrageSource() {
  if (!Config.Mode_Auto) return false;

  /* Priorité 1 : retour au solaire dès qu'il redevient disponible. */
  if (Source_Active != SRC_SOLAIRE && T_cap >= SEUIL_T_CAP_SOLAIRE_ON) {
    fermerGaz();
    Source_Active = SRC_SOLAIRE;
    return true;
  }

  /* Solaire insuffisant -> combustion. DÉMARRAGE À FROID : palier 100 %. */
  if (Source_Active == SRC_SOLAIRE && T_cap < SEUIL_T_CAP_SOLAIRE_OFF) {
    Source_Active = (Press_H2 >= Config.Press_H2_Min) ? SRC_H2 : SRC_GPL;
    palier = PALIER_100;
    armerPurge(PURGE_DEMARRAGE);
    return true;
  }

  /* Réservoir H2 épuisé -> GPL de secours.
   * Le PALIER COURANT EST CONSERVÉ mais la purge reste obligatoire avant la
   * remise en gaz (§8, §11 v2). */
  if (Source_Active == SRC_H2 && Press_H2 < Config.Press_H2_Min) {
    Serial.println("Pression H2 basse : bascule GPL, palier conserve");
    armerPurge(PURGE_BASCULEMENT);
    Source_Active = SRC_GPL;
    return true;
  }

  /* AJOUT (voir AVIS n°5) : retour automatique GPL -> H2 dès que la pression
   * est de nouveau suffisante, avec une marge anti-court-cycle. H2 reste
   * prioritaire sur le GPL (§2 : GPL = secours uniquement). Le palier
   * courant est là aussi conservé, purge obligatoire avant réouverture. */
  if (Source_Active == SRC_GPL && Press_H2 >= (Config.Press_H2_Min + MARGE_RETOUR_H2)) {
    Serial.println("Pression H2 retablie : bascule GPL -> H2, palier conserve");
    armerPurge(PURGE_BASCULEMENT);
    Source_Active = SRC_H2;
    return true;
  }

  return false;
}

static void synchroniserEtatSurSource() {
  if (Source_Active == SRC_SOLAIRE)  changerEtat(ETAT_MODE_SOLAIRE);
  else if (Source_Active == SRC_H2)  changerEtat(ETAT_MODE_H2);
  else                               changerEtat(ETAT_MODE_GPL);
}

/* Cœur de régulation commun à MODE_SOLAIRE / MODE_H2 / MODE_GPL /
 * PROLONGATION / FIN_TEMPORISATION. `synchroniser` est faux quand l'état
 * affiché ne doit pas être écrasé par un changement de source (PROLONGATION,
 * FIN_TEMPORISATION) : la source peut changer sans que l'état principal en
 * sorte. */
static void etapeRegulation(bool synchroniser) {
  arbitrageSource();
  if (synchroniser) synchroniserEtatSurSource();
  if (Source_Active == SRC_SOLAIRE) etapeSolaire();
  else                              gererCombustion(Source_Active == SRC_H2);
}

/* Démarrage d'un cycle depuis ATTENTE_DEMARRAGE, CONFIG_MENU ou SECHAGE_TERMINE. */
static void demarrerCycle() {
  if (Config.Mode_Auto) Config.T_init = T_sec;   // en auto, T_init est lue au capteur
  calculerSeuils();

  palier = PALIER_100;                // le démarrage est TOUJOURS à 100 %
  chrono_cycle = t_boucle;
  nb_echecs_allumage = 0;
  arret_force_duree = false;

  if (Config.Mode_Auto) {
    if (T_cap >= SEUIL_T_CAP_SOLAIRE_ON)              Source_Active = SRC_SOLAIRE;
    else if (Press_H2 >= Config.Press_H2_Min)         Source_Active = SRC_H2;
    else                                               Source_Active = SRC_GPL;
  } else {
    if (Config.Choix_Mode == 1)      Source_Active = SRC_SOLAIRE;
    else if (Config.Choix_Mode == 2) Source_Active = SRC_H2;
    else                              Source_Active = SRC_GPL;
  }

  armerPurge(PURGE_DEMARRAGE);        // purge avant toute mise en gaz
  synchroniserEtatSurSource();
}

/* Toutes les sorties forcées en position de repli (§15). */
static void appliquerUrgence() {
  fermerGaz();
  PWM_Purge   = PWM_PURGE_URGENCE;    // 255
  PWM_Distrib = PWM_ARRET;            // 0
  PWM_Extract = PWM_PURGE_URGENCE;    // 255
  Buzzer = true;
}

/* Le stop-request (H_sec<=H_fin, verrouillé par Temps_Min_Fin, ou palier 0 %
 * durable) est-il actif ? Centralisé pour être utilisé à la fois pour
 * ENTRER en FIN_TEMPORISATION et pour l'ANNULER si la condition redevient
 * fausse (ex. H_sec remonte transitoirement). */
static bool conditionFinDeCycle() {
  bool duree_min_ecoulee = (t_boucle - chrono_cycle) >= (Config.Temps_Min_Fin * 60000UL);
  if (!duree_min_ecoulee) return false;

  if (H_sec <= H_fin) return true;

  /* Critère secondaire (§6 v2) : T_sec a « durablement » atteint T_cible.
   * On interprète cela comme le palier 0 % maintenu depuis au moins
   * CONFIRMATION_PALIER_0_MS (voir AVIS n°6) — évite qu'un pic isolé de
   * température déclenche une demande d'arrêt. */
  bool a_palier_0 = (phase_combustion == PH_PURGE && raison_purge == PURGE_PALIER_0);
  if (a_palier_0 && (t_boucle - chrono_palier0) >= CONFIRMATION_PALIER_0_MS) return true;

  return false;
}

void pasFSM() {

  /* ===== switch principal : UN CASE PAR ÉTAT ===== */
  switch (etat_courant) {

    /* --- État initial : tout fermé, ventilateurs à l'arrêt --- */
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
      if (quitter_menu) { quitter_menu = false; sauverConfig(); changerEtat(ETAT_ATTENTE_DEMARRAGE); }
      else if (frontPris(B_START)) { sauverConfig(); demarrerCycle(); }
      break;

    /* --- Solaire : passif, aucune électrovanne --- */
    case ETAT_MODE_SOLAIRE:
      Source_Active = SRC_SOLAIRE;
      page_affichage = frontPris(B_MENU) ? (uint8_t)(1 - page_affichage) : page_affichage;
      etapeRegulation(true);
      break;

    /* --- Combustion hydrogène : PURGE -> ALLUMAGE -> PALIERS --- */
    case ETAT_MODE_H2:
      Source_Active = SRC_H2;
      page_affichage = frontPris(B_MENU) ? (uint8_t)(1 - page_affichage) : page_affichage;
      etapeRegulation(true);
      break;

    /* --- Combustion GPL (secours) : logique STRICTEMENT identique à H2 --- */
    case ETAT_MODE_GPL:
      Source_Active = SRC_GPL;
      page_affichage = frontPris(B_MENU) ? (uint8_t)(1 - page_affichage) : page_affichage;
      etapeRegulation(true);
      break;

    /* --- Prolongation : régulation normale, plafonnée par Temps_Prolongation
     * (voir AVIS n°2 : ce plafond est un ajout du cahier des charges v2 qui
     * contredit la règle v1 « aucune limite de temps ». Assumé tel quel :
     * si le plafond est atteint sans que H_fin soit atteint, le cycle est
     * arrêté de force, produit potentiellement pas assez sec — voir la
     * transition prioritaire dédiée plus bas). --- */
    case ETAT_PROLONGATION:
      page_affichage = frontPris(B_MENU) ? (uint8_t)(1 - page_affichage) : page_affichage;
      etapeRegulation(false);
      break;

    /* --- Attente de confirmation d'arrêt (§6 v2) : la régulation continue
     * normalement (le séchage n'est pas interrompu pendant la fenêtre de
     * confirmation), le LCD affiche la demande + le compte à rebours. --- */
    case ETAT_FIN_TEMPORISATION:
      etapeRegulation(false);
      if (frontPris(B_OK) || frontPris(B_STOP)) {
        fermerGaz();
        changerEtat(ETAT_SECHAGE_TERMINE);
      } else if (frontPris(B_MENU)) {
        /* Report manuel : l'opérateur juge que ce n'est pas fini et redonne
         * une pleine fenêtre de confirmation. NE PAS revenir directement à
         * MODE_H2/GPL/SOLAIRE ici : tant que la condition de fin (H_sec <=
         * H_fin) reste vraie, la transition prioritaire n°5 replongerait
         * immédiatement en FIN_TEMPORISATION dès l'itération suivante — un
         * aller-retour sans effet. Reporter l'échéance est le seul choix
         * cohérent avec une condition qui persiste réellement. */
        Serial.println("Arret automatique reporte par l'operateur");
        chrono_arret_auto = t_boucle;
      } else if (!conditionFinDeCycle()) {
        /* La condition qui a déclenché la demande d'arrêt n'est plus vraie
         * (ex. H_sec remonté transitoirement) : on annule automatiquement
         * plutôt que d'attendre bêtement un minuteur sur une donnée
         * périmée (AJOUT, robustesse). */
        changerEtat((Source_Active == SRC_SOLAIRE) ? ETAT_MODE_SOLAIRE :
                    (Source_Active == SRC_H2) ? ETAT_MODE_H2 : ETAT_MODE_GPL);
      } else if ((t_boucle - chrono_arret_auto) >= (Config.Temps_Arret_Auto * 1000UL)) {
        fermerGaz();
        changerEtat(ETAT_SECHAGE_TERMINE);
      }
      break;

    /* --- Séchage terminé : gaz fermé, extraction faible pour refroidir --- */
    case ETAT_SECHAGE_TERMINE:
      fermerGaz();
      PWM_Purge = PWM_ARRET;
      PWM_Distrib = PWM_ARRET;
      PWM_Extract = PWM_EXTRACT_REFROID;
      if (frontPris(B_START)) { arret_force_duree = false; demarrerCycle(); }
      else if (frontPris(B_MENU)) { Menu_Index = MENU_MODE_AUTO; changerEtat(ETAT_CONFIG_MENU); }
      else if ((t_boucle - chrono_refroidissement) >= DUREE_REFROIDISSEMENT) {
        changerEtat(ETAT_ATTENTE_DEMARRAGE);
      }
      break;

    /* --- Erreur de combustion (§13) : basculement raté ou échecs répétés.
     * Aucune réouverture automatique ; l'opérateur choisit REESSAYER /
     * MANUEL / AUTOMATIQUE. --- */
    case ETAT_ERREUR_COMBUSTION:
      fermerGaz();
      PWM_Purge   = PWM_PURGE_URGENCE;
      PWM_Distrib = PWM_ARRET;
      PWM_Extract = PWM_EXTRACT_PURGE;
      Buzzer = true;
      if (frontPris(B_MENU)) Choix_Erreur = (uint8_t)((Choix_Erreur + 1) % NB_CHOIX_ERREUR);
      if (frontPris(B_STOP)) {
        Buzzer = false;
        changerEtat(ETAT_SECHAGE_TERMINE);
      } else if (frontPris(B_OK)) {
        Buzzer = false;
        RaisonPurge raison_reprise = (raison_erreur_combustion == ERR_BASCULEMENT)
                                        ? PURGE_BASCULEMENT : PURGE_ECHEC;
        switch (Choix_Erreur) {
          case CHOIX_ERR_REESSAYER:
            nb_echecs_allumage = 0;
            armerPurge(raison_reprise);
            synchroniserEtatSurSource();
            break;
          case CHOIX_ERR_MANUEL:
            Config.Mode_Auto = false;
            sauverConfig();
            changerEtat(ETAT_ATTENTE_DEMARRAGE);
            break;
          case CHOIX_ERR_AUTOMATIQUE:
            Config.Mode_Auto = true;
            sauverConfig();
            nb_echecs_allumage = 0;
            armerPurge(raison_reprise);
            synchroniserEtatSurSource();
            break;
        }
      }
      break;

    /* --- Urgence ATEX : repli permanent + double chemin de réarmement (§15) --- */
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
          raison_purge = PURGE_DEMARRAGE;
          Menu_Index = MENU_MODE_AUTO;
          changerEtat(ETAT_ATTENTE_DEMARRAGE);
        }
      }
      break;

    default:
      changerEtat(ETAT_ATTENTE_DEMARRAGE);
      break;
  }

  /* ===== TRANSITIONS PRIORITAIRES — hors du switch, APRÈS lui =====
   * Elles s'appliquent depuis n'importe quel état et écrasent, si besoin, les
   * sorties calculées ci-dessus. ecrireSorties() n'étant appelée qu'ensuite,
   * rien d'incohérent n'atteint jamais les actionneurs. Ordre = priorité de
   * sécurité décroissante (§19 v2). */

  /* 1. URGENCE_ATEX — priorité absolue, évaluée à chaque itération. */
  if (causeUrgencePresente()) {
    if (etat_courant != ETAT_URGENCE_ATEX) {
      Serial.println("URGENCE ATEX : fuite gaz ou arret d'urgence");
      changerEtat(ETAT_URGENCE_ATEX);
    }
    appliquerUrgence();
    return;                       // aucune autre transition ne peut s'appliquer
  }

  /* 2. Arrêt propre demandé par l'opérateur (Btn_Stop), depuis un cycle en
   * cours (les cas particuliers ETAT_FIN_TEMPORISATION / ERREUR_COMBUSTION
   * traitent déjà Btn_Stop eux-mêmes dans leur propre case). */
  if (cycleEnCours() && etat_courant != ETAT_FIN_TEMPORISATION && frontPris(B_STOP)) {
    fermerGaz();
    changerEtat(ETAT_SECHAGE_TERMINE);
    return;
  }

  /* 3. Sécurité surchauffe, indépendante du palier 0 % : si jamais la
   * régulation par palier était mise en défaut (sonde décalée, charge
   * anormale), ce garde-fou coupe quand même le brûleur. */
  if (cycleEnCours() && T_sec >= T_SEC_MAX_SECURITE) {
    Serial.println("Surchauffe chambre : arret du bruleur");
    fermerGaz();
    changerEtat(ETAT_SECHAGE_TERMINE);
    return;
  }

  /* 4. Plafond de PROLONGATION (§6 v2, AVIS n°2) : arrêt forcé, même si
   * H_fin n'est pas atteint. Direct vers SECHAGE_TERMINE (pas de fenêtre de
   * confirmation : le plafond est déjà, par définition, un dépassement). */
  if (etat_courant == ETAT_PROLONGATION &&
      (t_boucle - chrono_prolongation) >= (Config.Temps_Prolongation * 60000UL)) {
    Serial.println("Temps_Prolongation depasse : arret force, humidite cible non garantie");
    arret_force_duree = true;
    fermerGaz();
    changerEtat(ETAT_SECHAGE_TERMINE);
    return;
  }

  /* 5. Demande d'arrêt (§6 v2) : H_sec <= H_fin (verrouillé par
   * Temps_Min_Fin) ou consigne atteinte durablement -> fenêtre de
   * confirmation avant arrêt automatique. */
  if ((etat_courant == ETAT_MODE_SOLAIRE || etat_courant == ETAT_MODE_H2 ||
       etat_courant == ETAT_MODE_GPL || etat_courant == ETAT_PROLONGATION) &&
      conditionFinDeCycle()) {
    changerEtat(ETAT_FIN_TEMPORISATION);
    return;
  }

  /* 6. Durée maximale dépassée sans avoir atteint la condition de fin ->
   * PROLONGATION. */
  if (cycleEnCours() && etat_courant != ETAT_PROLONGATION &&
      etat_courant != ETAT_FIN_TEMPORISATION &&
      (t_boucle - chrono_cycle) >= (Config.Duree_Max_Cycle * 60000UL)) {
    changerEtat(ETAT_PROLONGATION);
  }
}

/* --------------------------------------------------------------------------
 * 13. ecrireSorties() — seul point d'accès aux actionneurs
 * ------------------------------------------------------------------------*/
void ecrireSorties() {
  /* Verrou de sécurité défensif (§19) : même si la logique en amont garantit
   * déjà l'exclusion mutuelle H2/GPL, on le revérifie ici, au plus près du
   * matériel, avant toute écriture physique. */
  if (V_H2 && V_But) {
    Serial.println("!! INCOHERENCE : V_H2 et V_But simultanement demandees -> fermeture des deux");
    V_H2 = false;
    V_But = false;
  }

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
 * 14. gererIHM() — menu LCD (20x4) + 4 boutons + réarmement
 * ------------------------------------------------------------------------*/

/* Édite le champ courant du menu selon `sens` (+1 / -1), puis reborne
 * systématiquement toute la config (défense en profondeur : un champ ne
 * peut jamais sortir de sa plage sûre, quel que soit le pas appliqué). */
static void modifierChampMenu(int sens) {
  switch (Menu_Index) {
    case MENU_MODE_AUTO:
      Config.Mode_Auto = !Config.Mode_Auto;
      break;
    case MENU_CHOIX_MODE:
      if (sens > 0) Config.Choix_Mode = (Config.Choix_Mode >= 3) ? 1 : (uint8_t)(Config.Choix_Mode + 1);
      else          Config.Choix_Mode = (Config.Choix_Mode <= 1) ? 3 : (uint8_t)(Config.Choix_Mode - 1);
      break;
    case MENU_T_CIBLE:
      Config.T_cible += sens * PAS_TEMPERATURE;
      break;
    case MENU_T_INIT:
      Config.T_init += sens * PAS_TEMPERATURE;
      break;
    case MENU_H_INITIAL:
      Config.H_initial += sens * PAS_HUMIDITE;
      break;
    case MENU_H_PRODUIT:
      Config.H_produit_cible += sens * PAS_HUMIDITE;
      break;
    case MENU_DUREE_MAX:
      Config.Duree_Max_Cycle = (sens > 0) ? (Config.Duree_Max_Cycle + PAS_DUREE_MIN)
                              : (Config.Duree_Max_Cycle > PAS_DUREE_MIN ? Config.Duree_Max_Cycle - PAS_DUREE_MIN : 0UL);
      break;
    case MENU_TEMPS_PROLONGATION:
      Config.Temps_Prolongation = (sens > 0) ? (Config.Temps_Prolongation + PAS_DUREE_MIN)
                                 : (Config.Temps_Prolongation > PAS_DUREE_MIN ? Config.Temps_Prolongation - PAS_DUREE_MIN : 0UL);
      break;
    case MENU_HHYST:
      Config.Hhyst += sens * PAS_HHYST;
      break;
    case MENU_TEMPS_PURGE:
      Config.Temps_Purge = (sens > 0) ? (Config.Temps_Purge + PAS_TEMPS_SEC)
                          : (Config.Temps_Purge > PAS_TEMPS_SEC ? Config.Temps_Purge - PAS_TEMPS_SEC : 0UL);
      break;
    case MENU_TEMPS_ALLUMAGE:
      Config.Temps_Allumage = (sens > 0) ? (Config.Temps_Allumage + PAS_TEMPS_ALLUMAGE)
                             : (Config.Temps_Allumage > PAS_TEMPS_ALLUMAGE ? Config.Temps_Allumage - PAS_TEMPS_ALLUMAGE : 0UL);
      break;
    case MENU_TEMPS_MIN_FIN:
      Config.Temps_Min_Fin = (sens > 0) ? (Config.Temps_Min_Fin + PAS_DUREE_MIN)
                            : (Config.Temps_Min_Fin > PAS_DUREE_MIN ? Config.Temps_Min_Fin - PAS_DUREE_MIN : 0UL);
      break;
    case MENU_TEMPS_ARRET_AUTO:
      Config.Temps_Arret_Auto = (sens > 0) ? (Config.Temps_Arret_Auto + PAS_TEMPS_ARRET_AUTO)
                               : (Config.Temps_Arret_Auto > PAS_TEMPS_ARRET_AUTO ? Config.Temps_Arret_Auto - PAS_TEMPS_ARRET_AUTO : 0UL);
      break;
    case MENU_PRESS_H2_MIN:
      Config.Press_H2_Min += sens * PAS_PRESSION;
      break;
    case MENU_SEUIL_MQ8:
      Config.Seuil_MQ8 += sens * PAS_SEUIL_MQ;
      break;
    case MENU_SEUIL_MQ6:
      Config.Seuil_MQ6 += sens * PAS_SEUIL_MQ;
      break;
    default:
      break;   // MENU_REARMEMENT : rien à incrémenter
  }
  bornerConfig();
}

/* Construit les 4 lignes de l'afficheur 20x4 selon l'état courant.
 * NOTE : l'alignement exact des colonnes n'a pas pu être vérifié sur un
 * écran physique (voir docs/FSM_SECHOIR.md, section validation matérielle) —
 * seule la longueur (<= 20 caractères par ligne) a été contrôlée en test. */
static void composerLCD(char lignes[4][LCD_COLONNES + 1]) {
  char a[10], b[10];
  for (uint8_t i = 0; i < 4; i++) lignes[i][0] = '\0';
  const uint8_t T = LCD_COLONNES + 1;

  /* --- Menu de configuration / urgence ATEX (mêmes champs, réarmement en plus) --- */
  if (etat_courant == ETAT_CONFIG_MENU || etat_courant == ETAT_URGENCE_ATEX) {
    snprintf(lignes[0], T, "%s", etat_courant == ETAT_URGENCE_ATEX ? "!! URGENCE ATEX !!" : "-- PARAMETRES --");
    switch (Menu_Index) {
      case MENU_MODE_AUTO:
        snprintf(lignes[1], T, "Mode de marche");
        snprintf(lignes[2], T, "%s", Config.Mode_Auto ? "AUTOMATIQUE" : "MANUEL");
        break;
      case MENU_CHOIX_MODE:
        snprintf(lignes[1], T, "Source (mode manuel)");
        snprintf(lignes[2], T, "%s", Config.Choix_Mode == 1 ? "SOLAIRE" : (Config.Choix_Mode == 2 ? "H2" : "GPL"));
        break;
      case MENU_T_CIBLE:
        fmt1(a, sizeof(a), Config.T_cible);
        snprintf(lignes[1], T, "Temperature cible");
        snprintf(lignes[2], T, "%s C", a);
        break;
      case MENU_T_INIT:
        fmt1(a, sizeof(a), Config.T_init);
        snprintf(lignes[1], T, "Temp init (manuel)");
        snprintf(lignes[2], T, "%s C", a);
        break;
      case MENU_H_INITIAL:
        fmt1(a, sizeof(a), Config.H_initial);
        snprintf(lignes[1], T, "Humidite initiale");
        snprintf(lignes[2], T, "%s %%", a);
        break;
      case MENU_H_PRODUIT:
        fmt1(a, sizeof(a), Config.H_produit_cible);
        snprintf(lignes[1], T, "Humidite visee");
        snprintf(lignes[2], T, "%s %%", a);
        break;
      case MENU_DUREE_MAX:
        snprintf(lignes[1], T, "Duree max cycle");
        snprintf(lignes[2], T, "%lu min", (unsigned long)Config.Duree_Max_Cycle);
        break;
      case MENU_TEMPS_PROLONGATION:
        snprintf(lignes[1], T, "Plafond prolong.");
        snprintf(lignes[2], T, "%lu min", (unsigned long)Config.Temps_Prolongation);
        break;
      case MENU_HHYST:
        fmt1(a, sizeof(a), Config.Hhyst);
        snprintf(lignes[1], T, "Hysteresis (bande)");
        snprintf(lignes[2], T, "%s C", a);
        break;
      case MENU_TEMPS_PURGE:
        snprintf(lignes[1], T, "Duree de purge");
        snprintf(lignes[2], T, "%lu s", (unsigned long)Config.Temps_Purge);
        break;
      case MENU_TEMPS_ALLUMAGE:
        snprintf(lignes[1], T, "Delai max allumage");
        snprintf(lignes[2], T, "%lu s", (unsigned long)Config.Temps_Allumage);
        break;
      case MENU_TEMPS_MIN_FIN:
        snprintf(lignes[1], T, "Duree min avant fin");
        snprintf(lignes[2], T, "%lu min", (unsigned long)Config.Temps_Min_Fin);
        break;
      case MENU_TEMPS_ARRET_AUTO:
        snprintf(lignes[1], T, "Delai arret auto");
        snprintf(lignes[2], T, "%lu s", (unsigned long)Config.Temps_Arret_Auto);
        break;
      case MENU_PRESS_H2_MIN:
        fmt1(a, sizeof(a), Config.Press_H2_Min);
        snprintf(lignes[1], T, "Pression H2 min");
        snprintf(lignes[2], T, "%s bar", a);
        break;
      case MENU_SEUIL_MQ8:
        snprintf(lignes[1], T, "Seuil fuite H2");
        snprintf(lignes[2], T, "%d / 1023", Config.Seuil_MQ8);
        break;
      case MENU_SEUIL_MQ6:
        snprintf(lignes[1], T, "Seuil fuite GPL");
        snprintf(lignes[2], T, "%d / 1023", Config.Seuil_MQ6);
        break;
      case MENU_REARMEMENT:
        snprintf(lignes[1], T, "Rearmement ATEX");
        snprintf(lignes[2], T, "OK pour rearmer");
        break;
      default:
        break;
    }
    snprintf(lignes[3], T, "MENU +/- OK");
    return;
  }

  /* --- Échec de basculement / échecs d'allumage répétés (§13) --- */
  if (etat_courant == ETAT_ERREUR_COMBUSTION) {
    if (raison_erreur_combustion == ERR_BASCULEMENT) {
      snprintf(lignes[0], T, "BASCULEMENT ECHEC");
      snprintf(lignes[1], T, "-> %s impossible", nomSource(Source_Active));
    } else {
      snprintf(lignes[0], T, "ALLUMAGE ECHEC");
      snprintf(lignes[1], T, "Echecs: %u", nb_echecs_allumage);
    }
    snprintf(lignes[2], T, "%sRES %sMAN %sAUTO",
             Choix_Erreur == CHOIX_ERR_REESSAYER   ? ">" : " ",
             Choix_Erreur == CHOIX_ERR_MANUEL      ? ">" : " ",
             Choix_Erreur == CHOIX_ERR_AUTOMATIQUE ? ">" : " ");
    snprintf(lignes[3], T, "MENU:choix OK:val.");
    return;
  }

  /* --- Demande d'arrêt (§6 v2) --- */
  if (etat_courant == ETAT_FIN_TEMPORISATION) {
    uint32_t ecoule = t_boucle - chrono_arret_auto;
    uint32_t total_ms = Config.Temps_Arret_Auto * 1000UL;
    uint32_t restant_s = (ecoule < total_ms) ? (total_ms - ecoule) / 1000UL : 0UL;
    snprintf(lignes[0], T, "CONSIGNE ATTEINTE");
    snprintf(lignes[1], T, "ARRET DU SECHAGE ?");
    snprintf(lignes[2], T, "AUTO DANS %lu s", (unsigned long)restant_s);
    snprintf(lignes[3], T, "OK:stop MENU:report");
    return;
  }

  /* --- États idle simples --- */
  if (etat_courant == ETAT_ATTENTE_DEMARRAGE) {
    fmt1(a, sizeof(a), T_sec);
    fmt1(b, sizeof(b), Config.T_cible);
    snprintf(lignes[0], T, "SECHOIR HYBRIDE");
    snprintf(lignes[1], T, "Pret. START:demarrer");
    snprintf(lignes[2], T, "T=%sC Obj=%sC", a, b);
    snprintf(lignes[3], T, "%s MENU:regl.", Config.Mode_Auto ? "AUTO" : "MANUEL");
    return;
  }
  if (etat_courant == ETAT_SECHAGE_TERMINE) {
    fmt1(a, sizeof(a), H_sec);
    snprintf(lignes[0], T, "%s", arret_force_duree ? "ARRET FORCE (duree)" : "SECHAGE TERMINE");
    snprintf(lignes[1], T, "%s", arret_force_duree ? "Humidite non garantie" : "Cycle complet");
    snprintf(lignes[2], T, "Hsec fin %s%%", a);
    snprintf(lignes[3], T, "START:relancer");
    return;
  }

  /* --- Télémesure paginée pendant la régulation (Btn_Menu = page suivante) --- */
  if (page_affichage == 0) {
    fmt1(a, sizeof(a), T_sec); fmt1(b, sizeof(b), Config.T_cible);
    snprintf(lignes[0], T, "%s %s Fl:%d", nomSource(Source_Active), nomPalierOuPhase(), Flame ? 1 : 0);
    snprintf(lignes[1], T, "T%sC>%sC", a, b);
    fmt1(a, sizeof(a), H_sec); fmt1(b, sizeof(b), H_fin);
    snprintf(lignes[2], T, "Hs%s%% Hf%s%%", a, b);
    fmt1(a, sizeof(a), Press_H2);
    snprintf(lignes[3], T, "EV%d%d%d H2:%sb", EV1 ? 1 : 0, EV2 ? 1 : 0, EV3 ? 1 : 0, a);
  } else {
    fmt1(a, sizeof(a), T_amb); fmt1(b, sizeof(b), H_amb);
    snprintf(lignes[0], T, "Am%sC Ha%s%%", a, b);
    fmt1(a, sizeof(a), Config.H_initial); fmt1(b, sizeof(b), Config.H_produit_cible);
    snprintf(lignes[1], T, "Hi%s%% Ho%s%%", a, b);
    snprintf(lignes[2], T, "%s t=%lum", Config.Mode_Auto ? "AUTO" : "MANUEL",
             (unsigned long)((t_boucle - chrono_cycle) / 60000UL));
    snprintf(lignes[3], T, "Max%lum Pro%lum",
             (unsigned long)Config.Duree_Max_Cycle, (unsigned long)Config.Temps_Prolongation);
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
        /* Chemin 2 de réarmement : la demande est latchée ici et arbitrée
         * par pasFSM(), qui seul décide si la cause a disparu. */
        demande_rearm_ihm = true;
      } else if (etat_courant == ETAT_CONFIG_MENU) {
        quitter_menu = true;
      }
    }
  }

  /* --- Afficheur --- */
  if ((t_boucle - t_dernier_lcd) >= PERIODE_LCD) {
    t_dernier_lcd = t_boucle;
    char lignes[4][LCD_COLONNES + 1];
    composerLCD(lignes);
    lcd.clear();
    for (uint8_t i = 0; i < 4; i++) {
      lcd.setCursor(0, i);
      lcd.print(lignes[i]);
    }
  }
}
