# Séchoir solaire hybride — Spécification Stateflow (Simulink)

Ce document complète `docs/FSM_SECHOIR.md` (firmware Arduino) pour le modèle
`Commande_Sechoir_Hybride.slx`. Les deux implémentent **la même machine à
états** — c'est volontaire : le mémoire doit pouvoir affirmer que le modèle
de simulation et le code embarqué reposent sur une seule et même spécification.

Les noms de variables suivent ceux **déjà présents dans le fichier `.slx`
fourni** (`Choix_Manuel`, `Mode_Auto`, `H_produit`, `T_cible`...) plutôt que
ceux du firmware Arduino (`Choix_Mode`, `H_produit_cible`...), pour ne pas
casser ce qui existe déjà. Un tableau de correspondance est donné en §6.

---

## 1. État des lieux du fichier fourni

Voir la synthèse critique donnée dans la conversation : la hiérarchie d'états
de premier niveau existe (`FONCTIONNEMENT_NORMAL` → 7 enfants, `URGENCE_ATEX`
en parallèle) ainsi que `CONVERSION_PUISSANCE` (100/67/33/0 %), mais :

- le sous-système Simulink « FSM » n'expose que 12 entrées alors que le
  chart Stateflow en attend 21 (mismatch de ports, **bloquant**) ;
- `Seuil_MQ8` / `Seuil_MQ6` sont utilisés sans être déclarés ;
- aucun état n'a d'action (`entry:`/`during:`/`exit:`) ;
- aucune sous-machine de combustion (purge/allumage/régulation) ;
- aucun état `ERREUR_COMBUSTION`, pas de retour depuis `URGENCE_ATEX`.

Le script `matlab/completer_fsm_sechoir.m` comble tout cela **via l'API
Stateflow**, pas par édition manuelle du XML.

---

## 2. Dictionnaire de données du chart `FSM/Chart`

### 2.1 Déjà présentes dans le `.slx` (conservées telles quelles)

| Nom | Scope | Type | Rôle |
|---|---|---|---|
| `Btn_Start`, `Mode_Auto`, `Choix_Manuel` | Input | uint8 | Démarrage, mode, source manuelle (1=Solaire,2=H2,3=GPL) |
| `Btn_UP`, `Btn_DOWN`, `Btn_SELECT`, `Btn_OK` | Input | uint8 | Menu (non modélisé en détail, voir §5) |
| `Btn_Prolongation`, `Tps_Prolongation` | Input | uint8 / double | **Existant, mais incohérent avec le firmware v2** — voir §7 AVIS n°1 |
| `H_initial`, `H_produit`, `T_cible`, `T_cap`, `T_amb`, `H_amb`, `T_sec`, `H_sec` | Input | double | Consignes et mesures |
| `Press_H2`, `MQ8_H2`, `MQ6_But`, `Flame`, `AU_Manuel` | Input | double/uint8 | Sécurité |
| `V_H2`, `V_But`, `V_Fl_1`, `V_Fl_2`, `V_Fl_3`, `Spark` | Output | uint8 | Actionneurs gaz |
| `PWM_Inj`, `PWM_Ext`, `PWM_Purge`, `Buzzer`, `Etat_LCD` | Output | uint8 | Ventilation, alarme, affichage |
| `T1_seuil`, `T2_seuil`, `Hhyst` (déf. 5), `H_fin`, `chrono_confirmation`, `Temps_Min_Fin` (déf. 7200) | Local | double | Régulation |

### 2.2 À ajouter (créées par le script)

| Nom | Scope | Type | Valeur par défaut | Rôle |
|---|---|---|---|---|
| `Btn_Stop` | Input | uint8 | — | **Absent du fichier fourni**, indispensable pour l'arrêt propre |
| `Seuil_MQ8`, `Seuil_MQ6` | Input | double | — | Utilisés sans être déclarés dans le fichier fourni |
| `Press_H2_Min` | Input | double | — | Pression H2 minimale (bascule GPL) |
| `T3_seuil` | Local | double | 0 | 3ᵉ seuil de palier, `= T_cible` (§4) |
| `T_init` | Local | double | 25 | Mode auto : lue sur `T_sec` au démarrage |
| `palier` | Local | uint8 | 0 | 0=100 %, 1=67 %, 2=33 %, 3=0 % |
| `Source_Active` | Local | uint8 | 0 | 0=Solaire, 1=H2, 2=GPL |
| `raison_purge` | Local | uint8 | 0 | 0=Démarrage,1=Échec,2=Basculement,3=Palier0 (voir §4.2) |
| `nb_echecs_allumage` | Local | uint16 | 0 | Compteur d'échecs consécutifs |
| `MAX_ECHECS` | Local (constante) | uint16 | 3 | Seuil d'escalade vers `ERREUR_COMBUSTION` |
| `Choix_Erreur` | Local | uint8 | 0 | 0=Réessayer,1=Manuel,2=Automatique |
| `arret_force_duree` | Local | uint8 | 0 | Indicateur d'arrêt forcé (Temps_Prolongation dépassé) |
| `Seuil_Tcap_ON`, `Seuil_Tcap_OFF` | Local | double | 55 / 45 | Disponibilité solaire |
| `Marge_Retour_H2` | Local | double | 0,5 | Anti-court-cycle GPL→H2 |
| `Temps_Arret_Auto` | Local | double | 300 | Délai de confirmation `FIN_TEMPORISATION` (s) |
| `Duree_Max_Cycle` | Local | double | 36000 | Durée avant `PROLONGATION` (s = 600 min) |
| `Temps_Purge` | Local | double | 120 | Durée de purge obligatoire (s) — **absent du fichier fourni, oubli initial corrigé** |
| `Temps_Allumage` | Local | double | 4 | Délai max de confirmation de flamme (s) — **idem** |

> **Simplification volontaire par rapport au firmware** : les timers
> (`Temps_Purge`, `Temps_Allumage`, `Temps_Min_Fin`, `Temps_Arret_Auto`,
> `Tps_Prolongation`, la confirmation du palier 0 %) utilisent les opérateurs
> **temporels natifs de Stateflow** (`after(N, sec)`) au lieu de variables
> `chrono_*` gérées à la main comme sur l'Arduino (qui n'a pas cette
> construction). C'est plus simple et tout aussi rigoureux — mais c'est une
> vraie différence d'implémentation entre le modèle et le firmware, à
> mentionner explicitement dans le mémoire.

---

## 3. Hiérarchie d'états complète

```
FONCTIONNEMENT_NORMAL (OR, déjà présent)
├── ATTENTE_DEMARRAGE            (déjà présent, vide → entry ajoutée)
├── MODE_SOLAIRE                 (déjà présent, vide → entry ajoutée)
├── MODE_H2                      (déjà présent → sous-machine ajoutée)
│     ├── PURGE                  [NOUVEAU, état initial]
│     ├── ALLUMAGE               [NOUVEAU]
│     └── REGULATION             [NOUVEAU] (englobe 100/67/33 via `palier`)
├── MODE_GPL                     (déjà présent → MÊME sous-machine, dupliquée)
│     ├── PURGE / ALLUMAGE / REGULATION   [NOUVEAU, identique à MODE_H2]
├── PROLONGATION                 (déjà présent, vide → entry ajoutée)
├── FIN_TEMPORISATION            (déjà présent, vide → entry/during ajoutées)
├── SECHAGE_TERMINE              (déjà présent, vide → entry ajoutée)
└── ERREUR_COMBUSTION            [NOUVEAU, sibling de premier niveau]

URGENCE_ATEX (déjà présent, vide → entry ajoutée + transition retour ajoutée)
```

**Sur la duplication PURGE/ALLUMAGE/REGULATION entre `MODE_H2` et
`MODE_GPL`** : Stateflow ne permet pas de « partager » littéralement une
région d'états entre deux super-états sans passer par une bibliothèque de
composants (hors périmètre ici). La duplication des **boîtes d'état** est
donc inévitable, mais la **logique** reste factorisée : les deux
sous-machines appellent les mêmes fonctions graphiques
`gerer_palier()`, `appliquer_palier()`, `fermer_gaz()` — c'est exactement
l'équivalent Stateflow de `gererCombustion()` dans le firmware, qui est
partagée par les deux modes via un paramètre booléen `utilise_h2`.

### 3.1 Sous-machine de combustion (identique dans MODE_H2 et MODE_GPL)

```
PURGE (état initial)
  entry: fermer_gaz(); Spark=0; PWM_Purge=255; PWM_Inj=60; PWM_Ext=200;
  → ALLUMAGE
      [after(Temps_Purge, sec) &&
       (raison_purge ~= 3 || T_sec < T3_seuil - Hhyst/2)]
      { if (raison_purge == 3) { palier = 2; } }   % réveil : repart à 33 %, jamais 100 %

ALLUMAGE
  entry: appliquer_palier(); Spark=1;
  → REGULATION
      [Flame == 1]
      { Spark=0; nb_echecs_allumage=0; }
  → ERREUR_COMBUSTION
      [after(Temps_Allumage, sec) && Flame==0 && raison_purge==2]
      { }                                            % échec de BASCULEMENT : escalade immédiate
  → PURGE
      [after(Temps_Allumage, sec) && Flame==0 && raison_purge~=2 && nb_echecs_allumage < MAX_ECHECS]
      { nb_echecs_allumage++; raison_purge=1; }
  → ERREUR_COMBUSTION
      [after(Temps_Allumage, sec) && Flame==0 && nb_echecs_allumage >= MAX_ECHECS]
      { nb_echecs_allumage++; }

REGULATION
  during: gerer_palier();     % hystérésis 4 niveaux, cf. §4.1 — mémorisée par `palier`
  → PURGE
      [Flame == 0]
      { nb_echecs_allumage++; raison_purge=1; }        % perte de flamme inattendue
  → ERREUR_COMBUSTION
      [Flame == 0 && nb_echecs_allumage >= MAX_ECHECS]
  → PURGE
      [palier == 3]                                     % consigne atteinte : coupure VOLONTAIRE
      { raison_purge=3; }
```

`gerer_palier()` (fonction graphique au niveau du chart) :

```matlab
function gerer_palier()
  demi = Hhyst/2;
  switch palier
    case 0   % 100 %
      if T_sec >= T1_seuil + demi, palier = 1; end
    case 1   % 67 %
      if T_sec >= T2_seuil + demi
        palier = 2;
      elseif T_sec < T1_seuil - demi
        palier = 0;
      end
    case 2   % 33 %
      if T_sec >= T3_seuil + demi
        palier = 3;                  % coupure volontaire, gérée par l'appelant
      elseif T_sec < T2_seuil - demi
        palier = 1;
      end
  end
  if palier ~= 3
    appliquer_palier();
  end
end

function appliquer_palier()
  V_Fl_1 = uint8(palier == 0);
  V_Fl_2 = uint8(palier == 0 || palier == 1);
  V_Fl_3 = uint8(palier ~= 3);
end

function fermer_gaz()
  V_H2=0; V_But=0; V_Fl_1=0; V_Fl_2=0; V_Fl_3=0; Spark=0;
end
```

### 3.2 État `ERREUR_COMBUSTION` [NOUVEAU]

```
entry: fermer_gaz(); Buzzer=1; PWM_Purge=255; Choix_Erreur=0;
during: if Btn_SELECT==1, Choix_Erreur = mod(Choix_Erreur+1, 3); end
→ (retour vers MODE_H2 ou MODE_GPL selon Source_Active)
    [Btn_OK==1 && (Choix_Erreur==0 || Choix_Erreur==2)]
    { if Choix_Erreur==2, Mode_Auto=1; end
      nb_echecs_allumage=0; raison_purge = (raison etait BASCULEMENT) ? 2 : 1; }
→ ATTENTE_DEMARRAGE
    [Btn_OK==1 && Choix_Erreur==1]
    { Mode_Auto=0; Buzzer=0; }
→ SECHAGE_TERMINE
    [Btn_Stop==1]
    { Buzzer=0; fermer_gaz(); }
```

### 3.3 États simples (entry uniquement)

| État | Action `entry:` |
|---|---|
| `ATTENTE_DEMARRAGE` | `fermer_gaz(); PWM_Purge=0; PWM_Inj=0; PWM_Ext=0; Buzzer=0; Etat_LCD=0;` |
| `MODE_SOLAIRE` | `fermer_gaz(); PWM_Purge=0; PWM_Inj=220; PWM_Ext=150; Etat_LCD=2;` |
| `PROLONGATION` | `Etat_LCD=5;` *(la régulation en cours n'est pas interrompue, cf. transitions §4)* |
| `FIN_TEMPORISATION` | `Etat_LCD=6;` — `during:` néant, sortie gérée par transitions temporelles |
| `SECHAGE_TERMINE` | `fermer_gaz(); PWM_Purge=0; PWM_Inj=0; PWM_Ext=90;` |
| `URGENCE_ATEX` | `fermer_gaz(); PWM_Purge=255; PWM_Inj=0; PWM_Ext=255; Buzzer=1; Etat_LCD=9;` |

### 3.4 Action `during:` du chart (niveau `FONCTIONNEMENT_NORMAL`)

```matlab
H_fin = H_produit + H_amb;     % recalculé en continu, jamais figé (§5 du CdC)
```

---

## 4. Transitions de premier niveau (bord de `FONCTIONNEMENT_NORMAL`, comme `URGENCE_ATEX`)

Même idiome que la transition d'urgence déjà présente dans le fichier :
dessinées depuis le **bord** de `FONCTIONNEMENT_NORMAL`, avec un ordre
d'exécution explicite (`executionOrder`) qui reproduit la priorité de
sécurité du firmware (§19 du cahier des charges).

| # | Source → Destination | Condition | Action |
|---|---|---|---|
| 1 | `FONCTIONNEMENT_NORMAL → URGENCE_ATEX` | *(déjà présent)* `MQ8_H2>=Seuil_MQ8 \|\| MQ6_But>=Seuil_MQ6 \|\| AU_Manuel==1` | — |
| 2 | `[in(MODE_SOLAIRE)\|in(MODE_H2)\|in(MODE_GPL)\|in(PROLONGATION)\|in(FIN_TEMPORISATION)] → SECHAGE_TERMINE` | `Btn_Stop==1` | `fermer_gaz();` |
| 3 | `[…] → SECHAGE_TERMINE` | `T_sec >= 90` | `fermer_gaz();` *(sécurité indépendante du palier 0 %)* |
| 4 | `PROLONGATION → SECHAGE_TERMINE` | `after(Tps_Prolongation, sec)` *(depuis l'entrée en PROLONGATION)* | `arret_force_duree=1; fermer_gaz();` |
| 5 | `[in(MODE_SOLAIRE)\|in(MODE_H2)\|in(MODE_GPL)\|in(PROLONGATION)] → FIN_TEMPORISATION` | `after(Temps_Min_Fin,sec) && (H_sec<=H_fin \|\| (palier==3 && after(60,sec)))` | — |
| 6 | `[…] → PROLONGATION` | `after(Duree_Max_Cycle,sec) && ~in(PROLONGATION)` | — |
| 7 | `URGENCE_ATEX → FONCTIONNEMENT_NORMAL` **[NOUVEAU]** | `Btn_OK==1 && MQ8_H2<Seuil_MQ8 && MQ6_But<Seuil_MQ6 && AU_Manuel==0` | `Buzzer=0;` |

### 4.1 Transitions d'arbitrage de source (depuis `ATTENTE_DEMARRAGE`, `MODE_SOLAIRE`, `MODE_H2`, `MODE_GPL`)

| Source → Destination | Condition | Action |
|---|---|---|
| `ATTENTE_DEMARRAGE → MODE_SOLAIRE` *(déjà présent : manuel)* | `Btn_Start==1 && Mode_Auto==0 && Choix_Manuel==1` | — |
| `ATTENTE_DEMARRAGE → MODE_H2` *(déjà présent : manuel)* | `Btn_Start==1 && Mode_Auto==0 && Choix_Manuel==2` | `palier=0; raison_purge=0;` |
| `ATTENTE_DEMARRAGE → MODE_GPL` **[NOUVEAU]** | `Btn_Start==1 && Mode_Auto==0 && Choix_Manuel==3` | `palier=0; raison_purge=0;` |
| `ATTENTE_DEMARRAGE → MODE_SOLAIRE` **[NOUVEAU, auto]** | `Btn_Start==1 && Mode_Auto==1 && T_cap>=Seuil_Tcap_ON` | `T_init=T_sec; calculer_seuils();` |
| `ATTENTE_DEMARRAGE → MODE_H2` **[NOUVEAU, auto]** | `Btn_Start==1 && Mode_Auto==1 && T_cap<Seuil_Tcap_OFF && Press_H2>=Press_H2_Min` | `T_init=T_sec; calculer_seuils(); palier=0; raison_purge=0;` |
| `ATTENTE_DEMARRAGE → MODE_GPL` **[NOUVEAU, auto]** | `Btn_Start==1 && Mode_Auto==1 && T_cap<Seuil_Tcap_OFF && Press_H2<Press_H2_Min` | `T_init=T_sec; calculer_seuils(); palier=0; raison_purge=0;` |
| `MODE_H2 → MODE_SOLAIRE` **[NOUVEAU]** | `Mode_Auto==1 && T_cap>=Seuil_Tcap_ON` | — |
| `MODE_GPL → MODE_SOLAIRE` **[NOUVEAU]** | `Mode_Auto==1 && T_cap>=Seuil_Tcap_ON` | — |
| `MODE_SOLAIRE → MODE_H2` **[NOUVEAU]** | `Mode_Auto==1 && T_cap<Seuil_Tcap_OFF && Press_H2>=Press_H2_Min` | `palier=0; raison_purge=0;` |
| `MODE_SOLAIRE → MODE_GPL` **[NOUVEAU]** | `Mode_Auto==1 && T_cap<Seuil_Tcap_OFF && Press_H2<Press_H2_Min` | `palier=0; raison_purge=0;` |
| `MODE_H2 → MODE_GPL` **[NOUVEAU]** | `Mode_Auto==1 && Press_H2<Press_H2_Min` | `raison_purge=2;` *(**`palier` n'est PAS remis à 0** : conservé, §4.2)* |
| `MODE_GPL → MODE_H2` **[NOUVEAU]** | `Mode_Auto==1 && Press_H2>=Press_H2_Min+Marge_Retour_H2` | `raison_purge=2;` |

> **AJOUT au-delà du fichier fourni** : le retour automatique GPL→H2 (avec
> marge anti-court-cycle) n'est décrit nulle part explicitement dans le
> cahier des charges v2 mais découle directement de la hiérarchie des
> sources (§2 : GPL = secours uniquement). Identique à l'AVIS n°5 du
> firmware.

### 4.2 `raison_purge` : rappel de la sémantique (identique au firmware)

| Valeur | Sens | Comportement en fin de `PURGE` |
|---|---|---|
| 0 | Démarrage à froid / bascule | Toujours vers `ALLUMAGE`, palier déjà mis à 100 % |
| 1 | Échec (allumage ou perte de flamme) | Toujours vers `ALLUMAGE`, palier inchangé |
| 2 | Basculement H2↔GPL | Toujours vers `ALLUMAGE`, palier **conservé** ; échec → `ERREUR_COMBUSTION` direct |
| 3 | Coupure volontaire (consigne atteinte) | Vers `ALLUMAGE` **seulement si** `T_sec` a rebaissé sous le seuil, sinon reste en veille |

---

## 5. Ce qui n'est volontairement PAS modélisé en Stateflow

- **`CONFIG_MENU`** : la saisie interactive des paramètres (boutons
  `Btn_UP/DOWN/SELECT/OK`) n'apporte rien à la validation du comportement
  thermique/combustion, qui est l'objet de ce modèle de simulation. Les
  paramètres opérateur (`T_cible`, `H_produit`, `Hhyst`, `Temps_Purge`...)
  sont donc de simples données réglables (blocs `Constant` ou variables du
  workspace MATLAB), comme c'est déjà le cas pour `Hhyst` et
  `Temps_Min_Fin` dans le fichier fourni. Si le mémoire doit aussi
  documenter/valider l'IHM, cela reste réalisable séparément (Stateflow le
  permet), mais ce n'est pas ajouté ici pour ne pas alourdir le modèle de
  commande.
- **Persistance EEPROM** : propre au firmware Arduino, sans équivalent utile
  dans une simulation Simulink (les paramètres sont simplement fixés avant
  simulation).

---

## 6. Table de correspondance des noms (Simulink ↔ Arduino)

| Simulink (`.slx`) | Arduino (`sechoir_hybride.ino`) |
|---|---|
| `Mode_Auto`, `Choix_Manuel` | `Config.Mode_Auto`, `Config.Choix_Mode` |
| `H_produit`, `H_initial` | `Config.H_produit_cible`, `Config.H_initial` |
| `T_cible`, `T_init` | `Config.T_cible`, `Config.T_init` |
| `T1_seuil`, `T2_seuil`, `T3_seuil` | `T1`, `T2`, `T3` |
| `V_Fl_1/2/3` | `EV1/EV2/EV3` |
| `PWM_Inj`, `PWM_Ext` | `PWM_Distrib`, `PWM_Extract` |
| `palier` (0/1/2/3) | `palier` (`PALIER_100/67/33/0`) |
| `raison_purge` | `raison_purge` (`RaisonPurge`) |
| `Tps_Prolongation` | `Config.Temps_Prolongation` |

---

## 7. Avis critiques (dans le même esprit que `FSM_SECHOIR.md`)

### AVIS n°1 — `Btn_Prolongation` déjà présent dans le `.slx` fourni

Le fichier fourni déclare une entrée `Btn_Prolongation`, alors que le
cahier des charges v1 demandait explicitement sa **suppression** (« la
prolongation est déclenchée automatiquement, pas par un bouton ») et que le
firmware v2 ne l'a pas réintroduite : `PROLONGATION` s'y déclenche
uniquement par dépassement de `Duree_Max_Cycle`. Cette entrée est **laissée
inutilisée** dans la spécification ci-dessus (aucune transition ne la lit),
plutôt que supprimée du chart — pour ne pas casser le fichier fourni sans
votre accord. Si vous confirmez qu'elle ne doit pas exister, je la retire
dans une prochaine passe.

### AVIS n°2 — Le script ne peut pas rewiring lui-même le sous-système Simulink

`completer_fsm_sechoir.m` complète le **chart Stateflow** (états, transitions,
données, fonctions). Il ne touche pas au câblage **Simulink** du
sous-système « FSM » (les fils entre les blocs `Constant`/`Inport` et le
chart), qui reste à faire à la main :

1. Ouvrir le sous-système `FSM`, puis le chart et son panneau **Symbols**
   (onglet Modeling → Symbols) : il liste maintenant toutes les entrées
   attendues (les 22 déjà présentes + `Btn_Stop`, `Seuil_MQ8`, `Seuil_MQ6`,
   `Press_H2_Min` ajoutées par le script).
2. Dans Simulink, faire **Ctrl+D** (Update Diagram) sur le modèle : pour de
   simples ajouts de ports, Simulink régénère souvent automatiquement les
   `Inport` manquants du sous-système — essayer ça en premier.
3. Si ça ne suffit pas : ajouter à la main les `Inport` manquants dans
   `FSM` (le nombre exact et l'ordre sont donnés par le panneau Symbols,
   pas besoin de les recompter soi-même), puis au niveau `system_root` :
   remplacer les blocs `Constant` de test par des sources adaptées (autre
   `Constant`, ou blocs `From Workspace` si vous voulez rejouer un
   scénario/chronogramme réel pour la validation du mémoire).

### AVIS n°3 — Cohérence du critère de fin de cycle

Le fichier fourni a déjà `chrono_confirmation` et `Temps_Min_Fin=7200`
(2 h), cohérent avec le §5 du cahier des charges. Le critère secondaire
« température atteinte durablement » est implémenté ici comme
`palier==3 && after(60,sec)` — mêmes réserves que dans le firmware (voir
`FSM_SECHOIR.md` §1.2) : ce n'est pas un critère de qualité produit, juste
un indicateur que la régulation a atteint son point de fonctionnement.
