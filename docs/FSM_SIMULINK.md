# Séchoir solaire hybride — Modèle Simulink/Stateflow de la commande (v3)

Spécification de la partie **FSM** du modèle `Commande_Sechoir_Hybride.slx`
(sous-système « FSM », chart Stateflow, données, transitions), construite par
`matlab/completer_fsm_sechoir.m`. Elle reproduit **exactement** le firmware
Arduino `sechoir_hybride/sechoir_hybride.ino` v3, qui est la source de vérité
(voir `docs/FSM_SECHOIR.md` §0 pour les décisions de la v3).

Les sous-systèmes `CONVERSION_PUISSANCE` et `MODELE_THERMIQUE` ne sont pas
modifiés (§9).

---

## 0. Corrections par rapport à la version précédente du script

La version précédente (commit `bacc6c0`) n'a jamais été exécutée ; une
relecture complète du `.slx` d'origine a montré qu'elle **n'aurait pas
fonctionné**. Ne l'utilisez pas. Erreurs corrigées :

| Erreur de la version précédente | Réalité du modèle / correction |
|---|---|
| `FONCTIONNEMENT_NORMAL` rendu parallèle (AND) | Il contient aussi `ATTENTE_DEMARRAGE` et `SECHAGE_TERMINE` : ils seraient devenus des régions parallèles. → nouvel état **`EN_CYCLE`** (AND) à l'intérieur de `FONCTIONNEMENT_NORMAL`, qui reste OR. |
| Actions écrites en syntaxe C (`if (x) { y }`) | Le chart est en **langage d'action MATLAB** (`actionLanguage = 2`). → toute la syntaxe est MATLAB (`if x, y; end`, `~=`, `~in(...)`). |
| Variables temporaires non déclarées (`ECART_MIN`, `delta`, `demi`) | Interdit en Stateflow. → expressions écrites en ligne. |
| « `URGENCE_ATEX` inatteignable » | **Faux** : la transition d'origine SSID 118 (`FONCTIONNEMENT_NORMAL` → `URGENCE_ATEX`, fuite ou AU) existe. Elle est conservée, pas dupliquée. |
| Renommage `AU_Manuel` → `AU_Urgence` | Aurait créé une entrée non câblée. Le port du modèle s'appelle **`AU_Manuel`** (= `AU_Urgence` du firmware) : conservé. |
| `after(Temps_Min_Fin*60, sec)` | `Temps_Min_Fin` vaut 7200 **secondes** dans le modèle : ×60 aurait donné 120 h. → toutes les durées du chart sont en **secondes**. |
| `Mode_Auto_Eff` recopié depuis `Mode_Auto` à chaque entrée dans `ATTENTE_DEMARRAGE` | Annulait le choix MANUEL fait en `ERREUR_COMBUSTION`. → recopie seulement quand l'**entrée** `Mode_Auto` change. |
| Sous-états placés hors de la boîte de leur parent | Stateflow déduit la hiérarchie de l'inclusion graphique. → mise en page complète (§1). |
| « La transition la plus interne est prioritaire » | **Faux** : Stateflow évalue d'abord les transitions sortantes du parent (les plus externes). Sans conséquence ici (même destination), mais la doc est corrigée (§5). |

---

## 1. Hiérarchie d'états et mise en page

```
FSM/Chart
├── FONCTIONNEMENT_NORMAL (OR)            [20 20 1040 920]
│   entry, during : acquisition (grandeurs FIXE/AUTO, T_cap_est, H_fin,
│                   repère FROID/CHAUD, synchro Mode_Auto)
│   ├── ATTENTE_DEMARRAGE                 [60 60 180 80]
│   ├── EN_CYCLE (AND — deux régions parallèles)   [40 170 1000 750]
│   │   entry : demarrerCycle()     during : t_cycle, seuils, Etat_LCD
│   │   ├── SOURCE (région 1)             [60 200 640 700]
│   │   │   ├── MODE_SOLAIRE              [80 240 200 90]
│   │   │   ├── ERREUR_COMBUSTION         [320 240 360 90]
│   │   │   ├── MODE_H2                   [80 360 600 250]
│   │   │   │   ├── PURGE → ALLUMAGE → REGULATION
│   │   │   └── MODE_GPL                  [80 630 600 250]
│   │   │       ├── PURGE → ALLUMAGE → REGULATION
│   │   └── PHASE (région 2)              [720 200 300 700]
│   │       ├── NORMAL                    [740 240 260 80]
│   │       ├── DEMANDE_PROLONGATION      [740 360 260 250]  (ex-FIN_TEMPORISATION)
│   │       └── PROLONGATION              [740 630 260 250]
│   └── SECHAGE_TERMINE                   [860 60 180 80]
└── URGENCE_ATEX                          [20 980 320 150]
```

Positions `[x y largeur hauteur]` en coordonnées du chart. **Repli manuel** si
le script affiche `[HIERARCHIE INCORRECTE]` : dans l'éditeur Stateflow, faites
glisser chaque état signalé dans sa boîte parente, réglez `EN_CYCLE` en
décomposition *AND (parallel)* (clic droit > Decomposition), puis relancez le
script.

Pourquoi deux régions parallèles : dans le firmware, `DEMANDE_PROLONGATION` et
`PROLONGATION` appellent `etapeRegulation(false)` — la source et la combustion
continuent de tourner pendant que l'écran pose la question. `SOURCE` modélise
« quelle énergie et quelle phase de combustion », `PHASE` modélise « où en est
le cycle » ; aucune des deux n'interrompt l'autre.

---

## 2. Dictionnaire de données

**Toutes les durées du chart sont en secondes.** Le menu du firmware affiche
certaines durées en minutes : multiplier par 60.

### 2.1 Entrées (`Input`)

| Nom | Origine | Correspond à (firmware) |
|---|---|---|
| `Btn_Start`, `Btn_OK`, `Btn_UP`, `Btn_DOWN` | modèle d'origine | boutons START, OK, UP, DOWN |
| `Btn_SELECT` | modèle d'origine | bouton MENU (défilement des choix d'erreur) |
| `Btn_Stop`, `Btn_Rearm` | **ajoutées** (2 ports à câbler) | boutons STOP et RÉARMEMENT |
| `Mode_Auto`, `Choix_Manuel` | modèle d'origine | `Config.Mode_Auto`, `Config.Choix_Mode` |
| `T_cible`, `H_produit` | modèle d'origine | `Config.T_cible`, `Config.H_produit_cible` |
| `Tps_Prolongation` | modèle d'origine | `Config.Prolong_Defaut` — **en secondes** (1800 = 30 min) |
| `T_sec`, `H_sec`, `T_amb`, `H_amb`, `Press_H2` | modèle d'origine | mesures |
| `MQ8_H2`, `MQ6_But`, `Flame`, `AU_Manuel` | modèle d'origine | sécurité (`AU_Manuel` = `AU_Urgence`) |
| `T_cap`, `Btn_Prolongation`, `H_initial` | modèle d'origine | **inutilisées** en v3 (`T_cap` remplacée par `T_cap_est`) |

### 2.2 Sorties (`Output`, modèle d'origine)

`V_H2`, `V_But`, `V_Fl_1..3` (= EV1..3), `Spark`, `PWM_Purge`, `PWM_Inj`
(= `PWM_Distrib`), `PWM_Ext` (= `PWM_Extract`), `Buzzer`, `Etat_LCD`
(0 attente, 2 solaire, 3 H2, 4 GPL, 5 demande, 6 prolongation, 7 terminé,
8 erreur, 9 urgence — mêmes codes que `enum EtatFSM`).

### 2.3 Paramètres (`Local`, valeur initiale réglable dans le Model Explorer)

| Nom | Défaut | Firmware |
|---|---|---|
| `T_init_manuel` | 25 °C | `Config.T_init` (mode manuel) |
| `Hhyst` *(origine)* | 5 °C | `Config.Hhyst` |
| `Temps_Min_Fin` *(origine)* | 7200 s | `Config.Temps_Min_Fin` (120 min) |
| `Duree_Max_Cycle` | 36000 s | `Config.Duree_Max_Cycle` (600 min) |
| `Temps_Reponse` | 300 s | `Config.Temps_Reponse` |
| `Temps_Purge`, `Temps_Allumage` | 120 s, 4 s | idem |
| `Periode_Regul` | 1 s | `Config.Periode_Regul` |
| `Regul_Auto`, `Palier_Impose` | 1, 1 (67 %) | idem |
| `DeltaT_Sol`, `Marge_Sol`, `Hyst_Sol` | 20, 0, 5 °C | idem |
| `Seuil_Chaud` | 40 °C | `Config.Seuil_Chaud` |
| `Press_H2_Min`, `Marge_Retour_H2` | 2 bar, 0,5 bar | idem |
| `Seuil_MQ8`, `Seuil_MQ6` | 350 | idem (utilisés par la transition d'origine 118, jusque-là non déclarés) |
| `Fixe_T_amb/H_amb/H_sec/Press`, `Val_…` | 0 (AUTO), 25/40/60/5 | grandeurs FIXE/AUTO |
| `MAX_ECHECS`, `MAX_PERTES_FLAMME` | 3, 1 | constantes firmware |
| `T_SEC_MAX_SECURITE`, `DELAI_FLAMME_PARASITE` | 90 °C, 5 s | constantes firmware |

### 2.4 Variables internes (`Local`)

| Nom | Rôle |
|---|---|
| `T_amb_e`, `H_amb_e`, `H_sec_e`, `Press_e` | grandeurs effectives (mesure, ou valeur fixe saisie) |
| `T_cap_est` | `T_amb_e + DeltaT_Sol` |
| `H_fin` *(origine)* | `min(H_produit + H_amb_e, 95)` |
| `T_init`, `T1_seuil`, `T2_seuil` *(origine)*, `T3_seuil` | seuils de palier |
| `regime_chaud` | repère FROID (0) / CHAUD (1) |
| `Mode_Auto_Eff`, `Mode_Auto_prev` | mode effectif (le choix fait en erreur prime jusqu'au prochain changement du sélecteur) |
| `palier` | 0 = 100 %, 1 = 67 %, 2 = 33 %, 3 = 0 % |
| `Source_Active` | 0 solaire, 1 H2, 2 GPL |
| `raison_purge` | 0 démarrage, 1 échec, 2 bascule, 3 palier 0 % |
| `nb_echecs_allumage`, `nb_pertes_flamme` | compteurs |
| `Choix_Erreur`, `Btn_SELECT_prev`, `Btn_UP_prev`, `Btn_DOWN_prev` | menus et fronts |
| `cause_urgence` | 1 fuite H2, 2 fuite GPL, 3 AU, 4 perte flamme, 5 flamme parasite |
| `post_purge` | post-purge après retour au solaire |
| `t_cycle`, `t_derniere_regul` | temps depuis Btn_Start ; dernière comparaison de régulation |
| `motif_demande`, `humidite_deja_atteinte`, `duree_prolongation` | fin de cycle |

---

## 3. Actions des états (langage d'action MATLAB)

Blocs réutilisés :

- **`fermer_gaz`** : `V_H2=uint8(0); V_But=uint8(0); V_Fl_1=uint8(0); V_Fl_2=uint8(0); V_Fl_3=uint8(0); Spark=uint8(0);`
- **`appliquer_palier`** : `V_Fl_1=uint8(palier==0); V_Fl_2=uint8(palier==0 || palier==1); V_Fl_3=uint8(palier~=3);`
- **`ventil_palier`** : `PWM_Purge=60` ; `PWM_Inj/PWM_Ext` = 255/200 (100 %), 210/165 (67 %), 165/130 (33 %).
- **`calculer_seuils`** : `T1 = T_init + (T_cible−T_init)/3`, `T2 = T_init + 2(T_cible−T_init)/3`, `T3 = T_cible` (tous = `T_cible` si `T_cible <= T_init + 3`).
- **`palier_initial`** : palier imposé si `Regul_Auto==0` ; sinon 100 % en FROID ; en CHAUD, palier selon `T_sec` (< T1 : 100 %, < T2 : 67 %, < T3 : 33 %, sinon 0 %) ; `raison_purge = 3` si 0 % (veille), sinon 0.

| État | Actions |
|---|---|
| `FONCTIONNEMENT_NORMAL` | `entry, during:` grandeurs effectives, `T_cap_est`, `H_fin`, repère FROID/CHAUD (bande ±`Hhyst`/2 autour de `Seuil_Chaud`), `if Mode_Auto ~= Mode_Auto_prev, Mode_Auto_Eff = Mode_Auto; …` |
| `ATTENTE_DEMARRAGE` | `entry:` fermer_gaz, PWM à 0, `Buzzer=0`, `Etat_LCD=0` |
| `EN_CYCLE` | `entry:` `T_init` (= `T_sec` en auto, `T_init_manuel` sinon), calculer_seuils, `regime_chaud = (T_sec >= Seuil_Chaud)`, compteurs à 0, `t_cycle=0`, palier_initial. `during:` `t_cycle = temporalCount(sec)`, calculer_seuils (T_cible modifiable en cycle), `Etat_LCD` selon les sous-états actifs |
| `MODE_SOLAIRE` | `entry, during:` fermer_gaz, `Source_Active=0` ; post-purge (255/60/200) pendant `Temps_Purge` après un retour depuis la combustion, sinon ventilation solaire (0/220/150) |
| `MODE_H2` / `MODE_GPL` | `entry:` `Source_Active = 1` / `2` |
| `PURGE` | `entry:` fermer_gaz, ventilation de purge 255/60/200 |
| `ALLUMAGE` | `entry:` vanne source, appliquer_palier, `Spark=1`, ventil_palier |
| `REGULATION` | `entry:` vanne source, `t_derniere_regul=0`. `during:` palier imposé, **ou** gerer_palier toutes les `Periode_Regul` s ; si `palier ~= 3` : appliquer_palier + ventil_palier. Le gaz n'est **jamais** fermé ici au 0 % (voir §4.3) |
| `ERREUR_COMBUSTION` | `entry:` fermer_gaz, `Buzzer=1`, ventilation 255/0/200, `Choix_Erreur=0`. `during:` défilement des 3 choix sur **front** de `Btn_SELECT` |
| `NORMAL`, `PROLONGATION` | aucune action |
| `DEMANDE_PROLONGATION` | `entry:` mémorisation de UP/DOWN (fronts). `during:` si l'humidité arrive pendant la question, `motif_demande = 0` |
| `SECHAGE_TERMINE` | `entry:` fermer_gaz, PWM 0/0/90, `Buzzer=0`, `Etat_LCD=7` |
| `URGENCE_ATEX` | `entry:` fermer_gaz, 255/0/255, `Buzzer=1`, `Etat_LCD=9`, `cause_urgence` (si pas déjà fixée par une transition flamme : AU, sinon fuite H2, sinon fuite GPL). `during:` fermer_gaz |

---

## 4. Transitions

`ERR` désigne `in(EN_CYCLE.SOURCE.ERREUR_COMBUSTION)`. L'ordre des lignes est
l'ordre de création, qui est aussi l'ordre d'évaluation entre transitions
issues du même état.

### 4.1 Sécurité et fin de cycle globale

| Source → Destination | Condition | Action |
|---|---|---|
| `FONCTIONNEMENT_NORMAL` → `URGENCE_ATEX` *(origine, SSID 118)* | `MQ8_H2 >= Seuil_MQ8 \|\| MQ6_But >= Seuil_MQ6 \|\| AU_Manuel == 1` | — |
| `FONCTIONNEMENT_NORMAL` → `URGENCE_ATEX` | `duration(Flame==1 && V_H2==0 && V_But==0) >= DELAI_FLAMME_PARASITE` | `cause_urgence=5` |
| `URGENCE_ATEX` → `ATTENTE_DEMARRAGE` | `(Btn_OK \|\| Btn_Rearm) && MQ8_H2 < Seuil_MQ8 && MQ6_But < Seuil_MQ6 && AU_Manuel==0 && Flame==0` | `Buzzer=0; palier=0; raison_purge=0; cause_urgence=0` |
| `EN_CYCLE` → `SECHAGE_TERMINE` | `Btn_Stop==1` | fermer_gaz |
| `EN_CYCLE` → `SECHAGE_TERMINE` | `T_sec >= T_SEC_MAX_SECURITE && ~ERR` | fermer_gaz |
| `SECHAGE_TERMINE` → `ATTENTE_DEMARRAGE` | `after(300, sec) \|\| Btn_Start==1` | — |

### 4.2 Démarrage et arbitrage de source (région SOURCE)

`SOL` = `(T_sec >= Seuil_Chaud && T_cap_est >= T_cible+Marge_Sol−Hyst_Sol) || T_cap_est >= T_cible+Marge_Sol`
(au démarrage : FROID → seuil ON seul, CHAUD → solaire accepté dès OFF).

| Source → Destination | Condition | Action |
|---|---|---|
| *(défaut)* → `MODE_SOLAIRE` | — | — (sécurité : jamais de gaz par défaut) |
| `ATTENTE` → `MODE_SOLAIRE` *(origine 119, manuel)* | `Btn_Start && Mode_Auto_Eff==0 && Choix_Manuel==1` | — (garde alignée sur `Mode_Auto_Eff` par le script) |
| `ATTENTE` → `MODE_H2` *(origine 120, manuel)* | `Btn_Start && Mode_Auto_Eff==0 && Choix_Manuel==2` | — (idem) |
| `ATTENTE` → `MODE_SOLAIRE` | `Btn_Start && Mode_Auto_Eff==1 && SOL` | — |
| `ATTENTE` → `MODE_H2` | `Btn_Start && Mode_Auto_Eff==1 && ~SOL && Press_e >= Press_H2_Min` | — |
| `ATTENTE` → `MODE_GPL` | `Btn_Start && Mode_Auto_Eff==1 && ~SOL && Press_e < Press_H2_Min` | — |
| `ATTENTE` → `MODE_GPL` | `Btn_Start && Mode_Auto_Eff==0 && Choix_Manuel==3` | — |
| `MODE_H2` → `MODE_SOLAIRE` | `Mode_Auto_Eff==1 && T_cap_est >= T_cible+Marge_Sol` | fermer_gaz ; `post_purge=1` |
| `MODE_GPL` → `MODE_SOLAIRE` | idem | idem |
| `MODE_SOLAIRE` → `MODE_H2` | `Mode_Auto_Eff==1 && ~MAINTIEN && Press_e >= Press_H2_Min` | `post_purge=0; nb_echecs=0;` palier_initial |
| `MODE_SOLAIRE` → `MODE_GPL` | idem avec `Press_e < Press_H2_Min` | idem |
| `MODE_H2` → `MODE_GPL` | `Mode_Auto_Eff==1 && Press_e < Press_H2_Min` | `raison_purge=2; nb_echecs=0` (palier **conservé**) |
| `MODE_GPL` → `MODE_H2` | `Mode_Auto_Eff==1 && Press_e >= Press_H2_Min + Marge_Retour_H2` | idem |

`MAINTIEN` = seuil pour garder le solaire : ON en FROID, OFF en CHAUD.

### 4.3 Combustion (identique sous `MODE_H2` et `MODE_GPL`)

| Source → Destination | Condition | Action |
|---|---|---|
| *(défaut)* → `PURGE` | — | — |
| `PURGE` → `ALLUMAGE` | `after(Temps_Purge,sec) && (raison_purge ~= 3 \|\| T_sec < T3_seuil − Hhyst/2)` | `if raison_purge==3, palier=2; end` |
| `ALLUMAGE` → `REGULATION` | `Flame==1` | `Spark=0; nb_echecs=0` |
| `ALLUMAGE` → `PURGE` | `after(Temps_Allumage,sec) && Flame==0 && nb_echecs+1 < MAX_ECHECS` | `nb_echecs++; raison_purge=1` |
| `ALLUMAGE` → `ERREUR_COMBUSTION` | `after(Temps_Allumage,sec) && Flame==0 && nb_echecs+1 >= MAX_ECHECS` | `nb_echecs++` |
| `REGULATION` → `PURGE` | `Flame==0 && nb_pertes_flamme < MAX_PERTES_FLAMME` | fermer_gaz ; `nb_pertes_flamme++; raison_purge=1` (relance) |
| `REGULATION` → `URGENCE_ATEX` | `Flame==0 && nb_pertes_flamme >= MAX_PERTES_FLAMME` | `cause_urgence=4` |
| `REGULATION` → `PURGE` | `palier==3` | `raison_purge=3` (coupure volontaire, veille) |

Le contrôle de flamme est créé **avant** la coupure au 0 % : c'est l'ordre du
firmware (flamme vérifiée d'abord). Comme `REGULATION` ne ferme jamais le gaz
lui-même, la flamme est encore présente quand `[palier==3]` est évaluée : une
coupure volontaire n'est jamais prise pour une panne.

### 4.4 `ERREUR_COMBUSTION`

| Destination | Condition | Action |
|---|---|---|
| `MODE_H2` | `Btn_OK && (Choix_Erreur==0 \|\| Choix_Erreur==2) && Source_Active==1` | `if Choix_Erreur==2, Mode_Auto_Eff=1; end; nb_echecs=0; raison_purge=1; Buzzer=0` |
| `MODE_GPL` | idem avec `Source_Active==2` | idem |
| `ATTENTE_DEMARRAGE` | `Btn_OK && Choix_Erreur==1` | `Mode_Auto_Eff=0; Buzzer=0` |
| `SECHAGE_TERMINE` | `Btn_Stop==1` | `Buzzer=0` |

### 4.5 Région PHASE (fin de cycle)

`HUM` = `t_cycle >= Temps_Min_Fin && H_sec_e <= H_fin`.

| Source → Destination | Condition | Action |
|---|---|---|
| *(défaut)* → `NORMAL` | — | — |
| `DEMANDE_PROLONGATION` → `NORMAL` | `ERR` | — (le firmware repasse par MODE_x après une erreur) |
| `PROLONGATION` → `NORMAL` | `ERR` | — |
| `NORMAL` → `DEMANDE_PROLONGATION` | `~ERR && HUM` | `motif=0; humidite_deja_atteinte=1; duree=Tps_Prolongation` |
| `NORMAL` → `DEMANDE_PROLONGATION` | `~ERR && t_cycle >= Duree_Max_Cycle` | `motif=1; duree=Tps_Prolongation` |
| `DEMANDE` → `PROLONGATION` | `Btn_OK==1` | — |
| `DEMANDE` → `SECHAGE_TERMINE` | `Btn_Stop==1` | fermer_gaz |
| `DEMANDE` → `SECHAGE_TERMINE` | `after(Temps_Reponse, sec)` | fermer_gaz (fin sans réponse, 0 %) |
| `DEMANDE` → `DEMANDE` | front de `Btn_UP` | `duree = min(duree+300, 43200)` (la boucle relance aussi le délai de réponse) |
| `DEMANDE` → `DEMANDE` | front de `Btn_DOWN` | `duree = max(duree−300, 300)` |
| `PROLONGATION` → `DEMANDE` | `humidite_deja_atteinte==0 && HUM` | `motif=0; humidite_deja_atteinte=1; duree=Tps_Prolongation` |
| `PROLONGATION` → `DEMANDE` | `after(duree_prolongation, sec)` | `motif=2; duree=Tps_Prolongation` |

---

## 5. Règles Stateflow à connaître (pour modifier le chart à la main)

- **Priorité** : à chaque pas, Stateflow évalue d'abord les transitions qui
  **sortent** de l'état parent, puis celles de ses enfants. L'urgence (bord de
  `FONCTIONNEMENT_NORMAL`) passe donc avant tout le reste, puis Stop et
  surchauffe (bord de `EN_CYCLE`), puis les transitions internes. Entre
  transitions issues du même état : ordre de création (ou `ExecutionOrder`).
- **Transitions qui ne doivent toucher qu'une région** (fin de cycle,
  prolongation) : toujours tracées **à l'intérieur** de `PHASE`, jamais depuis
  le bord de `EN_CYCLE`, qui ferait sortir puis rentrer les deux régions (la
  combustion repartirait de zéro).
- **`after(N, sec)`** mesure le temps depuis l'entrée dans l'état **source** de
  la transition. **`temporalCount(sec)`** donne ce temps dans une action
  d'état (utilisé pour `t_cycle`). **`duration(C)`** donne le temps depuis que
  la condition `C` est vraie sans interruption (flamme parasite).
- **`in(A.B)`** : nom qualifié, résolu en remontant la hiérarchie depuis le
  parent de la transition.
- **Une entrée (`Input`) ne peut jamais être écrite** : d'où `Mode_Auto_Eff`.

---

## 6. Correspondance firmware → Stateflow

| Firmware | Stateflow |
|---|---|
| `lireEntrees()` (valeurs fixes, `T_cap`, `H_fin`) + `majRegime()` | `entry, during` de `FONCTIONNEMENT_NORMAL` |
| `demarrerCycle()` | `entry` de `EN_CYCLE` + transitions de `ATTENTE_DEMARRAGE` |
| `etapeRegulation(false)` pendant demande / prolongation | régions parallèles `SOURCE` ‖ `PHASE` |
| `arbitrageSource()` | transitions entre `MODE_SOLAIRE` / `MODE_H2` / `MODE_GPL` |
| `etapeSolaire()` (post-purge) | actions de `MODE_SOLAIRE` |
| `gererCombustion()` | `PURGE` / `ALLUMAGE` / `REGULATION` et leurs transitions |
| `majPalier()`, `Periode_Regul`, palier imposé | `during` de `REGULATION` |
| `palierInitial()` + `armerPurgeMiseEnRoute()` | bloc palier_initial (entry `EN_CYCLE`, transitions solaire → combustion) |
| `entrerErreurCombustion()` + `case ETAT_ERREUR_COMBUSTION` | `ERREUR_COMBUSTION` |
| `case ETAT_DEMANDE_PROLONGATION`, `case ETAT_PROLONGATION`, transition prioritaire 4 | région `PHASE` |
| transitions prioritaires 1a / 1b | SSID 118 + transition `duration(...)` |
| transitions prioritaires 2 / 3 | bord de `EN_CYCLE` → `SECHAGE_TERMINE` |
| `declencherUrgence(URG_PERTE_FLAMME)` | `REGULATION` → `URGENCE_ATEX` |
| `case ETAT_URGENCE_ATEX` (réarmement) | `URGENCE_ATEX` → `ATTENTE_DEMARRAGE` |

**Différences assumées** (sans effet sur les sorties dans l'usage normal) :
UP/DOWN pendant le cycle modifient `T_cible` dans le firmware ; dans Simulink,
`T_cible` est une entrée (bloc Constant), et le chart recalcule simplement les
seuils à chaque pas. Le menu de paramètres (`CONFIG_MENU`) n'est pas modélisé :
les paramètres sont les données `Local` du §2.3.

---

## 7. Mode d'emploi

1. Ouvrir **`matlab/Commande_Sechoir_Hybride_corrige.slx`** (chart d'origine +
   corrections physiques Pnom/Kth/tauth). Ne pas partir d'un modèle déjà
   modifié par une version précédente du script.
2. Dans la fenêtre de commande MATLAB, se placer dans le dossier `matlab/`,
   puis :
   `completer_fsm_sechoir('Commande_Sechoir_Hybride_corrige')`
3. Lire la sortie : `[HIERARCHIE INCORRECTE]` → repli manuel du §1, puis
   relancer ; sinon, clic droit sur le chart → *Update Chart*.
4. Câbler les **2 nouvelles entrées** `Btn_Stop` et `Btn_Rearm` (blocs
   Constant à 0 pour une première simulation), et régler `Tps_Prolongation`
   **en secondes** (1800).
5. Lancer la simulation et ouvrir le *Diagnostic Viewer* : me renvoyer tout
   message d'erreur tel quel.

---

## 8. Vérifications effectuées — et ce qui ne l'a pas été

Effectué, sans MATLAB :
- le script a été analysé par **MISS_HIT** (analyseur statique MATLAB) :
  syntaxe correcte ;
- les **37 libellés** générés (9 états, 28 transitions) ont été reconstruits et
  analysés comme du code MATLAB : syntaxe correcte (une erreur `end end` a été
  trouvée et corrigée ainsi) ;
- chaque variable utilisée dans les libellés est déclarée (données d'origine
  ou ajoutées par le script), aucune donnée ajoutée n'est inutilisée, et
  aucune **entrée** n'est écrite ;
- la hiérarchie, les noms de données et le langage d'action ont été relevés
  directement dans le `.slx` d'origine (`chart_122.xml`).

**Non vérifié** (aucun MATLAB disponible) : l'exécution du script ; les API
`Decomposition`, changement de parent, `ExecutionOrder`, tracé des
transitions ; les opérateurs `temporalCount` et `duration` ; la simulation
elle-même. Le script signale chaque échec d'API au lieu de s'arrêter.

---

## 9. Interface avec les sous-systèmes existants

- `CONVERSION_PUISSANCE` reçoit `V_Fl_1..3` et produit la puissance injectée
  dans `MODELE_THERMIQUE` (inchangé).
- `MODELE_THERMIQUE` produit `T_sec`, rebouclé en entrée du chart (inchangé).
