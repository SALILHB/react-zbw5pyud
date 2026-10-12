/* ============================================================================
 *  LIAISON INTERFACE OPÉRATEUR — mise en œuvre
 *
 *  Incluse à la FIN de sechoir_hybride.ino (même unité de compilation) : elle
 *  utilise directement front[], demande_rearm_ihm, Config, bornerConfig(),
 *  calculerSeuils(), sauverConfig(), causeUrgencePresente()...
 *
 *  Règles (docs/PROTOCOLE.md) :
 *   - pasFSM() n'est pas modifiée. START / STOP / OK sont des fronts de
 *     boutons virtuels, consommés par frontPris() comme les vrais boutons ;
 *     le réarmement passe par demande_rearm_ihm (chemin 2 existant).
 *   - L'accusé de réception est calculé APRÈS pasFSM() : une commande dont
 *     le front n'a pas été consommé n'a eu aucun effet -> ok:false.
 *   - Aucune sortie n'est jamais forcée ; l'arrêt d'urgence D42 est lu par
 *     lireEntrees() comme avant et aucune commande ne peut l'acquitter.
 *   - Rien de bloquant : lecture caractère par caractère (lignes de 63
 *     caractères au plus), émission par morceaux selon la place libre des
 *     tampons d'émission (availableForWrite()).
 * ==========================================================================*/
#pragma once

static const char LIAISON_VERSION[] = "sechoir-if 1";

/* ---------------------------------------------------------------------------
 *  TABLE DES 31 PARAMÈTRES (ordre de enum ChampMenu, sans MENU_REARMEMENT).
 *  Bornes et pas : constantes existantes du programme. En mémoire flash.
 * -------------------------------------------------------------------------*/
enum TypeChamp : uint8_t { TC_DECIMAL = 'n', TC_ENTIER = 'e', TC_BOOLEEN = 'b', TC_LISTE = 'l' };
enum GroupeChamp : uint8_t { G_CYCLE = 0, G_REGULATION, G_SOLAIRE, G_SECURITE, G_DEGRADE, NB_GROUPES };

struct ChampMeta {
  char    cle[16];
  char    libelle[32];
  char    unite[6];
  char    options[24];     // valeurs nommées (booléen / liste), séparées par |
  uint8_t type;
  uint8_t groupe;
  uint8_t securite;        // 1 = paramètre de sécurité combustion : confirmation
  float   vmin, vmax, pas;
};

static const uint8_t NB_CHAMPS_IF = MENU_REARMEMENT;   // 31

static const ChampMeta META_CHAMPS[NB_CHAMPS_IF] PROGMEM = {
  {"mode_auto",      "Mode de marche",           "",    "MANUEL|AUTO",        TC_BOOLEEN, G_CYCLE,      0, 0, 1, 1},
  {"choix_mode",     "Source (mode manuel)",     "",    "SOLAIRE|H2|GPL",     TC_LISTE,   G_CYCLE,      0, 1, 3, 1},
  {"t_cible",        "Température cible",        "°C",  "",                   TC_DECIMAL, G_CYCLE,      0, T_CIBLE_MIN, T_CIBLE_MAX, PAS_TEMPERATURE},
  {"t_init",         "Température init. (manuel)", "°C", "",                  TC_DECIMAL, G_CYCLE,      0, T_INIT_MIN, T_INIT_MAX, PAS_TEMPERATURE},
  {"h_initial",      "Humidité initiale",        "%",   "",                   TC_DECIMAL, G_CYCLE,      0, H_INITIAL_MIN, H_INITIAL_MAX, PAS_HUMIDITE},
  {"h_produit",      "Humidité visée",           "%",   "",                   TC_DECIMAL, G_CYCLE,      0, H_PRODUIT_MIN, H_PRODUIT_MAX, PAS_HUMIDITE},
  {"duree_max",      "Durée max de séchage",     "min", "",                   TC_ENTIER,  G_CYCLE,      0, DUREE_MAX_CYCLE_MIN, DUREE_MAX_CYCLE_MAX, PAS_DUREE_MIN},
  {"prolong_defaut", "Prolongation proposée",    "min", "",                   TC_ENTIER,  G_CYCLE,      0, PROLONG_MIN, PROLONG_MAX, PAS_PROLONG},
  {"temps_reponse",  "Délai réponse opérateur",  "s",   "",                   TC_ENTIER,  G_CYCLE,      0, TEMPS_REPONSE_MIN, TEMPS_REPONSE_MAX, PAS_TEMPS_REPONSE},
  {"temps_min_fin",  "Durée min avant fin",      "min", "",                   TC_ENTIER,  G_CYCLE,      0, TEMPS_MIN_FIN_MIN, TEMPS_MIN_FIN_MAX, PAS_DUREE_MIN},
  {"hhyst",          "Hystérésis des paliers",   "°C",  "",                   TC_DECIMAL, G_REGULATION, 0, HHYST_MIN, HHYST_MAX, PAS_HHYST},
  {"periode_regul",  "Période de régulation",    "s",   "",                   TC_ENTIER,  G_REGULATION, 0, PERIODE_REGUL_MIN, PERIODE_REGUL_MAX, PAS_PERIODE_REGUL},
  {"regul_auto",     "Régulation des paliers",   "",    "PALIER IMPOSE|AUTO", TC_BOOLEEN, G_REGULATION, 0, 0, 1, 1},
  {"palier_impose",  "Palier imposé",            "",    "100%|67%|33%",       TC_LISTE,   G_REGULATION, 0, PALIER_100, PALIER_33, 1},
  {"delta_t_sol",    "Tcap = Tamb + DeltaT",     "°C",  "",                   TC_DECIMAL, G_SOLAIRE,    0, DELTA_T_SOL_MIN, DELTA_T_SOL_MAX, PAS_TEMPERATURE},
  {"marge_sol",      "Solaire ON = Tc + marge",  "°C",  "",                   TC_DECIMAL, G_SOLAIRE,    0, MARGE_SOL_MIN, MARGE_SOL_MAX, PAS_TEMPERATURE},
  {"hyst_sol",       "Solaire OFF = ON - hyst",  "°C",  "",                   TC_DECIMAL, G_SOLAIRE,    0, HYST_SOL_MIN, HYST_SOL_MAX, PAS_TEMPERATURE},
  {"seuil_chaud",    "Repère CHAUD (Tsec >=)",   "°C",  "",                   TC_DECIMAL, G_SOLAIRE,    0, SEUIL_CHAUD_MIN, SEUIL_CHAUD_MAX, PAS_TEMPERATURE},
  {"temps_purge",    "Durée de purge",           "s",   "",                   TC_ENTIER,  G_SECURITE,   1, TEMPS_PURGE_MIN, TEMPS_PURGE_MAX, PAS_TEMPS_SEC},
  {"temps_allumage", "Délai max d'allumage",     "s",   "",                   TC_ENTIER,  G_SECURITE,   1, TEMPS_ALLUMAGE_MIN, TEMPS_ALLUMAGE_MAX, PAS_TEMPS_ALLUMAGE},
  {"press_h2_min",   "Pression H2 minimale",     "bar", "",                   TC_DECIMAL, G_SECURITE,   1, PRESS_H2_MIN_PLANCHER, PRESS_H2_MIN_PLAFOND, PAS_PRESSION},
  {"seuil_mq8",      "Seuil de fuite H2 (MQ-8)", "/1023", "",                 TC_ENTIER,  G_SECURITE,   1, SEUIL_MQ_MIN, SEUIL_MQ_MAX, PAS_SEUIL_MQ},
  {"seuil_mq6",      "Seuil de fuite GPL (MQ-6)", "/1023", "",                TC_ENTIER,  G_SECURITE,   1, SEUIL_MQ_MIN, SEUIL_MQ_MAX, PAS_SEUIL_MQ},
  {"t_amb_fixe",     "T ambiante",               "",    "AUTO|FIXE",          TC_BOOLEEN, G_DEGRADE,    0, 0, 1, 1},
  {"t_amb_val",      "T ambiante fixe",          "°C",  "",                   TC_DECIMAL, G_DEGRADE,    0, T_AMB_FIXE_MIN, T_AMB_FIXE_MAX, PAS_TEMPERATURE},
  {"h_amb_fixe",     "H ambiante",               "",    "AUTO|FIXE",          TC_BOOLEEN, G_DEGRADE,    0, 0, 1, 1},
  {"h_amb_val",      "H ambiante fixe",          "%",   "",                   TC_DECIMAL, G_DEGRADE,    0, HUM_FIXE_MIN, HUM_FIXE_MAX, PAS_HUMIDITE},
  {"h_sec_fixe",     "H séchoir",                "",    "AUTO|FIXE",          TC_BOOLEEN, G_DEGRADE,    0, 0, 1, 1},
  {"h_sec_val",      "H séchoir fixe",           "%",   "",                   TC_DECIMAL, G_DEGRADE,    0, HUM_FIXE_MIN, HUM_FIXE_MAX, PAS_HUMIDITE},
  {"press_fixe",     "Pression H2",              "",    "AUTO|FIXE",          TC_BOOLEEN, G_DEGRADE,    0, 0, 1, 1},
  {"press_val",      "Pression H2 fixe",         "bar", "",                   TC_DECIMAL, G_DEGRADE,    0, PRESS_FIXE_MIN, PRESS_FIXE_MAX, PAS_PRESSION},
};

static const char NOM_GROUPE_0[] PROGMEM = "Cycle";
static const char NOM_GROUPE_1[] PROGMEM = "Régulation";
static const char NOM_GROUPE_2[] PROGMEM = "Solaire";
static const char NOM_GROUPE_3[] PROGMEM = "Sécurité combustion";
static const char NOM_GROUPE_4[] PROGMEM = "Mode dégradé";
static const char* const NOMS_GROUPES[NB_GROUPES] PROGMEM = {
  NOM_GROUPE_0, NOM_GROUPE_1, NOM_GROUPE_2, NOM_GROUPE_3, NOM_GROUPE_4
};

static void lireMeta(uint8_t i, ChampMeta& m) { memcpy_P(&m, &META_CHAMPS[i], sizeof(ChampMeta)); }

static int8_t chercherChamp(const char* cle) {
  for (uint8_t i = 0; i < NB_CHAMPS_IF; i++) {
    if (strcmp_P(cle, META_CHAMPS[i].cle) == 0) return (int8_t)i;
  }
  return -1;
}

/* Valeur courante d'un champ de Config, en flottant. */
static float lireChamp(uint8_t i) {
  switch (i) {
    case MENU_MODE_AUTO:      return Config.Mode_Auto ? 1 : 0;
    case MENU_CHOIX_MODE:     return Config.Choix_Mode;
    case MENU_T_CIBLE:        return Config.T_cible;
    case MENU_T_INIT:         return Config.T_init;
    case MENU_H_INITIAL:      return Config.H_initial;
    case MENU_H_PRODUIT:      return Config.H_produit_cible;
    case MENU_DUREE_MAX:      return (float)Config.Duree_Max_Cycle;
    case MENU_PROLONG_DEFAUT: return (float)Config.Prolong_Defaut;
    case MENU_TEMPS_REPONSE:  return (float)Config.Temps_Reponse;
    case MENU_TEMPS_MIN_FIN:  return (float)Config.Temps_Min_Fin;
    case MENU_HHYST:          return Config.Hhyst;
    case MENU_PERIODE_REGUL:  return (float)Config.Periode_Regul;
    case MENU_REGUL_AUTO:     return Config.Regul_Auto ? 1 : 0;
    case MENU_PALIER_IMPOSE:  return Config.Palier_Impose;
    case MENU_DELTA_T_SOL:    return Config.DeltaT_Sol;
    case MENU_MARGE_SOL:      return Config.Marge_Sol;
    case MENU_HYST_SOL:       return Config.Hyst_Sol;
    case MENU_SEUIL_CHAUD:    return Config.Seuil_Chaud;
    case MENU_TEMPS_PURGE:    return (float)Config.Temps_Purge;
    case MENU_TEMPS_ALLUMAGE: return (float)Config.Temps_Allumage;
    case MENU_PRESS_H2_MIN:   return Config.Press_H2_Min;
    case MENU_SEUIL_MQ8:      return (float)Config.Seuil_MQ8;
    case MENU_SEUIL_MQ6:      return (float)Config.Seuil_MQ6;
    case MENU_T_AMB_MODE:     return Config.Fixe_T_amb ? 1 : 0;
    case MENU_T_AMB_VAL:      return Config.Val_T_amb;
    case MENU_H_AMB_MODE:     return Config.Fixe_H_amb ? 1 : 0;
    case MENU_H_AMB_VAL:      return Config.Val_H_amb;
    case MENU_H_SEC_MODE:     return Config.Fixe_H_sec ? 1 : 0;
    case MENU_H_SEC_VAL:      return Config.Val_H_sec;
    case MENU_PRESS_MODE:     return Config.Fixe_Press ? 1 : 0;
    case MENU_PRESS_VAL:      return Config.Val_Press;
    default:                  return 0;
  }
}

/* Conversions sûres avant bornage : jamais de valeur hors du type. */
static uint32_t versDuree(float v) {
  if (v <= 0.0f) return 0UL;
  if (v > 100000.0f) return 100000UL;
  return (uint32_t)(v + 0.5f);
}
static uint8_t versIndice(float v) {
  if (v < 0.0f || v > 250.0f) return 255;   // hors liste : bornerConfig() rétablit la valeur par défaut
  return (uint8_t)(v + 0.5f);
}
static int versEntier(float v) {
  if (v < -30000.0f) return -30000;
  if (v > 30000.0f) return 30000;
  return (int)(v + (v >= 0.0f ? 0.5f : -0.5f));
}

/* Écrit un champ ; l'appelant applique ENSUITE bornerConfig() puis
 * calculerSeuils(), exactement comme le menu du pupitre. */
static void ecrireChamp(uint8_t i, float v) {
  bool b = (v != 0.0f);
  switch (i) {
    case MENU_MODE_AUTO:      Config.Mode_Auto = b; break;
    case MENU_CHOIX_MODE:     Config.Choix_Mode = versIndice(v); break;
    case MENU_T_CIBLE:        Config.T_cible = v; break;
    case MENU_T_INIT:         Config.T_init = v; break;
    case MENU_H_INITIAL:      Config.H_initial = v; break;
    case MENU_H_PRODUIT:      Config.H_produit_cible = v; break;
    case MENU_DUREE_MAX:      Config.Duree_Max_Cycle = versDuree(v); break;
    case MENU_PROLONG_DEFAUT: Config.Prolong_Defaut = versDuree(v); break;
    case MENU_TEMPS_REPONSE:  Config.Temps_Reponse = versDuree(v); break;
    case MENU_TEMPS_MIN_FIN:  Config.Temps_Min_Fin = versDuree(v); break;
    case MENU_HHYST:          Config.Hhyst = v; break;
    case MENU_PERIODE_REGUL:  Config.Periode_Regul = versDuree(v); break;
    case MENU_REGUL_AUTO:     Config.Regul_Auto = b; break;
    case MENU_PALIER_IMPOSE:  Config.Palier_Impose = versIndice(v); break;
    case MENU_DELTA_T_SOL:    Config.DeltaT_Sol = v; break;
    case MENU_MARGE_SOL:      Config.Marge_Sol = v; break;
    case MENU_HYST_SOL:       Config.Hyst_Sol = v; break;
    case MENU_SEUIL_CHAUD:    Config.Seuil_Chaud = v; break;
    case MENU_TEMPS_PURGE:    Config.Temps_Purge = versDuree(v); break;
    case MENU_TEMPS_ALLUMAGE: Config.Temps_Allumage = versDuree(v); break;
    case MENU_PRESS_H2_MIN:   Config.Press_H2_Min = v; break;
    case MENU_SEUIL_MQ8:      Config.Seuil_MQ8 = versEntier(v); break;
    case MENU_SEUIL_MQ6:      Config.Seuil_MQ6 = versEntier(v); break;
    case MENU_T_AMB_MODE:     Config.Fixe_T_amb = b; break;
    case MENU_T_AMB_VAL:      Config.Val_T_amb = v; break;
    case MENU_H_AMB_MODE:     Config.Fixe_H_amb = b; break;
    case MENU_H_AMB_VAL:      Config.Val_H_amb = v; break;
    case MENU_H_SEC_MODE:     Config.Fixe_H_sec = b; break;
    case MENU_H_SEC_VAL:      Config.Val_H_sec = v; break;
    case MENU_PRESS_MODE:     Config.Fixe_Press = b; break;
    case MENU_PRESS_VAL:      Config.Val_Press = v; break;
    default: break;
  }
}

/* Valeur d'un champ en texte : au dixième pour les décimaux, entière sinon. */
static void formaterChamp(char* buf, uint8_t taille, uint8_t type, float v) {
  if (type == TC_DECIMAL) fmt1(buf, taille, v);
  else snprintf_P(buf, taille, PSTR("%ld"), (long)versEntier(v));
}

/* ---------------------------------------------------------------------------
 *  RÉCEPTION DES LIGNES (USB = Serial, ESP32 = Serial1)
 * -------------------------------------------------------------------------*/
static const uint8_t TAILLE_LIGNE_RX = 64;     // 63 caractères utiles

struct RecepteurLigne {
  char    tampon[TAILLE_LIGNE_RX];
  uint8_t n;
  bool    trop_longue;   // caractères jetés jusqu'à la fin de ligne
  bool    prete;         // ligne complète en attente de traitement
};
static RecepteurLigne rx_usb, rx_esp;

/* ---------------------------------------------------------------------------
 *  ACCUSÉS DE RÉCEPTION (file de 4)
 * -------------------------------------------------------------------------*/
struct AccuseReception {
  char cmd[10];
  char val[14];
  char raison[44];
  bool ok;
};
static const uint8_t NB_ACKS = 4;
static AccuseReception acks[NB_ACKS];
static uint8_t ack_debut = 0, ack_nb = 0;

static void ajouterAck(const char* cmd, bool ok, const char* val, const char* raison) {
  if (ack_nb >= NB_ACKS) return;                    // file pleine : très improbable
  AccuseReception& a = acks[(uint8_t)((ack_debut + ack_nb) % NB_ACKS)];
  strncpy(a.cmd, cmd, sizeof(a.cmd) - 1);       a.cmd[sizeof(a.cmd) - 1] = '\0';
  strncpy(a.val, val, sizeof(a.val) - 1);       a.val[sizeof(a.val) - 1] = '\0';
  strncpy(a.raison, raison, sizeof(a.raison) - 1); a.raison[sizeof(a.raison) - 1] = '\0';
  a.ok = ok;
  ack_nb++;
}

/* Accusé dont la raison est une chaîne en mémoire flash. */
static void ajouterAckP(const char* cmd, bool ok, const char* val, const char* raison_P) {
  char r[44];
  strncpy_P(r, raison_P, sizeof(r) - 1);
  r[sizeof(r) - 1] = '\0';
  ajouterAck(cmd, ok, val, r);
}

/* ---------------------------------------------------------------------------
 *  COMMANDE EN COURS (résultat connu seulement après pasFSM())
 * -------------------------------------------------------------------------*/
enum TypeInjection : uint8_t { INJ_AUCUNE = 0, INJ_BOUTON, INJ_REARM };
static struct {
  uint8_t type;
  uint8_t bouton;
  char    cmd[10];
} cmd_en_cours = {INJ_AUCUNE, 0, ""};

/* Raison d'un refus de réarmement : la cause encore présente, lue comme le
 * fait causeUrgencePresente(). */
static void raisonCausePresente(char* r, uint8_t taille) {
  if (AU_Urgence) {
    strncpy_P(r, PSTR("arret d'urgence enfonce : le deverrouiller"), taille - 1);
    r[taille - 1] = '\0';
  } else if (MQ8_H2 > Config.Seuil_MQ8) {
    snprintf_P(r, taille, PSTR("fuite H2 : MQ-8 %d > %d"), MQ8_H2, Config.Seuil_MQ8);
  } else if (MQ6_But > Config.Seuil_MQ6) {
    snprintf_P(r, taille, PSTR("fuite GPL : MQ-6 %d > %d"), MQ6_But, Config.Seuil_MQ6);
  } else if (Flame) {
    strncpy_P(r, PSTR("flamme encore detectee"), taille - 1);
    r[taille - 1] = '\0';
  } else {
    strncpy_P(r, PSTR("refuse par la commande"), taille - 1);
    r[taille - 1] = '\0';
  }
}

static void refuserEtat(const char* cmd) {
  char r[44];
  snprintf_P(r, sizeof(r), PSTR("sans effet en %s"), nomEtat(etat_courant));
  ajouterAck(cmd, false, "", r);
}

/* Prépare un appui virtuel. Si le bouton réel vient d'être appuyé au même
 * instant (pupitre présent), la commande attend le tour suivant. */
static bool injecterBouton(const char* cmd, uint8_t bouton) {
  if (front[bouton]) return false;
  front[bouton] = true;
  cmd_en_cours.type = INJ_BOUTON;
  cmd_en_cours.bouton = bouton;
  strncpy(cmd_en_cours.cmd, cmd, sizeof(cmd_en_cours.cmd) - 1);
  cmd_en_cours.cmd[sizeof(cmd_en_cours.cmd) - 1] = '\0';
  return true;
}

/* États où le bouton START agit (pasFSM) */
static bool etatAccepteStart() {
  return etat_courant == ETAT_ATTENTE_DEMARRAGE || etat_courant == ETAT_CONFIG_MENU ||
         etat_courant == ETAT_SECHAGE_TERMINE;
}
/* États où le bouton STOP agit (pasFSM) */
static bool etatAccepteStop() {
  return cycleEnCours() || etat_courant == ETAT_ERREUR_COMBUSTION;
}
/* États où reglerTcibleEnCycle() est appelée (UP/DOWN = T_cible) */
static bool etatReglageTcible() {
  return etat_courant == ETAT_MODE_SOLAIRE || etat_courant == ETAT_MODE_H2 ||
         etat_courant == ETAT_MODE_GPL || etat_courant == ETAT_PROLONGATION;
}
/* Écriture libre des paramètres : hors cycle seulement */
static bool etatEcritureConfig() {
  return etat_courant == ETAT_ATTENTE_DEMARRAGE || etat_courant == ETAT_SECHAGE_TERMINE;
}

static bool lireNombre(const char* s, float& v) {
  if (s == NULL || *s == '\0') return false;
  char* fin = NULL;
  double d = strtod(s, &fin);
  if (fin == s || *fin != '\0') return false;
  if (d != d || d > 1.0e7 || d < -1.0e7) return false;
  v = (float)d;
  return true;
}

static void commandeSet(char* cle, char* valeur) {
  int8_t i = chercherChamp(cle ? cle : "");
  if (i < 0) { ajouterAckP("SET", false, "", PSTR("cle inconnue")); return; }
  float v;
  if (!lireNombre(valeur, v)) { ajouterAckP("SET", false, "", PSTR("valeur invalide")); return; }

  ChampMeta m;
  lireMeta((uint8_t)i, m);
  char txt[14];

#if AVEC_PUPITRE_LCD
  if (etat_courant == ETAT_CONFIG_MENU) {
    ajouterAckP("SET", false, "", PSTR("menu du pupitre ouvert"));
    return;
  }
#endif
  if (etatEcritureConfig()) {
    ecrireChamp((uint8_t)i, v);
    bornerConfig();
    calculerSeuils();
  } else if (i == MENU_T_CIBLE && etatReglageTcible()) {
    /* Mêmes instructions que reglerTcibleEnCycle() (UP/DOWN au pupitre). */
    Config.T_cible = v;
    bornerConfig();
    calculerSeuils();
    sauverConfig();
  } else if (cycleEnCours()) {
    ajouterAckP("SET", false, "",
                i == MENU_T_CIBLE ? PSTR("repondre d'abord a la demande")
                                  : PSTR("en cycle, seule t_cible se regle"));
    return;
  } else {
    refuserEtat("SET");
    return;
  }
  formaterChamp(txt, sizeof(txt), m.type, lireChamp((uint8_t)i));
  ajouterAck("SET", true, txt, m.cle);   // raison = clé : la page sait quel champ
}

static void majuscules(char* s) {
  for (; s && *s; s++) if (*s >= 'a' && *s <= 'z') *s = (char)(*s - 'a' + 'A');
}

static bool demande_cfg = false, demande_meta = false;

/* Traite une ligne reçue. Renvoie false si elle doit être retraitée au tour
 * suivant (bouton réel appuyé au même instant). */
static bool traiterLigne(char* ligne) {
  char* mots[3] = {NULL, NULL, NULL};
  uint8_t n = 0;
  char* p = ligne;
  while (*p && n < 3) {
    while (*p == ' ') p++;
    if (!*p) break;
    mots[n++] = p;
    while (*p && *p != ' ') p++;
    if (*p) *p++ = '\0';
  }
  if (n == 0) return true;
  char* cmd = mots[0];
  majuscules(cmd);

  if (strcmp_P(cmd, PSTR("PING")) == 0) {
    ajouterAck("PING", true, LIAISON_VERSION, "");
  } else if (strcmp_P(cmd, PSTR("GET")) == 0) {
    majuscules(mots[1]);
    if (mots[1] && strcmp_P(mots[1], PSTR("CFG")) == 0)       { demande_cfg = true;  ajouterAck("GET", true, "CFG", ""); }
    else if (mots[1] && strcmp_P(mots[1], PSTR("META")) == 0) { demande_meta = true; ajouterAck("GET", true, "META", ""); }
    else ajouterAckP("GET", false, "", PSTR("GET CFG ou GET META"));
  } else if (strcmp_P(cmd, PSTR("SET")) == 0) {
    commandeSet(mots[1], mots[2]);
  } else if (strcmp_P(cmd, PSTR("SAVE")) == 0) {
    if (etatEcritureConfig()) { sauverConfig(); ajouterAck("SAVE", true, "", ""); }
    else refuserEtat("SAVE");
  } else if (strcmp_P(cmd, PSTR("START")) == 0) {
    if (!etatAccepteStart()) refuserEtat("START");
    else return injecterBouton("START", B_START);
  } else if (strcmp_P(cmd, PSTR("STOP")) == 0) {
    if (!etatAccepteStop()) refuserEtat("STOP");
    else return injecterBouton("STOP", B_STOP);
  } else if (strcmp_P(cmd, PSTR("TERMINER")) == 0) {
    if (etat_courant != ETAT_DEMANDE_PROLONGATION) refuserEtat("TERMINER");
    else return injecterBouton("TERMINER", B_STOP);
  } else if (strcmp_P(cmd, PSTR("PROLONG")) == 0) {
    if (etat_courant != ETAT_DEMANDE_PROLONGATION) { refuserEtat("PROLONG"); return true; }
    majuscules(mots[1]);
    if (mots[1] && strcmp_P(mots[1], PSTR("OK")) == 0) return injecterBouton("PROLONG", B_OK);
    float v;
    if (!lireNombre(mots[1], v)) { ajouterAckP("PROLONG", false, "", PSTR("PROLONG <min> ou PROLONG OK")); return true; }
    /* Comme UP/DOWN en DEMANDE_PROLONGATION : N borné, délai de réponse relancé. */
    uint32_t nmin = versDuree(v);
    if (nmin < PROLONG_MIN) nmin = PROLONG_MIN;
    if (nmin > PROLONG_MAX) nmin = PROLONG_MAX;
    duree_prolongation_min = nmin;
    chrono_demande = t_boucle;
    char txt[14];
    snprintf_P(txt, sizeof(txt), PSTR("%lu"), (unsigned long)nmin);
    ajouterAck("PROLONG", true, txt, "");
  } else if (strcmp_P(cmd, PSTR("ERREUR")) == 0) {
    if (etat_courant != ETAT_ERREUR_COMBUSTION) { refuserEtat("ERREUR"); return true; }
    majuscules(mots[1]);
    uint8_t choix;
    if (mots[1] && strcmp_P(mots[1], PSTR("REESSAYER")) == 0)  choix = CHOIX_ERR_REESSAYER;
    else if (mots[1] && strcmp_P(mots[1], PSTR("MANUEL")) == 0) choix = CHOIX_ERR_MANUEL;
    else if (mots[1] && strcmp_P(mots[1], PSTR("AUTO")) == 0)   choix = CHOIX_ERR_AUTOMATIQUE;
    else { ajouterAckP("ERREUR", false, "", PSTR("REESSAYER, MANUEL ou AUTO")); return true; }
    if (front[B_OK]) return false;
    /* Même effet que MENU (choix) puis OK au pupitre. */
    Choix_Erreur = choix;
    return injecterBouton("ERREUR", B_OK);
  } else if (strcmp_P(cmd, PSTR("REARM")) == 0) {
    if (etat_courant != ETAT_URGENCE_ATEX) { refuserEtat("REARM"); return true; }
    /* Chemin 2 de réarmement : pasFSM() seul décide (causeUrgencePresente()). */
    demande_rearm_ihm = true;
    cmd_en_cours.type = INJ_REARM;
    strncpy(cmd_en_cours.cmd, "REARM", sizeof(cmd_en_cours.cmd));
  } else if (strcmp_P(cmd, PSTR("PILOTE")) == 0) {
    ajouterAckP("PILOTE", true, "", PSTR("gere par la passerelle ESP32"));
  } else {
    char c[10];
    strncpy(c, cmd, sizeof(c) - 1);
    c[sizeof(c) - 1] = '\0';
    ajouterAckP(c, false, "", PSTR("commande inconnue"));
  }
  return true;
}

/* Lit les caractères disponibles d'un port jusqu'à une ligne complète. */
static void lirePort(PortSerie& port, RecepteurLigne& r) {
  uint8_t lus = 0;
  while (!r.prete && lus < TAILLE_LIGNE_RX && port.available() > 0) {
    char c = (char)port.read();
    lus++;
    if (c == '\r') continue;
    if (c == '\n') {
      if (r.trop_longue) {
        r.trop_longue = false;
        r.n = 0;
        ajouterAckP("?", false, "", PSTR("ligne trop longue (63 car. max)"));
      } else if (r.n > 0) {
        r.tampon[r.n] = '\0';
        r.prete = true;
      }
    } else if (r.trop_longue) {
      /* on jette jusqu'à la fin de la ligne */
    } else if (r.n < TAILLE_LIGNE_RX - 1) {
      r.tampon[r.n++] = c;
    } else {
      r.trop_longue = true;
    }
  }
}

/* ---------------------------------------------------------------------------
 *  ÉMISSION NON BLOQUANTE
 *  Un message (journal, accusé, cfg, meta, télémétrie) est produit fragment
 *  par fragment ; chaque fragment est écrit selon la place libre des deux
 *  tampons d'émission. Un message n'est jamais interrompu par un autre.
 * -------------------------------------------------------------------------*/
enum TypeMessage : uint8_t { MSG_AUCUN = 0, MSG_JOURNAL, MSG_ACK, MSG_CFG, MSG_META, MSG_TEL };
static const uint32_t PERIODE_TELEMESURE = 1000UL;

static char     tx_frag[136];
static uint8_t  tx_len = 0, tx_pos = 0;
static uint8_t  tx_msg = MSG_AUCUN;
static uint8_t  tx_etape = 0;
static uint32_t t_derniere_tel = 0;
static bool     tel_jamais_emise = true;
static uint16_t tel_numero = 0;

static int secondesRestantes(uint32_t depuis, uint32_t duree_ms) {
  uint32_t ecoule = t_boucle - depuis;
  return (ecoule < duree_ms) ? (int)((duree_ms - ecoule) / 1000UL) : 0;
}

/* Fragment n° tx_etape de la télémesure. Renvoie true pour le dernier. */
static bool fragmentTelemesure() {
  char a[8], b[8], c[8], d[8], e[8];
  const uint8_t T = sizeof(tx_frag);
  switch (tx_etape) {
    case 0:
      snprintf_P(tx_frag, T, PSTR("{\"t\":\"tel\",\"n\":%u,\"ms\":%lu,\"lcd\":%d"),
                 tel_numero, (unsigned long)t_boucle, (int)AVEC_PUPITRE_LCD);
      return false;
    case 1:
      snprintf_P(tx_frag, T, PSTR(",\"etat\":\"%s\",\"e\":%u,\"src\":\"%s\",\"ph\":\"%s\""),
                 nomEtat(etat_courant), (unsigned)etat_courant, nomSource(Source_Active),
                 nomPalierOuPhase());
      return false;
    case 2:
      snprintf_P(tx_frag, T, PSTR(",\"pal\":%u,\"auto\":%d,\"reg\":%d,\"pimp\":%u,\"chaud\":%d"),
                 (unsigned)palier, Config.Mode_Auto ? 1 : 0, Config.Regul_Auto ? 1 : 0,
                 (unsigned)Config.Palier_Impose, regime_chaud ? 1 : 0);
      return false;
    case 3:
      fmt1(a, sizeof(a), T_sec); fmt1(b, sizeof(b), Config.T_cible);
      fmt1(c, sizeof(c), T1); fmt1(d, sizeof(d), T2); fmt1(e, sizeof(e), T3);
      snprintf_P(tx_frag, T, PSTR(",\"tsec\":%s,\"tc\":%s,\"t1\":%s,\"t2\":%s,\"t3\":%s"), a, b, c, d, e);
      return false;
    case 4:
      fmt1(a, sizeof(a), Config.Hhyst); fmt1(b, sizeof(b), H_sec);
      fmt1(c, sizeof(c), H_fin); fmt1(d, sizeof(d), H_amb);
      snprintf_P(tx_frag, T, PSTR(",\"hh\":%s,\"hsec\":%s,\"hfin\":%s,\"hamb\":%s"), a, b, c, d);
      return false;
    case 5:
      fmt1(a, sizeof(a), T_amb); fmt1(b, sizeof(b), T_cap); fmt1(c, sizeof(c), T_cap_mesure);
      fmt1(d, sizeof(d), seuilSolaireOn()); fmt1(e, sizeof(e), seuilSolaireOff());
      snprintf_P(tx_frag, T, PSTR(",\"tamb\":%s,\"tcap\":%s,\"tcapm\":%s,\"son\":%s,\"soff\":%s"),
                 a, b, c, d, e);
      return false;
    case 6:
      snprintf_P(tx_frag, T, PSTR(",\"fl\":%d,\"mq8\":%d,\"s8\":%d,\"mq6\":%d,\"s6\":%d"),
                 Flame ? 1 : 0, MQ8_H2, Config.Seuil_MQ8, MQ6_But, Config.Seuil_MQ6);
      return false;
    case 7:
      fmt1(a, sizeof(a), Press_H2); fmt1(b, sizeof(b), Config.Press_H2_Min);
      snprintf_P(tx_frag, T, PSTR(",\"ph2\":%s,\"ph2min\":%s,\"au\":%d,\"urg\":%u,\"urgp\":%d"),
                 a, b, AU_Urgence ? 1 : 0, (unsigned)cause_urgence, causeUrgencePresente() ? 1 : 0);
      return false;
    case 8:
      snprintf_P(tx_frag, T,
                 PSTR(",\"vh2\":%d,\"vbut\":%d,\"ev1\":%d,\"ev2\":%d,\"ev3\":%d,\"spk\":%d,\"buz\":%d"),
                 V_H2 ? 1 : 0, V_But ? 1 : 0, EV1 ? 1 : 0, EV2 ? 1 : 0, EV3 ? 1 : 0,
                 Spark ? 1 : 0, Buzzer ? 1 : 0);
      return false;
    case 9:
      snprintf_P(tx_frag, T, PSTR(",\"pwmp\":%u,\"pwmd\":%u,\"pwme\":%u,\"postp\":%d"),
                 (unsigned)PWM_Purge, (unsigned)PWM_Distrib, (unsigned)PWM_Extract,
                 post_purge_active ? 1 : 0);
      return false;
    case 10: {
      bool en_cycle = cycleEnCours() || etat_courant == ETAT_ERREUR_COMBUSTION;
      long tcy = en_cycle ? (long)((t_boucle - chrono_cycle) / 1000UL) : -1L;
      snprintf_P(tx_frag, T, PSTR(",\"tcy\":%ld,\"tmax\":%lu,\"tmf\":%lu"),
                 tcy, (unsigned long)(Config.Duree_Max_Cycle * 60UL),
                 (unsigned long)(Config.Temps_Min_Fin * 60UL));
      return false;
    }
    case 11: {
      int trep = (etat_courant == ETAT_DEMANDE_PROLONGATION)
                 ? secondesRestantes(chrono_demande, Config.Temps_Reponse * 1000UL) : -1;
      long tprol = (etat_courant == ETAT_PROLONGATION)
                   ? (long)((t_boucle - chrono_prolongation) / 1000UL) : -1L;
      snprintf_P(tx_frag, T, PSTR(",\"trep\":%d,\"trept\":%lu,\"nprol\":%lu,\"tprol\":%ld,\"motif\":%u"),
                 trep, (unsigned long)Config.Temps_Reponse, (unsigned long)duree_prolongation_min,
                 tprol, (unsigned)motif_demande);
      return false;
    }
    case 12: {
      int tpur = -1;
      if (Source_Active == SRC_SOLAIRE) {
        if (post_purge_active) tpur = secondesRestantes(chrono_post_purge, Config.Temps_Purge * 1000UL);
      } else if (phase_combustion == PH_PURGE && raison_purge != PURGE_PALIER_0 &&
                 (cycleEnCours() || etat_courant == ETAT_ERREUR_COMBUSTION)) {
        tpur = secondesRestantes(chrono_purge, Config.Temps_Purge * 1000UL);
      }
      int tref = (etat_courant == ETAT_SECHAGE_TERMINE)
                 ? secondesRestantes(chrono_refroidissement, DUREE_REFROIDISSEMENT) : -1;
      snprintf_P(tx_frag, T, PSTR(",\"choix\":%u,\"ech\":%u,\"pf\":%u,\"tpur\":%d,\"tref\":%d,\"jp\":%u"),
                 (unsigned)Choix_Erreur, (unsigned)nb_echecs_allumage, (unsigned)nb_pertes_flamme,
                 tpur, tref, (unsigned)jr_perdues);
      return false;
    }
    default:
      snprintf_P(tx_frag, T, PSTR(",\"fta\":%d,\"fha\":%d,\"fhs\":%d,\"fp\":%d}\n"),
                 Config.Fixe_T_amb ? 1 : 0, Config.Fixe_H_amb ? 1 : 0,
                 Config.Fixe_H_sec ? 1 : 0, Config.Fixe_Press ? 1 : 0);
      tel_numero++;
      return true;
  }
}

/* {"t":"cfg","mode_auto":1,...} : un champ par fragment. */
static bool fragmentConfig() {
  const uint8_t T = sizeof(tx_frag);
  if (tx_etape == 0) {
    strncpy_P(tx_frag, PSTR("{\"t\":\"cfg\""), T);
    return false;
  }
  uint8_t i = (uint8_t)(tx_etape - 1);
  ChampMeta m;
  lireMeta(i, m);
  char v[14];
  formaterChamp(v, sizeof(v), m.type, lireChamp(i));
  bool dernier = (i == NB_CHAMPS_IF - 1);
  snprintf_P(tx_frag, T, dernier ? PSTR(",\"%s\":%s}\n") : PSTR(",\"%s\":%s"), m.cle, v);
  return dernier;
}

/* {"t":"meta","champs":[{...},...]} : deux fragments par champ. */
static bool fragmentMeta() {
  const uint8_t T = sizeof(tx_frag);
  if (tx_etape == 0) {
    strncpy_P(tx_frag, PSTR("{\"t\":\"meta\",\"version\":\"sechoir-if 1\",\"champs\":["), T);
    return false;
  }
  uint8_t i = (uint8_t)((tx_etape - 1) / 2);
  ChampMeta m;
  lireMeta(i, m);
  if (((tx_etape - 1) % 2) == 0) {
    char g[24];
    strncpy_P(g, (const char*)pgm_read_word(&NOMS_GROUPES[m.groupe]), sizeof(g) - 1);
    g[sizeof(g) - 1] = '\0';
    snprintf_P(tx_frag, T, PSTR("%s{\"k\":\"%s\",\"l\":\"%s\",\"u\":\"%s\",\"ty\":\"%c\",\"g\":\"%s\",\"s\":%u"),
               i == 0 ? "" : ",", m.cle, m.libelle, m.unite, (char)m.type, g, (unsigned)m.securite);
    return false;
  }
  char vmin[10], vmax[10], pas[10];
  formaterChamp(vmin, sizeof(vmin), m.type, m.vmin);
  formaterChamp(vmax, sizeof(vmax), m.type, m.vmax);
  if (m.type == TC_DECIMAL) fmt1(pas, sizeof(pas), m.pas);
  else formaterChamp(pas, sizeof(pas), m.type, m.pas);
  bool dernier = (i == NB_CHAMPS_IF - 1);
  snprintf_P(tx_frag, T, dernier ? PSTR(",\"min\":%s,\"max\":%s,\"pas\":%s,\"o\":\"%s\"}]}\n")
                                 : PSTR(",\"min\":%s,\"max\":%s,\"pas\":%s,\"o\":\"%s\"}"),
             vmin, vmax, pas, m.options);
  return dernier;
}

static bool fragmentAck() {
  AccuseReception& a = acks[ack_debut];
  snprintf_P(tx_frag, sizeof(tx_frag),
             PSTR("{\"t\":\"ack\",\"cmd\":\"%s\",\"ok\":%s,\"val\":\"%s\",\"raison\":\"%s\"}\n"),
             a.cmd, a.ok ? "true" : "false", a.val, a.raison);
  ack_debut = (uint8_t)((ack_debut + 1) % NB_ACKS);
  ack_nb--;
  return true;
}

/* Une ligne du journal (texte brut, sans JSON). */
static bool fragmentJournal() {
  uint8_t n = 0;
  bool fin = false;
  while (jr_nb > 0 && n < sizeof(tx_frag) - 1) {
    char c = jr_fifo[jr_debut];
    jr_debut = (uint8_t)((jr_debut + 1) % TAILLE_JOURNAL);
    jr_nb--;
    tx_frag[n++] = c;
    if (c == '\n') { fin = true; break; }
  }
  tx_frag[n] = '\0';
  return fin || jr_nb == 0;
}

/* Prépare le fragment suivant ; false s'il n'y a rien à émettre. */
static bool fragmentSuivant() {
  tx_pos = 0;
  tx_len = 0;
  if (tx_msg == MSG_AUCUN) {
    if (jr_nb > 0)            tx_msg = MSG_JOURNAL;
    else if (ack_nb > 0)      tx_msg = MSG_ACK;
    else if (demande_cfg)   { tx_msg = MSG_CFG;  demande_cfg = false; }
    else if (demande_meta)  { tx_msg = MSG_META; demande_meta = false; }
    else if (tel_jamais_emise || (t_boucle - t_derniere_tel) >= PERIODE_TELEMESURE) {
      tx_msg = MSG_TEL;
      t_derniere_tel = t_boucle;
      tel_jamais_emise = false;
    } else {
      return false;
    }
    tx_etape = 0;
  }
  bool dernier;
  switch (tx_msg) {
    case MSG_JOURNAL: dernier = fragmentJournal();    break;
    case MSG_ACK:     dernier = fragmentAck();        break;
    case MSG_CFG:     dernier = fragmentConfig();     break;
    case MSG_META:    dernier = fragmentMeta();       break;
    default:          dernier = fragmentTelemesure(); break;
  }
  tx_len = (uint8_t)strlen(tx_frag);
  if (dernier) tx_msg = MSG_AUCUN;
  else tx_etape++;
  return true;
}

static void emettre() {
  for (;;) {
    if (tx_pos >= tx_len) {
      if (!fragmentSuivant()) return;
      if (tx_len == 0) continue;
    }
    int place = PORT_USB.availableForWrite();
    int place1 = Serial1.availableForWrite();
    if (place1 < place) place = place1;
    if (place <= 0) return;                      // tampons pleins : on reprendra au tour suivant
    uint8_t n = (uint8_t)(tx_len - tx_pos);
    if (place < n) n = (uint8_t)place;
    PORT_USB.write((const uint8_t*)tx_frag + tx_pos, n);
    Serial1.write((const uint8_t*)tx_frag + tx_pos, n);
    tx_pos = (uint8_t)(tx_pos + n);
  }
}

/* ---------------------------------------------------------------------------
 *  POINTS D'APPEL
 * -------------------------------------------------------------------------*/
void liaisonInit() {
  memset(&rx_usb, 0, sizeof(rx_usb));
  memset(&rx_esp, 0, sizeof(rx_esp));
  ack_debut = ack_nb = 0;
  cmd_en_cours.type = INJ_AUCUNE;
  tx_len = tx_pos = 0;
  tx_msg = MSG_AUCUN;
  tel_jamais_emise = true;
  demande_cfg = demande_meta = false;
}

void liaisonAvantFSM() {
  lirePort(PORT_USB, rx_usb);
  lirePort(Serial1, rx_esp);
  /* Une ligne au plus par tour : l'USB d'abord (poste de dépannage), puis l'ESP32. */
  RecepteurLigne* r = rx_usb.prete ? &rx_usb : (rx_esp.prete ? &rx_esp : NULL);
  if (r != NULL && traiterLigne(r->tampon)) {
    r->prete = false;
    r->n = 0;
  }
}

void liaisonApresFSM() {
  if (cmd_en_cours.type == INJ_BOUTON) {
    bool consomme = !front[cmd_en_cours.bouton];
    if (consomme) {
      ajouterAck(cmd_en_cours.cmd, true, nomEtat(etat_courant), "");
    } else {
      front[cmd_en_cours.bouton] = false;        // jamais transmis à gererIHM()
      if (etat_courant == ETAT_URGENCE_ATEX) {
        ajouterAckP(cmd_en_cours.cmd, false, "", PSTR("urgence prioritaire"));
      } else {
        refuserEtat(cmd_en_cours.cmd);
      }
    }
  } else if (cmd_en_cours.type == INJ_REARM) {
    if (etat_courant != ETAT_URGENCE_ATEX) {
      ajouterAck("REARM", true, nomEtat(etat_courant), "");
    } else {
      char r[44];
      raisonCausePresente(r, sizeof(r));
      ajouterAck("REARM", false, "", r);
    }
  }
  cmd_en_cours.type = INJ_AUCUNE;
  emettre();
}

/* Fin du .ino : `Serial` redevient le port réel pour la suite (bancs de test). */
#undef Serial
