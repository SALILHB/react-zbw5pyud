# Séchoir solaire hybride — Modèle Simulink/Stateflow de la commande

Ce document est la spécification complète de la partie **FSM** du modèle
`Commande_Sechoir_Hybride.slx` (le sous-système « FSM », son chart
Stateflow, ses données et ses transitions). Il est écrit pour **correspondre
exactement au firmware Arduino** `sechoir_hybride/sechoir_hybride.ino`
(§10 v2 de ce dernier) : c'est le principe de la conception basée sur les
modèles (*Model-Based Design*) — le chart Stateflow est la spécification
exécutable dont le code embarqué est dérivé, les deux doivent donc décrire
rigoureusement la même machine à états.

Les sous-systèmes `CONVERSION_PUISSANCE` (palier → puissance) et
`MODELE_THERMIQUE` (réponse thermique de la chambre) existent déjà dans le
modèle et ne sont pas repris ici, sauf pour rappeler leur interface avec le
chart FSM (§7).

Ce document sert de référence à `matlab/completer_fsm_sechoir.m`, qui
construit automatiquement tout ce qui suit via l'API Stateflow. Si le script
bute sur une limitation d'API dans votre version de MATLAB, ce document
contient tout le nécessaire pour compléter le chart **à la main** dans
l'éditeur Stateflow.

---

## 1. Hiérarchie d'états

```
FSM/Chart
│
├── FONCTIONNEMENT_NORMAL (OR-state, super-état englobant)
│     during: H_fin = H_produit + H_amb;      (recalcule en continu, §5 du firmware)
│     │
│     ├── ATTENTE_DEMARRAGE        (état initial, tout fermé)
│     ├── MODE_SOLAIRE             (passif : aucune electrovanne)
│     ├── MODE_H2                  ┐
│     │     ├── PURGE              │ sous-machine de combustion,
│     │     ├── ALLUMAGE           │ IDENTIQUE dans MODE_H2 et MODE_GPL
│     │     └── REGULATION         │ (palier 100/67/33/0 %, §4.1 du firmware)
│     ├── MODE_GPL                 ┘
│     │     ├── PURGE
│     │     ├── ALLUMAGE
│     │     └── REGULATION
│     ├── PROLONGATION             (Duree_Max_Cycle dépassée, régulation poursuivie)
│     ├── FIN_TEMPORISATION        (demande d'arrêt : confirmation ou arrêt auto)
│     └── SECHAGE_TERMINE          (cycle terminé, normal ou forcé)
│
├── ERREUR_COMBUSTION       (sibling de FONCTIONNEMENT_NORMAL : basculement raté
│                            ou échecs d'allumage répétés, menu REESSAYER/
│                            MANUEL/AUTOMATIQUE)
│
└── URGENCE_ATEX            (sibling PARALLÈLE et PRIORITAIRE, accessible
                             depuis n'importe quel état de FONCTIONNEMENT_NORMAL)
```

Correspondance avec le firmware : chaque état ci-dessus porte exactement le
nom de la valeur `enum EtatFSM` correspondante dans `sechoir_hybride.ino`
(section 4). `PURGE`/`ALLUMAGE`/`REGULATION` correspondent aux valeurs de
`enum PhaseCombustion` (section 5) — `REGULATION` regroupe ici
`PH_PALIER_100`/`PH_PALIER_67`/`PH_PALIER_33`, la distinction entre les trois
étant portée par la variable `palier`, pas par des sous-états séparés (même
simplification que dans `gererCombustion()`).

---

## 2. Dictionnaire de données du chart

Tous les noms ci-dessous sont ceux **déjà utilisés dans `sechoir_hybride.ino`**
(sans le préfixe `Config.` — le chart Stateflow n'a pas de notion de
structure C, chaque champ devient une donnée du chart).

### 2.1 Entrées (`Scope = Input`)

| Nom | Type | Correspond à |
|---|---|---|
| `Btn_Start` | uint8 | `frontPris(B_START)` |
| `Btn_Stop` | uint8 | `frontPris(B_STOP)` |
| `Btn_OK` | uint8 | `frontPris(B_OK)` |
| `Mode_Auto` | uint8 | `Config.Mode_Auto` |
| `Choix_Manuel` | uint8 | `Config.Choix_Mode` (1=Solaire, 2=H2, 3=GPL) |
| `T_sec`, `H_sec` | double | `T_sec`, `H_sec` (mesures chambre) |
| `T_cap`, `T_amb`, `H_amb` | double | `T_cap`, `T_amb`, `H_amb` |
| `T_cible` | double | `Config.T_cible` |
| `H_produit` | double | `Config.H_produit_cible` |
| `Press_H2` | double | `Press_H2` |
| `Press_H2_Min` | double | `Config.Press_H2_Min` |
| `MQ8_H2`, `MQ6_But` | double | `MQ8_H2`, `MQ6_But` |
| `Seuil_MQ8`, `Seuil_MQ6` | double | `Config.Seuil_MQ8`, `Config.Seuil_MQ6` |
| `Flame` | uint8 | `Flame` |
| `AU_Manuel` | uint8 | `AU_Urgence` |

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
| `T_init` | double | 25 | `Config.T_init` |
| `Hhyst` | double | 5 | `Config.Hhyst` |
| `H_fin` | double | 0 | `H_fin` (recalculé en continu, cf. §1) |
| `palier` | uint8 | 0 | `palier` (0=100 %, 1=67 %, 2=33 %, 3=0 %) |
| `Source_Active` | uint8 | 0 | `Source_Active` (0=Solaire, 1=H2, 2=GPL) |
| `raison_purge` | uint8 | 0 | `raison_purge` (0=Démarrage, 1=Échec, 2=Basculement, 3=Palier0) |
| `nb_echecs_allumage` | uint16 | 0 | `nb_echecs_allumage` |
| `MAX_ECHECS` | uint16 | 3 | `MAX_ECHECS_AVANT_ALARME` |
| `Choix_Erreur` | uint8 | 0 | `Choix_Erreur` (0=Réessayer, 1=Manuel, 2=Automatique) |
| `arret_force_duree` | uint8 | 0 | `arret_force_duree` |
| `Seuil_Tcap_ON`, `Seuil_Tcap_OFF` | double | 55 / 45 | `SEUIL_T_CAP_SOLAIRE_ON/OFF` |
| `Marge_Retour_H2` | double | 0,5 | `MARGE_RETOUR_H2` |
| `Temps_Purge` | double | 120 | `Config.Temps_Purge` (s) |
| `Temps_Allumage` | double | 4 | `Config.Temps_Allumage` (s) |
| `Temps_Min_Fin` | double | 7200 | `Config.Temps_Min_Fin` (s) — **déjà présent dans le `.slx` fourni** |
| `Temps_Arret_Auto` | double | 300 | `Config.Temps_Arret_Auto` (s) |
| `Tps_Prolongation` | double | — | `Config.Temps_Prolongation` (min) — **entrée déjà présente**, utilisée en minutes (`after(Tps_Prolongation,sec)` compare donc en secondes : voir remarque §5) |
| `Duree_Max_Cycle` | double | 36000 | `Config.Duree_Max_Cycle` (s) |

> **Remarque unité `Tps_Prolongation`** : le firmware la stocke en minutes
> mais compare en millisecondes (`Config.Temps_Prolongation * 60000UL`).
> Le chart utilise l'opérateur temporel natif `after(Tps_Prolongation, sec)`,
> qui compte en **secondes** — il faut donc soit convertir
> (`after(Tps_Prolongation*60, sec)`), soit stocker `Tps_Prolongation`
> directement en secondes côté Simulink. Le script utilise la seconde
> option pour rester cohérent avec `Duree_Max_Cycle`/`Temps_Purge`/etc.,
> qui sont toutes en secondes — **à corriger dans le bloc `Constant`
> `Tcible`/`Tps_Prolongation` du diagramme si vous branchez une vraie
> valeur en minutes.**

---

## 3. Fonctions partagées — **inlinées**, pas de `Stateflow.EMLFunction`

Un premier essai factorisait `gerer_palier()` / `appliquer_palier()` /
`fermer_gaz()` / `calculer_seuils()` en fonctions Stateflow séparées.
`Stateflow.EMLFunction` s'est révélée indisponible dans la session MATLAB
réelle testée. La logique est donc **écrite directement** dans chaque
action d'état/transition qui en a besoin, dupliquée entre `MODE_H2` et
`MODE_GPL` (exactement l'équivalent du paramètre booléen `utiliseH2` de
`gererCombustion()` dans le firmware, qui lui reste factorisé côté C++).

Blocs de texte réutilisés tels quels (mêmes noms que dans le script) :

**`fermer_gaz`** (§7.3 firmware : fermeture complète du circuit gaz) :
```matlab
V_H2=uint8(0); V_But=uint8(0); V_Fl_1=uint8(0); V_Fl_2=uint8(0); V_Fl_3=uint8(0); Spark=uint8(0);
```

**`appliquer_palier`** (§10 firmware : palier → état des 3 électrovannes) :
```matlab
V_Fl_1=uint8(palier==0); V_Fl_2=uint8(palier==0||palier==1); V_Fl_3=uint8(palier~=3);
```

**`calculer_seuils`** (§4.3 firmware) :
```matlab
ECART_MIN=3;
if T_cible<=T_init+ECART_MIN
  T1_seuil=T_cible; T2_seuil=T_cible; T3_seuil=T_cible;
else
  delta=T_cible-T_init;
  T1_seuil=T_init+delta/3; T2_seuil=T_init+2*delta/3; T3_seuil=T_cible;
end
```

**`gerer_palier`** (§4.2 firmware : hystérésis 4 niveaux mémorisée par
`palier`) :
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

---

## 4. Code de chaque état

### 4.1 États simples (entry uniquement)

| État | `entry:` |
|---|---|
| `ATTENTE_DEMARRAGE` | `fermer_gaz` `PWM_Purge=uint8(0); PWM_Inj=uint8(0); PWM_Ext=uint8(0); Buzzer=uint8(0); Etat_LCD=uint8(0);` |
| `MODE_SOLAIRE` | `fermer_gaz` `PWM_Purge=uint8(0); PWM_Inj=uint8(220); PWM_Ext=uint8(150); Etat_LCD=uint8(2);` |
| `PROLONGATION` | `Etat_LCD=uint8(5);` |
| `FIN_TEMPORISATION` | `Etat_LCD=uint8(6);` |
| `SECHAGE_TERMINE` | `fermer_gaz` `PWM_Purge=uint8(0); PWM_Inj=uint8(0); PWM_Ext=uint8(90);` |
| `URGENCE_ATEX` | `fermer_gaz` `PWM_Purge=uint8(255); PWM_Inj=uint8(0); PWM_Ext=uint8(255); Buzzer=uint8(1); Etat_LCD=uint8(9);` |

(`fermer_gaz` = le bloc de texte défini en §3, concaténé littéralement.)

`FONCTIONNEMENT_NORMAL` (le super-état lui-même) :
```
during: H_fin = H_produit + H_amb;
```

### 4.2 Sous-machine de combustion (`MODE_H2` et `MODE_GPL`, identique)

Pour `MODE_H2` : `actionVanne = 'V_H2=uint8(1); V_But=uint8(0);'`
Pour `MODE_GPL` : `actionVanne = 'V_H2=uint8(0); V_But=uint8(1);'`

```
PURGE  (état initial de la sous-machine, transition par defaut sans source)
  entry: fermer_gaz  Spark=uint8(0); PWM_Purge=uint8(255); PWM_Inj=uint8(60); PWM_Ext=uint8(200);

ALLUMAGE
  entry: <actionVanne> appliquer_palier  Spark=uint8(1);

REGULATION
  entry: <actionVanne>
  during: gerer_palier
```

### 4.3 `ERREUR_COMBUSTION` (sibling de `FONCTIONNEMENT_NORMAL`)

```
entry: fermer_gaz  Buzzer=uint8(1); PWM_Purge=uint8(255); Choix_Erreur=uint8(0);
during: if Btn_SELECT==1, Choix_Erreur = mod(Choix_Erreur+1, uint8(3)); end
```

> Le firmware utilise `Btn_Menu` pour faire défiler les 3 choix
> (`CHOIX_ERR_REESSAYER`/`MANUEL`/`AUTOMATIQUE`) — le chart utilise
> `Btn_SELECT`, seul bouton de navigation déjà déclaré dans le `.slx`
> fourni ; à renommer si vous préférez garder `Btn_Menu` partout par
> cohérence stricte avec le firmware.

---

## 5. Table complète des transitions

### 5.1 Sous-machine de combustion (communes à MODE_H2/MODE_GPL, portées par le chart)

| Source → Destination | Condition | Action |
|---|---|---|
| *(défaut)* → `PURGE` | — | — |
| `PURGE` → `ALLUMAGE` | `after(Temps_Purge,sec) && (raison_purge~=3 \|\| T_sec<T3_seuil-Hhyst/2)` | `if(raison_purge==3){palier=2;}` |
| `ALLUMAGE` → `REGULATION` | `Flame==1` | `Spark=0; nb_echecs_allumage=0;` |
| `ALLUMAGE` → `ERREUR_COMBUSTION` | `after(Temps_Allumage,sec) && Flame==0 && raison_purge==2` | — *(échec de bascule : escalade immédiate)* |
| `ALLUMAGE` → `PURGE` | `after(Temps_Allumage,sec) && Flame==0 && raison_purge~=2 && nb_echecs_allumage<MAX_ECHECS` | `nb_echecs_allumage++; raison_purge=1;` |
| `ALLUMAGE` → `ERREUR_COMBUSTION` | `after(Temps_Allumage,sec) && Flame==0 && nb_echecs_allumage>=MAX_ECHECS` | `nb_echecs_allumage++;` |
| `REGULATION` → `PURGE` | `Flame==0 && nb_echecs_allumage<MAX_ECHECS` | `nb_echecs_allumage++; raison_purge=1;` |
| `REGULATION` → `ERREUR_COMBUSTION` | `Flame==0 && nb_echecs_allumage>=MAX_ECHECS` | `nb_echecs_allumage++;` |
| `REGULATION` → `PURGE` | `palier==3` *(coupure volontaire, consigne atteinte)* | `raison_purge=3;` |

### 5.2 `ERREUR_COMBUSTION`

| Destination | Condition | Action |
|---|---|---|
| `MODE_H2` | `Btn_OK==1 && (Choix_Erreur==0\|\|Choix_Erreur==2) && Source_Active==1` | `if Choix_Erreur==2, Mode_Auto=1; end nb_echecs_allumage=0; raison_purge=2;` |
| `MODE_GPL` | `Btn_OK==1 && (Choix_Erreur==0\|\|Choix_Erreur==2) && Source_Active==2` | *(idem)* |
| `ATTENTE_DEMARRAGE` | `Btn_OK==1 && Choix_Erreur==1` | `Mode_Auto=0; Buzzer=0;` |
| `SECHAGE_TERMINE` | `Btn_Stop==1` | `Buzzer=0;` `fermer_gaz` |

### 5.3 Transitions prioritaires (bord de `FONCTIONNEMENT_NORMAL`)

Même idiome que la transition d'urgence déjà présente dans le `.slx` fourni
(dessinée depuis le bord du super-état, pas depuis chaque enfant un par un).

| Destination | Condition | Action |
|---|---|---|
| *(déjà présent)* `URGENCE_ATEX` | `MQ8_H2>=Seuil_MQ8 \|\| MQ6_But>=Seuil_MQ6 \|\| AU_Manuel==1` | — |
| `SECHAGE_TERMINE` | `(in(MODE_SOLAIRE)\|in(MODE_H2)\|in(MODE_GPL)\|in(PROLONGATION)\|in(FIN_TEMPORISATION)) && Btn_Stop==1` | `fermer_gaz` |
| `SECHAGE_TERMINE` | `(in(MODE_SOLAIRE)\|in(MODE_H2)\|in(MODE_GPL)\|in(PROLONGATION)\|in(FIN_TEMPORISATION)) && T_sec>=90` | `fermer_gaz` *(sécurité surchauffe, indépendante du palier 0 %)* |
| `SECHAGE_TERMINE` (depuis `PROLONGATION`) | `after(Tps_Prolongation,sec)` | `arret_force_duree=1;` `fermer_gaz` |
| `FIN_TEMPORISATION` | `(in(MODE_SOLAIRE)\|in(MODE_H2)\|in(MODE_GPL)\|in(PROLONGATION)) && after(Temps_Min_Fin,sec) && (H_sec<=H_fin \|\| (palier==3 && after(60,sec)))` | — |
| `SECHAGE_TERMINE` (depuis `FIN_TEMPORISATION`) | `Btn_OK==1 \|\| Btn_Stop==1` | `fermer_gaz` *(confirmation opérateur)* |
| `SECHAGE_TERMINE` (depuis `FIN_TEMPORISATION`) | `after(Temps_Arret_Auto,sec)` | `fermer_gaz` *(arrêt automatique)* |
| `PROLONGATION` | `(in(MODE_SOLAIRE)\|in(MODE_H2)\|in(MODE_GPL)) && after(Duree_Max_Cycle,sec)` | — |
| `FONCTIONNEMENT_NORMAL` (depuis `URGENCE_ATEX`) | `Btn_OK==1 && MQ8_H2<Seuil_MQ8 && MQ6_But<Seuil_MQ6 && AU_Manuel==0` | `Buzzer=0;` *(réarmement)* |

### 5.4 Arbitrage de source (mode automatique)

| Source → Destination | Condition | Action |
|---|---|---|
| *(déjà présent)* `ATTENTE_DEMARRAGE` → `MODE_SOLAIRE` | `Btn_Start==1 && Mode_Auto==0 && Choix_Manuel==1` | — |
| *(déjà présent)* `ATTENTE_DEMARRAGE` → `MODE_H2` | `Btn_Start==1 && Mode_Auto==0 && Choix_Manuel==2` | — |
| `ATTENTE_DEMARRAGE` → `MODE_GPL` | `Btn_Start==1 && Mode_Auto==0 && Choix_Manuel==3` | `palier=0; raison_purge=0;` |
| `ATTENTE_DEMARRAGE` → `MODE_SOLAIRE` | `Btn_Start==1 && Mode_Auto==1 && T_cap>=Seuil_Tcap_ON` | `T_init=T_sec;` `calculer_seuils` |
| `ATTENTE_DEMARRAGE` → `MODE_H2` | `Btn_Start==1 && Mode_Auto==1 && T_cap<Seuil_Tcap_OFF && Press_H2>=Press_H2_Min` | `T_init=T_sec;` `calculer_seuils` `palier=0; raison_purge=0;` |
| `ATTENTE_DEMARRAGE` → `MODE_GPL` | `Btn_Start==1 && Mode_Auto==1 && T_cap<Seuil_Tcap_OFF && Press_H2<Press_H2_Min` | *(idem)* |
| `MODE_H2` → `MODE_SOLAIRE` | `Mode_Auto==1 && T_cap>=Seuil_Tcap_ON` | — |
| `MODE_GPL` → `MODE_SOLAIRE` | `Mode_Auto==1 && T_cap>=Seuil_Tcap_ON` | — |
| `MODE_SOLAIRE` → `MODE_H2` | `Mode_Auto==1 && T_cap<Seuil_Tcap_OFF && Press_H2>=Press_H2_Min` | `palier=0; raison_purge=0;` |
| `MODE_SOLAIRE` → `MODE_GPL` | `Mode_Auto==1 && T_cap<Seuil_Tcap_OFF && Press_H2<Press_H2_Min` | *(idem)* |
| `MODE_H2` → `MODE_GPL` | `Mode_Auto==1 && Press_H2<Press_H2_Min` | `raison_purge=2;` *(palier **conservé**)* |
| `MODE_GPL` → `MODE_H2` | `Mode_Auto==1 && Press_H2>=Press_H2_Min+Marge_Retour_H2` | `raison_purge=2;` *(retour auto, marge anti-court-cycle)* |

---

## 6. Notes d'implémentation Stateflow

- **Priorité/ordre d'exécution** : quand plusieurs transitions partent du
  même état/bord, Stateflow les évalue dans l'ordre d'ancienneté de
  création sauf `ExecutionOrder` explicite. Le script fixe cet ordre en
  créant les transitions dans l'ordre du firmware (§19 : urgence > arrêt
  propre > surchauffe > plafond prolongation > fin de cycle > durée max).
  Si vous complétez à la main, respectez cet ordre ou fixez
  `ExecutionOrder` directement dans l'inspecteur.
- **`in(NomEtat)`** : opérateur Stateflow natif, syntaxe non quotée
  (`in(MODE_H2)`, pas `in('MODE_H2')`).
- **`after(N, sec)`** : temporisation native Stateflow, relative à l'entrée
  dans l'état source de la transition — remplace les `chrono_*` gérés à la
  main en C (le firmware doit le faire lui-même car il n'a pas cette
  construction ; le modèle Simulink, si.
- **Duplication assumée** : `PURGE`/`ALLUMAGE`/`REGULATION` existent en
  double (dans `MODE_H2` et dans `MODE_GPL`) car Stateflow ne permet pas de
  partager littéralement une région d'états entre deux super-états sans
  bibliothèque de composants. La logique reste identique aux textes
  d'action près (`actionVanne`) — c'est l'équivalent du paramètre
  `utiliseH2` de `gererCombustion()`.

## 7. Interface avec les sous-systèmes existants

- `CONVERSION_PUISSANCE` reçoit `V_Fl_1`/`V_Fl_2`/`V_Fl_3` (sorties du chart)
  et produit la puissance `u` injectée dans `MODELE_THERMIQUE` — aucune
  modification nécessaire pour le brancher au chart complété : les noms de
  sortie du chart correspondent déjà à ses entrées.
- `MODELE_THERMIQUE` produit `T_sec`, qui reboucle en entrée du chart —
  déjà câblé dans le `.slx` fourni (screenshot).
