# Séchoir solaire hybride — Modèle Simulink/Stateflow de la commande

Ce document est la spécification complète de la partie **FSM** du modèle
`Commande_Sechoir_Hybride.slx` (le sous-système « FSM », son chart
Stateflow, ses données et ses transitions). Il est écrit pour **correspondre
exactement au firmware Arduino** `sechoir_hybride/sechoir_hybride.ino` : le
firmware est la source de vérité en cas de conflit (Model-Based Design à
l'envers du sens habituel ici — la mémoire suivra le code, pas l'inverse).

Les sous-systèmes `CONVERSION_PUISSANCE` (palier → puissance) et
`MODELE_THERMIQUE` (réponse thermique de la chambre) existent déjà dans le
modèle et ne sont pas repris ici, sauf pour rappeler leur interface avec le
chart FSM (§8).

Ce document sert de référence à `matlab/completer_fsm_sechoir.m`, qui
construit automatiquement tout ce qui suit via l'API Stateflow. Si le script
bute sur une limitation d'API dans votre version de MATLAB, ce document
contient tout le nécessaire pour compléter le chart **à la main** dans
l'éditeur Stateflow.

---

## 0. Révision majeure : régions parallèles SOURCE / PHASE

Une revue croisée (script précédent vs. lecture directe du firmware) a
montré que `PROLONGATION` et `FIN_TEMPORISATION`, modélisés comme de simples
OR-siblings de `MODE_SOLAIRE`/`MODE_H2`/`MODE_GPL`, **arrêtaient de facto la
régulation de combustion** dès qu'on y entrait — alors que le firmware
(`etapeRegulation(bool synchroniser)`, appelée avec `synchroniser=false`
depuis `ETAT_PROLONGATION` et `ETAT_FIN_TEMPORISATION`) continue d'exécuter
`arbitrageSource()` puis `etapeSolaire()`/`gererCombustion()` **sans aucune
interruption**, quel que soit l'état affiché.

Corrigé par une décomposition **AND (parallèle)** de `FONCTIONNEMENT_NORMAL`
en deux régions indépendantes :

```
FONCTIONNEMENT_NORMAL   (Decomposition = PARALLEL_AND)
├── SOURCE   (OR)  — laquelle des 3 sources est active, avec sa propre
│                    sous-machine PURGE/ALLUMAGE/REGULATION
│     ├── MODE_SOLAIRE
│     ├── MODE_H2  (PURGE/ALLUMAGE/REGULATION)
│     ├── MODE_GPL (PURGE/ALLUMAGE/REGULATION)
│     └── ERREUR_COMBUSTION
└── PHASE    (OR)  — ce qui est affiché/confirmé, indépendamment de la source
      ├── NORMAL          (= etat_courant ∈ {SOLAIRE,H2,GPL} côté firmware)
      ├── PROLONGATION
      └── FIN_TEMPORISATION
```

Cette structure reproduit fidèlement `etapeRegulation()` : entrer dans
`PROLONGATION` (région PHASE) ne touche pas à la région SOURCE, qui continue
son cycle PURGE→ALLUMAGE→REGULATION (ou son fonctionnement solaire) sans
interruption — exactement le comportement du firmware.

**Alternative envisagée et écartée** : dupliquer entièrement la sous-machine
SOURCE (solaire + H2 + GPL, avec leurs PURGE/ALLUMAGE/REGULATION) une
deuxième fois dans `PROLONGATION` et une troisième fois dans
`FIN_TEMPORISATION`. Écartée car le firmware confirme que `Source_Active`
peut valoir solaire pendant ces deux phases (aucune restriction) : la
duplication aurait dû reproduire l'intégralité de la région SOURCE deux fois
de plus, un risque de divergence bien plus grave qu'une primitive Stateflow
non vérifiée.

**Risque assumé et signalé** : `Decomposition='PARALLEL_AND'` et le
reparentage programmatique (`.Parent =`, avec repli sur l'API historique
`sf('set', id, '.parent', ...)`) de `MODE_SOLAIRE`/`MODE_H2`/`MODE_GPL`/
`PROLONGATION`/`FIN_TEMPORISATION` **n'ont pas été testés dans une vraie
session MATLAB**. Si l'un des deux échoue, `completer_fsm_sechoir.m`
l'indique clairement (`[STRUCTURE INCOMPLETE]` / `[ECHEC reparentage...]`)
et propose un repli manuel : glisser-déposer les 5 états dans les nouvelles
boîtes `SOURCE`/`PHASE` dans l'éditeur Stateflow, puis relancer le script
(idempotent), qui complète alors automatiquement tout ce qui en dépend
(`ERREUR_COMBUSTION`, transitions de fin de cycle, `Duree_Max_Cycle`).

---

## 1. Hiérarchie d'états

```
FSM/Chart
│
├── ATTENTE_DEMARRAGE        (état initial, tout fermé)
│
├── FONCTIONNEMENT_NORMAL (AND-state, deux régions parallèles)
│     during: H_fin = H_produit + H_amb;      (recalculé en continu, quelle
│                                               que soit la combinaison de
│                                               sous-états actifs)
│     │
│     ├── SOURCE (OR)
│     │     entry: T_init / calculer_seuils / palier=100% — UNE FOIS par
│     │            cycle (voir §3)
│     │     │
│     │     ├── MODE_SOLAIRE             (passif : aucune électrovanne)
│     │     ├── MODE_H2                  ┐
│     │     │     ├── PURGE              │ sous-machine de combustion,
│     │     │     ├── ALLUMAGE           │ IDENTIQUE dans MODE_H2 et MODE_GPL
│     │     │     └── REGULATION         │ (palier 100/67/33/0 %)
│     │     ├── MODE_GPL                 ┘
│     │     │     ├── PURGE
│     │     │     ├── ALLUMAGE
│     │     │     └── REGULATION
│     │     └── ERREUR_COMBUSTION   (basculement raté ou échecs d'allumage
│     │                              répétés ; menu REESSAYER/MANUEL/
│     │                              AUTOMATIQUE)
│     │
│     └── PHASE (OR)
│           ├── NORMAL             (= etat_courant ∈ {SOLAIRE,H2,GPL})
│           ├── PROLONGATION       (Duree_Max_Cycle dépassée, régulation
│           │                       poursuivie par la région SOURCE)
│           └── FIN_TEMPORISATION  (demande d'arrêt : confirmation, report,
│                                   annulation auto ou arrêt auto)
│
├── SECHAGE_TERMINE          (cycle terminé, normal ou forcé)
│
└── URGENCE_ATEX             (priorité absolue, accessible depuis
                              ATTENTE_DEMARRAGE, FONCTIONNEMENT_NORMAL
                              [couvre SOURCE/PHASE/ERREUR_COMBUSTION en tant
                              que descendants] et SECHAGE_TERMINE)
```

Correspondance avec le firmware : chaque état porte le nom de la valeur
`enum EtatFSM` correspondante. `PURGE`/`ALLUMAGE`/`REGULATION` correspondent
aux valeurs de `enum PhaseCombustion` — `REGULATION` regroupe ici
`PH_PALIER_100`/`PH_PALIER_67`/`PH_PALIER_33`, la distinction étant portée
par la variable `palier`, pas par des sous-états séparés (même
simplification que dans `gererCombustion()`). `NORMAL`/`PROLONGATION`/
`FIN_TEMPORISATION` correspondent directement à `etat_courant`, mais
uniquement pour la partie « affichage/confirmation » : la partie « quelle
source, quelle phase de combustion » est intégralement portée par SOURCE,
en parallèle.

---

## 2. Dictionnaire de données du chart

Tous les noms ci-dessous sont ceux **déjà utilisés dans `sechoir_hybride.ino`**
(sans le préfixe `Config.`).

### 2.1 Entrées (`Scope = Input`)

| Nom | Type | Correspond à |
|---|---|---|
| `Btn_Start` | uint8 | `frontPris(B_START)` |
| `Btn_Stop` | uint8 | `frontPris(B_STOP)` |
| `Btn_OK` | uint8 | `frontPris(B_OK)` |
| `Btn_Rearm` | uint8 | `frontPris(B_REARM)` — **ajouté** : chemin de réarmement physique manquant du chart précédent (seul `Btn_OK` était câblé) |
| `Mode_Auto` | uint8 | `Config.Mode_Auto` (lu une seule fois, en entrée d'`ATTENTE_DEMARRAGE`, vers `Mode_Auto_Eff` — voir §2.3) |
| `Choix_Manuel` | uint8 | `Config.Choix_Mode` (1=Solaire, 2=H2, 3=GPL) |
| `T_sec`, `H_sec` | double | `T_sec`, `H_sec` (mesures chambre) |
| `T_cap`, `T_amb`, `H_amb` | double | `T_cap`, `T_amb`, `H_amb` |
| `T_cible` | double | `Config.T_cible` |
| `T_init_saisie` | double | valeur de démarrage saisie par l'opérateur en mode manuel — **ajouté** (voir §3, `T_init` était sinon figé à 25 en manuel) |
| `H_produit` | double | `Config.H_produit_cible` |
| `Press_H2` | double | `Press_H2` |
| `Press_H2_Min` | double | `Config.Press_H2_Min` |
| `MQ8_H2`, `MQ6_But` | double | `MQ8_H2`, `MQ6_But` |
| `Seuil_MQ8`, `Seuil_MQ6` | double | `Config.Seuil_MQ8`, `Config.Seuil_MQ6` |
| `Flame` | uint8 | `Flame` |
| `AU_Urgence` | uint8 | `AU_Urgence` — **renommé** depuis `AU_Manuel` (probable erreur de frappe : `causeUrgencePresente()` teste bien `AU_Urgence` dans le firmware ; à vérifier contre le port réel du sous-système) |

### 2.2 Sorties (`Scope = Output`)

| Nom | Type | Correspond à |
|---|---|---|
| `V_H2`, `V_But` | uint8 | `V_H2`, `V_But` |
| `V_Fl_1`, `V_Fl_2`, `V_Fl_3` | uint8 | `EV1`, `EV2`, `EV3` |
| `Spark` | uint8 | `Spark` |
| `PWM_Purge`, `PWM_Inj`, `PWM_Ext` | uint8 | `PWM_Purge`, `PWM_Distrib`, `PWM_Extract` |
| `Buzzer` | uint8 | `Buzzer` |
| `Etat_LCD` | uint8 | `Etat_LCD` |

### 2.3 Données locales (`Scope = Local`)

| Nom | Type | Défaut | Correspond à |
|---|---|---|---|
| `T1_seuil`, `T2_seuil`, `T3_seuil` | double | 0 | `T1`, `T2`, `T3` |
| `T_init` | double | 25 | `Config.T_init` — écrit désormais par l'`entry:` de `SOURCE` (voir §3), plus par chaque transition individuellement |
| `Hhyst` | double | 5 | `Config.Hhyst` |
| `H_fin` | double | 0 | `H_fin` (recalculé en continu) |
| `palier` | uint8 | 0 | `palier` (0=100 %, 1=67 %, 2=33 %, 3=0 %) |
| `Source_Active` | uint8 | 0 | `Source_Active` (0=Solaire, 1=H2, 2=GPL) |
| `raison_purge` | uint8 | 0 | `raison_purge` (0=Démarrage, 1=Échec, 2=Basculement, 3=Palier0) |
| `nb_echecs_allumage` | uint16 | 0 | `nb_echecs_allumage` |
| `MAX_ECHECS` | uint16 | 3 | `MAX_ECHECS_AVANT_ALARME` |
| `Choix_Erreur` | uint8 | 0 | `Choix_Erreur` (0=Réessayer, 1=Manuel, 2=Automatique) |
| `raison_erreur_combustion` | uint8 | 1 | `raison_erreur_combustion` (0=ERR_BASCULEMENT, 1=ERR_ALLUMAGE_REPETE) — **ajouté**, absent du chart précédent |
| `Mode_Auto_Eff` | uint8 | 0 | miroir local de `Mode_Auto`, resynchronisé à chaque entrée dans `ATTENTE_DEMARRAGE` — **ajouté** : écrire directement dans `Mode_Auto` (Input) est illégal en Stateflow |
| `Btn_SELECT_prev` | uint8 | 0 | mémorisation pour détection de front sur `Btn_SELECT` — **ajouté** (voir §4.3) |
| `palier0_stable` | uint8 | 0 | vrai si `palier==3` depuis ≥60 s (voir §4.2, PURGE) — **ajouté**, remplace un `after(60,sec)` mal placé dans une garde composite |
| `arret_force_duree` | uint8 | 0 | `arret_force_duree` |
| `Seuil_Tcap_ON`, `Seuil_Tcap_OFF` | double | 55 / 45 | `SEUIL_T_CAP_SOLAIRE_ON/OFF` |
| `T_SEC_MAX_SECURITE` | double | 90 | `T_SEC_MAX_SECURITE` — **ajouté** (constante nommée, remplace un littéral `90`) |
| `Marge_Retour_H2` | double | 0,5 | `MARGE_RETOUR_H2` |
| `Temps_Purge` | double | 120 | `Config.Temps_Purge` (s — déjà en secondes côté firmware, `*1000UL`) |
| `Temps_Allumage` | double | 4 | `Config.Temps_Allumage` (s, idem) |
| `Temps_Min_Fin` | double | — | `Config.Temps_Min_Fin` (**minutes** côté firmware, `*60000UL`) — supposé déjà présent dans le `.slx` fourni ; utilisé via `after(Temps_Min_Fin*60,sec)` (voir remarque unités ci-dessous) |
| `Temps_Arret_Auto` | double | 300 | `Config.Temps_Arret_Auto` (s — déjà en secondes, `*1000UL`) |
| `Temps_Prolongation` | double | — | `Config.Temps_Prolongation` (**minutes**, `*60000UL`) — supposé présent dans le `.slx` fourni ; utilisé via `after(Temps_Prolongation*60,sec)` |
| `Duree_Max_Cycle` | double | 600 | `Config.Duree_Max_Cycle` (**minutes**, `*60000UL` — changé de 36000 s à 600 min pour la même valeur réelle de 10 h) ; utilisé via `after(Duree_Max_Cycle*60,sec)` |

> **Remarque unités (corrigée, portée élargie)** : une revue précédente
> n'avait signalé le mésalignement minutes/secondes que pour
> `Temps_Prolongation`. La lecture directe du firmware
> (`pasFSM()`/`conditionFinDeCycle()`) montre que **`Duree_Max_Cycle` et
> `Temps_Min_Fin` sont eux aussi stockés en minutes** dans `Config`
> (`* 60000UL`), alors que `Temps_Purge`, `Temps_Allumage` et
> `Temps_Arret_Auto` sont bien en secondes (`* 1000UL`). Les trois premiers
> sont donc utilisés dans le chart via `after(X*60, sec)` ; si
> `Temps_Min_Fin`/`Temps_Prolongation` sont déjà câblés depuis un bloc
> `Constant` exprimé en minutes dans le `.slx` fourni, ce facteur `*60` est
> correct tel quel — vérifiez simplement qu'aucune conversion n'est déjà
> faite en amont (double conversion sinon).

---

## 3. Fonctions partagées — **inlinées**, pas de `Stateflow.EMLFunction`

`Stateflow.EMLFunction` s'est révélée indisponible dans la session MATLAB
réelle testée. La logique est donc **écrite directement** dans chaque
action d'état/transition qui en a besoin, dupliquée entre `MODE_H2` et
`MODE_GPL` (équivalent du paramètre booléen `utiliseH2` de
`gererCombustion()`, resté factorisé côté C++).

**`fermer_gaz`** :
```matlab
V_H2=uint8(0); V_But=uint8(0); V_Fl_1=uint8(0); V_Fl_2=uint8(0); V_Fl_3=uint8(0); Spark=uint8(0);
```

**`appliquer_palier`** :
```matlab
V_Fl_1=uint8(palier==0); V_Fl_2=uint8(palier==0||palier==1); V_Fl_3=uint8(palier~=3);
```

**`calculer_seuils`** :
```matlab
ECART_MIN=3;
if T_cible<=T_init+ECART_MIN
  T1_seuil=T_cible; T2_seuil=T_cible; T3_seuil=T_cible;
else
  delta=T_cible-T_init;
  T1_seuil=T_init+delta/3; T2_seuil=T_init+2*delta/3; T3_seuil=T_cible;
end
```

**`gerer_palier`** :
```matlab
demi=Hhyst/2;
switch palier
  case 0
    if T_sec >= T1_seuil + demi, palier = uint8(1); end
  case 1
    if T_sec >= T2_seuil + demi
      palier = uint8(2);
    elseif T_sec < T1_seuil - demi
      palier = uint8(0);
    end
  case 2
    if T_sec >= T3_seuil + demi
      palier = uint8(3);
    elseif T_sec < T2_seuil - demi
      palier = uint8(1);
    end
end
if palier ~= 3
  V_Fl_1=uint8(palier==0); V_Fl_2=uint8(palier==0||palier==1); V_Fl_3=uint8(palier~=3);
end
```

**`entry:` de `SOURCE`** — **nouveau**, remplace le calcul de `T_init`/seuils
qui était (partiellement) dupliqué dans chaque transition de démarrage.
S'exécute une seule fois par cycle (`SOURCE` n'est réactivée qu'au
`Btn_Start`, jamais lors d'un basculement H2↔GPL ni d'une reprise depuis
`ERREUR_COMBUSTION`, ces transitions restant internes à la région) :
```matlab
if (Mode_Auto_Eff==0) { T_init=T_init_saisie; } else { T_init=T_sec; }
ECART_MIN=3; if T_cible<=T_init+ECART_MIN, T1_seuil=T_cible; T2_seuil=T_cible; T3_seuil=T_cible;
else delta=T_cible-T_init; T1_seuil=T_init+delta/3; T2_seuil=T_init+2*delta/3; T3_seuil=T_cible; end
palier=uint8(0); raison_purge=uint8(0); nb_echecs_allumage=uint16(0);
```
Couvre aussi bien les 3 démarrages automatiques que les 3 démarrages manuels
— y compris les 2 transitions manuelles **préexistantes** du `.slx` fourni
(solaire/H2), que le script ne modifie pas directement.

---

## 4. Code de chaque état

### 4.1 États simples (entry uniquement)

| État | `entry:` |
|---|---|
| `ATTENTE_DEMARRAGE` | `fermer_gaz` `PWM_Purge=uint8(0); PWM_Inj=uint8(0); PWM_Ext=uint8(0); Buzzer=uint8(0); Etat_LCD=uint8(0); Mode_Auto_Eff=Mode_Auto;` |
| `MODE_SOLAIRE` | `fermer_gaz` `PWM_Purge=uint8(0); PWM_Inj=uint8(220); PWM_Ext=uint8(150); Etat_LCD=uint8(2);` |
| `PROLONGATION` | `Etat_LCD=uint8(5);` |
| `FIN_TEMPORISATION` | `Etat_LCD=uint8(6);` |
| `SECHAGE_TERMINE` | `fermer_gaz` `PWM_Purge=uint8(0); PWM_Inj=uint8(0); PWM_Ext=uint8(90);` |
| `URGENCE_ATEX` | `fermer_gaz` `PWM_Purge=uint8(255); PWM_Inj=uint8(0); PWM_Ext=uint8(255); Buzzer=uint8(1); Etat_LCD=uint8(9);` |

`FONCTIONNEMENT_NORMAL` (le super-état AND lui-même) :
```
during: H_fin = H_produit + H_amb;
```
(s'exécute à chaque pas quelle que soit la combinaison de sous-états actifs
dans les deux régions — inchangé par la restructuration parallèle.)

### 4.2 Sous-machine de combustion (`MODE_H2` et `MODE_GPL`, identique, sous `SOURCE`)

Pour `MODE_H2` : `actionVanne = 'V_H2=uint8(1); V_But=uint8(0);'`
Pour `MODE_GPL` : `actionVanne = 'V_H2=uint8(0); V_But=uint8(1);'`

```
PURGE  (état initial de la sous-machine, transition par défaut sans source)
  entry: fermer_gaz  Spark=uint8(0); PWM_Purge=uint8(255); PWM_Inj=uint8(60); PWM_Ext=uint8(200); palier0_stable=uint8(0);
  during: if (raison_purge==3 && after(60,sec)) { palier0_stable=uint8(1); }

ALLUMAGE
  entry: <actionVanne> appliquer_palier  Spark=uint8(1); palier0_stable=uint8(0);

REGULATION
  entry: <actionVanne>
  during: gerer_palier
```

`palier0_stable` remplace la garde composite `(palier==3 && after(60,sec))`
qui figurait auparavant dans la condition de fin de cycle : `after()` mesure
le temps depuis l'entrée dans l'état source de la transition qui le porte,
pas depuis qu'une variable a changé de valeur — il ne peut donc pas mesurer
« depuis que `palier` vaut 3 » s'il est écrit sur une transition qui part
d'ailleurs. Le sourcer directement dans `PURGE` (état réellement (ré)entré
au moment précis où `palier` devient 3, via la transition `REGULATION` →
`PURGE` avec `raison_purge=3`) mesure la bonne durée nativement.

### 4.3 `ERREUR_COMBUSTION` (sibling de `MODE_SOLAIRE`/`MODE_H2`/`MODE_GPL`, sous `SOURCE`)

```
entry: fermer_gaz  Buzzer=uint8(1); PWM_Purge=uint8(255); Choix_Erreur=uint8(0);
during: if (Btn_SELECT==1 && Btn_SELECT_prev==0) { Choix_Erreur = mod(Choix_Erreur+uint8(1), uint8(3)); }
        Btn_SELECT_prev = Btn_SELECT;
```

Détection de front ajoutée sur `Btn_SELECT` (`Btn_SELECT_prev`) : sans elle,
`Choix_Erreur` défilait en boucle tant que le bouton restait maintenu
(niveau, pas front) — corrigé.

---

## 5. Table complète des transitions

### 5.1 Sous-machine de combustion (communes à MODE_H2/MODE_GPL)

| Source → Destination | Condition | Action |
|---|---|---|
| *(défaut)* → `PURGE` | — | — |
| `PURGE` → `ALLUMAGE` | `after(Temps_Purge,sec) && (raison_purge~=3 \|\| T_sec<T3_seuil-Hhyst/2)` | `if(raison_purge==3){palier=2;}` |
| `ALLUMAGE` → `REGULATION` | `Flame==1` | `Spark=0; nb_echecs_allumage=0;` |
| `ALLUMAGE` → `ERREUR_COMBUSTION` | `after(Temps_Allumage,sec) && Flame==0 && raison_purge==2` | `raison_erreur_combustion=0;` *(ERR_BASCULEMENT, échec de bascule : escalade immédiate)* |
| `ALLUMAGE` → `PURGE` | `after(Temps_Allumage,sec) && Flame==0 && raison_purge~=2 && (nb_echecs_allumage+1)<MAX_ECHECS` | `nb_echecs_allumage++; raison_purge=1;` |
| `ALLUMAGE` → `ERREUR_COMBUSTION` | `after(Temps_Allumage,sec) && Flame==0 && (nb_echecs_allumage+1)>=MAX_ECHECS` | `nb_echecs_allumage++; raison_erreur_combustion=1;` *(ERR_ALLUMAGE_REPETE)* |
| `REGULATION` → `PURGE` | `Flame==0 && (nb_echecs_allumage+1)<MAX_ECHECS` | `nb_echecs_allumage++; raison_purge=1;` |
| `REGULATION` → `PURGE` | `palier==3` *(coupure volontaire, consigne atteinte)* | `raison_purge=3;` |

**Corrigé** (hors-par-un) : le firmware incrémente `nb_echecs_allumage`
**puis** teste `>= MAX_ECHECS_AVANT_ALARME`. Le chart précédent testait
`< MAX_ECHECS` **avant** d'incrémenter, ce qui autorisait une tentative de
plus que le firmware avant d'escalader. Les gardes ci-dessus comparent
`(nb_echecs_allumage+1)` au seuil pour reproduire exactement l'ordre du
firmware (escalade au 3ᵉ échec avec `MAX_ECHECS=3`).

**Supprimé** : `REGULATION → ERREUR_COMBUSTION` (« perte de flamme
répétée »). C'était du code mort — vérifié dans le firmware lui-même, pas
une particularité de la traduction Stateflow : `nb_echecs_allumage` est
systématiquement remis à 0 par l'unique transition qui mène à `REGULATION`
(`ALLUMAGE → REGULATION`), donc une première perte de flamme ne peut jamais
l'amener directement à `MAX_ECHECS` (≥2) ; toute reprise ultérieure repasse
par `ALLUMAGE`, qui gère lui-même l'escalade.

### 5.2 `ERREUR_COMBUSTION`

| Destination | Condition | Action |
|---|---|---|
| `MODE_H2` | `Btn_OK==1 && (Choix_Erreur==0\|\|Choix_Erreur==2) && Source_Active==1` | `if(Choix_Erreur==2){Mode_Auto_Eff=1;} nb_echecs_allumage=0; if(raison_erreur_combustion==0){raison_purge=2;}else{raison_purge=1;}` |
| `MODE_GPL` | `Btn_OK==1 && (Choix_Erreur==0\|\|Choix_Erreur==2) && Source_Active==2` | *(idem)* |
| `ATTENTE_DEMARRAGE` | `Btn_OK==1 && Choix_Erreur==1` | `Mode_Auto_Eff=0; Buzzer=0;` |
| `SECHAGE_TERMINE` | `Btn_Stop==1` | `Buzzer=0;` `fermer_gaz` |

**Corrigé** : `Mode_Auto` (Input) → `Mode_Auto_Eff` (Local), l'écriture dans
un Input étant illégale en Stateflow (bloquant à la compilation). **Corrigé**
également : `raison_purge` ne vaut plus systématiquement `2` (bascule) au
retour — il suit `raison_erreur_combustion` (`2` si l'erreur venait d'un
échec de bascule, `1` sinon), redonnant un jeu complet de `MAX_ECHECS`
tentatives après un échec d'allumage ordinaire au lieu d'une seule.

### 5.3 Transitions prioritaires

| Source → Destination | Condition | Action | Remarque |
|---|---|---|---|
| `ATTENTE_DEMARRAGE` → `URGENCE_ATEX` | `MQ8_H2>Seuil_MQ8 \|\| MQ6_But>Seuil_MQ6 \|\| AU_Urgence==1` | — | **ajouté** |
| `FONCTIONNEMENT_NORMAL` → `URGENCE_ATEX` | *(idem)* | — | **ajouté** — couvre tout le cycle, y compris `ERREUR_COMBUSTION` (descendant) |
| `SECHAGE_TERMINE` → `URGENCE_ATEX` | *(idem)* | — | **ajouté** |
| `FONCTIONNEMENT_NORMAL` → `SECHAGE_TERMINE` | `Btn_Stop==1` | `fermer_gaz` | garde `in(...)` retirée : inutile, la source de la transition suffit à restreindre son évaluation |
| `FONCTIONNEMENT_NORMAL` → `SECHAGE_TERMINE` | `T_sec>=T_SEC_MAX_SECURITE` | `fermer_gaz` | idem |
| `PROLONGATION` → `SECHAGE_TERMINE` | `after(Temps_Prolongation*60,sec)` | `arret_force_duree=1; fermer_gaz` | unité minutes → `*60` |
| `NORMAL` → `FIN_TEMPORISATION` | `after(Temps_Min_Fin*60,sec) && (H_sec<=H_fin \|\| palier0_stable==1)` | — | source = `NORMAL` (région PHASE), pas `FONCTIONNEMENT_NORMAL` |
| `PROLONGATION` → `FIN_TEMPORISATION` | `H_sec<=H_fin \|\| palier0_stable==1` | — | pas de garde `Temps_Min_Fin` : déjà vraie par construction si `Duree_Max_Cycle>=Temps_Min_Fin` (voir §2.3) |
| `FIN_TEMPORISATION` → `FIN_TEMPORISATION` | `Btn_Menu==1` | — | **ajouté** : « report », auto-transition qui réinitialise le chronomètre `Temps_Arret_Auto` par sortie/réentrée standard |
| `FIN_TEMPORISATION` → `NORMAL` | `H_sec>H_fin && palier0_stable==0` | — | **ajouté** : annulation automatique si la condition de fin redevient fausse |
| `FIN_TEMPORISATION` → `SECHAGE_TERMINE` | `Btn_OK==1 \|\| Btn_Stop==1` | `fermer_gaz` | confirmation opérateur |
| `FIN_TEMPORISATION` → `SECHAGE_TERMINE` | `after(Temps_Arret_Auto,sec)` | `fermer_gaz` | arrêt automatique |
| `NORMAL` → `PROLONGATION` | `after(Duree_Max_Cycle*60,sec)` | — | source = `NORMAL`, unité minutes → `*60` |
| `URGENCE_ATEX` → `ATTENTE_DEMARRAGE` | `(Btn_OK==1\|\|Btn_Rearm==1) && MQ8_H2<=Seuil_MQ8 && MQ6_But<=Seuil_MQ6 && AU_Urgence==0` | `Buzzer=0; palier=0; raison_purge=0;` | **corrigé** : ciblait à tort `FONCTIONNEMENT_NORMAL` — le firmware repart de `ATTENTE_DEMARRAGE`, l'opérateur doit rappuyer sur Start ; chemin `Btn_Rearm` ajouté |

**Découverte majeure** (lecture directe du firmware, absente de toute revue
précédente) : le chart précédent ne modélisait **que la sortie** de
`URGENCE_ATEX` (le réarmement) — aucune transition n'y **entrait**. L'état
le plus critique du système (arrêt d'urgence ATEX) était donc totalement
inatteignable dans le modèle Simulink. Corrigé par les 3 premières lignes
du tableau ci-dessus. Si un état `CONFIG_MENU` existe dans le `.slx` fourni
(non géré par ce script), ajoutez-y la même garde à la main.

### 5.4 Arbitrage de source (mode automatique, région `SOURCE`)

| Source → Destination | Condition | Action |
|---|---|---|
| *(déjà présent)* `ATTENTE_DEMARRAGE` → `MODE_SOLAIRE` | `Btn_Start==1 && Mode_Auto==0 && Choix_Manuel==1` | *(vérifier `Mode_Auto`→`Mode_Auto_Eff`, non modifié par le script)* |
| *(déjà présent)* `ATTENTE_DEMARRAGE` → `MODE_H2` | `Btn_Start==1 && Mode_Auto==0 && Choix_Manuel==2` | *(idem)* |
| `ATTENTE_DEMARRAGE` → `MODE_GPL` | `Btn_Start==1 && Mode_Auto_Eff==0 && Choix_Manuel==3` | — |
| `ATTENTE_DEMARRAGE` → `MODE_SOLAIRE` | `Btn_Start==1 && Mode_Auto_Eff==1 && T_cap>=Seuil_Tcap_ON` | — |
| `ATTENTE_DEMARRAGE` → `MODE_H2` | `Btn_Start==1 && Mode_Auto_Eff==1 && T_cap<Seuil_Tcap_ON && Press_H2>=Press_H2_Min` | — |
| `ATTENTE_DEMARRAGE` → `MODE_GPL` | `Btn_Start==1 && Mode_Auto_Eff==1 && T_cap<Seuil_Tcap_ON && Press_H2<Press_H2_Min` | — |
| `MODE_H2` → `MODE_SOLAIRE` | `Mode_Auto_Eff==1 && T_cap>=Seuil_Tcap_ON` | `raison_purge=0;` |
| `MODE_GPL` → `MODE_SOLAIRE` | `Mode_Auto_Eff==1 && T_cap>=Seuil_Tcap_ON` | `raison_purge=0;` |
| `MODE_SOLAIRE` → `MODE_H2` | `Mode_Auto_Eff==1 && T_cap<Seuil_Tcap_OFF && Press_H2>=Press_H2_Min` | `palier=0; raison_purge=0;` |
| `MODE_SOLAIRE` → `MODE_GPL` | `Mode_Auto_Eff==1 && T_cap<Seuil_Tcap_OFF && Press_H2<Press_H2_Min` | *(idem)* |
| `MODE_H2` → `MODE_GPL` | `Mode_Auto_Eff==1 && Press_H2<Press_H2_Min` | `raison_purge=2;` *(palier **conservé**)* |
| `MODE_GPL` → `MODE_H2` | `Mode_Auto_Eff==1 && Press_H2>=Press_H2_Min+Marge_Retour_H2` | `raison_purge=2;` *(retour auto, marge anti-court-cycle)* |

**Corrigé** (zone morte de démarrage à froid) : `demarrerCycle()` dans le
firmware n'utilise **que** `Seuil_Tcap_ON` pour le premier arbitrage à froid
(`T_cap>=ON → solaire ; sinon combustion`) — jamais `Seuil_Tcap_OFF` à ce
stade (l'hystérèse ON/OFF ne s'applique qu'en cours de cycle, via
`arbitrageSource()`). Le chart précédent utilisait `Seuil_Tcap_OFF` pour les
3 transitions de démarrage automatique, créant une zone morte
`T_cap ∈ [OFF,ON)` où `Btn_Start` ne produisait aucune transition. Corrigé :
les 3 transitions de démarrage automatique utilisent désormais uniquement
`Seuil_Tcap_ON`.

**T_init/seuils** : ne sont plus calculés dans ces transitions (redondant
avec l'`entry:` de `SOURCE`, voir §3) — simplifié.

**Résidu signalé, non corrigé automatiquement** : les 2 transitions
manuelles préexistantes (solaire/H2) ne sont pas modifiées par ce script
(il ne touche jamais une transition qu'il n'a pas lui-même créée). Si elles
testent encore `Mode_Auto==0` plutôt que `Mode_Auto_Eff==0`, corrigez-les à
la main dans l'éditeur Stateflow pour rester cohérent avec le reste du
chart (impact limité : ne joue que dans le cas de récupération
« AUTOMATIQUE » depuis `ERREUR_COMBUSTION`).

---

## 6. Notes d'implémentation Stateflow

- **Régions parallèles et priorité des transitions internes** : une
  transition sourcée sur `FONCTIONNEMENT_NORMAL` lui-même sort/rentre dans
  les **deux** régions (SOURCE et PHASE) — à réserver aux transitions qui
  doivent réellement tout réinitialiser (urgence, arrêt propre,
  surchauffe). Toute transition qui ne doit affecter qu'une seule région
  (fin de cycle, `Duree_Max_Cycle`, report/annulation de
  `FIN_TEMPORISATION`) doit être sourcée **depuis l'intérieur de cette
  région** (`NORMAL`, `PROLONGATION`, `FIN_TEMPORISATION`), jamais depuis le
  bord de `FONCTIONNEMENT_NORMAL` — sans quoi Stateflow force la
  sortie/réentrée de la région SOURCE et perd son sous-état actif.
- **Priorité entre transition interne et transition de bord** : quand une
  transition sourcée sur `FONCTIONNEMENT_NORMAL` et une transition locale à
  un état imbriqué (ex. `FIN_TEMPORISATION`/`ERREUR_COMBUSTION` gérant
  `Btn_Stop` elles-mêmes) sont valides simultanément, Stateflow donne
  toujours la priorité à la transition la plus profonde — pas besoin de
  garde `in(...)` d'exclusion explicite.
- **`in(NomEtat)`** : opérateur Stateflow natif, syntaxe non quotée.
- **`after(N, sec)`** : relatif à l'entrée dans l'état **source de la
  transition qui le porte** — jamais « depuis qu'une variable a changé »
  (voir §4.2 sur `palier0_stable`) ni « depuis le début du cycle » sauf si
  la transition est sourcée exactement sur l'état entré une fois par cycle
  (`SOURCE`, `NORMAL`).
- **Duplication assumée** : `PURGE`/`ALLUMAGE`/`REGULATION` existent en
  double (`MODE_H2`/`MODE_GPL`) — équivalent du paramètre `utiliseH2` de
  `gererCombustion()`, resté factorisé côté C++.
- **Reparentage programmatique non vérifié** : voir §0.

## 7. Invariant de configuration à respecter

`Duree_Max_Cycle >= Temps_Min_Fin` (les deux en minutes) doit être assuré
— idéalement dans `bornerConfig()` côté firmware. Sans cet invariant, la
transition `PROLONGATION → FIN_TEMPORISATION` (§5.3), qui ne re-teste pas
`Temps_Min_Fin` car elle le suppose déjà écoulé, pourrait théoriquement
laisser passer une fin de cycle légèrement avant `Temps_Min_Fin` réel. Cas
limite improbable (mauvaise configuration opérateur), signalé plutôt que
silencieusement ignoré.

## 8. Interface avec les sous-systèmes existants

- `CONVERSION_PUISSANCE` reçoit `V_Fl_1`/`V_Fl_2`/`V_Fl_3` (sorties du chart)
  et produit la puissance `u` injectée dans `MODELE_THERMIQUE` — aucune
  modification nécessaire pour le brancher au chart complété.
- `MODELE_THERMIQUE` produit `T_sec`, qui reboucle en entrée du chart —
  déjà câblé dans le `.slx` fourni.
