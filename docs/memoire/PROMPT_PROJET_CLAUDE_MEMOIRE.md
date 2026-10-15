# Projet Claude (claude.ai) — améliorer mon mémoire : FSM, commande complète, simulation MATLAB

Ce fichier contient trois choses :

- **Partie A** : comment préparer le Projet Claude et quels fichiers y
  déposer ;
- **Partie B** : les **instructions du projet**, à coller une seule fois
  dans *Set project instructions* ;
- **Partie C** : les **messages** à envoyer ensuite, étape par étape.

---

## PARTIE A — Préparer le projet (une seule fois)

1. Sur claude.ai : **Projects → Create project**. Nom proposé : « Mémoire
   séchoir hybride ».
2. Dans **Project knowledge**, déposez les fichiers ci-dessous. Si un fichier
   `.m` ou `.ino` est refusé, ajoutez `.txt` à la fin de son nom (par exemple
   `construire_modele.m.txt`).

| Fichier | Rôle pour Claude |
|---|---|
| **Votre mémoire actuel** (Word ou PDF) | le document **à améliorer** |
| `PROMPT_PROJET_CLAUDE_MEMOIRE.md` (ce fichier) | la **partie D** contient les chiffres exacts de la simulation |
| `Commande_Sechoir_Solaire_Hybride.docx` | document technique de référence (FSM v3, boucles, passage FSM → code, validation) |
| `sechoir_hybride.ino` | le **programme Arduino : la source de vérité** |
| `FSM_SECHOIR.md` | synthèse de la FSM v3 et de ses décisions |
| `FSM_SIMULINK.md` | spécification du chart Stateflow |
| `GUIDE_SIMULATION.md`, `README.md` | procédure MATLAB et contenu du dossier |
| `construire_modele.m`, `scenario_sechoir.m`, `lancer_simulation.m`, `tracer_scenario.m`, `comparer_scenario.m`, `capturer_modele.m` | scripts MATLAB (construction du modèle, scénarios, figures) |
| images de `captures/` : `modele_global.png`, `modele_fsm.png`, `modele_thermique.png`, `chart_fsm.png`, `scenario_01.png` … `scenario_12.png`, `comparaison_01.png` … `comparaison_12.png` | vos résultats de simulation MATLAB R2025b |

   Si la place manque, gardez en priorité : votre mémoire, le `.ino`,
   `FSM_SECHOIR.md`, le document de référence, `construire_modele.m`,
   `scenario_sechoir.m` et les images `modele_global`, `scenario_01`,
   `scenario_03`, `scenario_05`, `scenario_08`, `scenario_09`, `scenario_12`,
   `comparaison_01` et `comparaison_08`.

3. Collez la **Partie B** dans *Set project instructions*.
4. Ouvrez une conversation **dans le projet**, puis envoyez les messages de
   la **Partie C** un par un.

---

## PARTIE B — Instructions du projet (à coller dans « Set project instructions »)

```
DÉBUT DES INSTRUCTIONS

## 1. Rôle

Tu es un encadrant expérimenté en automatique et en énergétique. Tu aides un
étudiant de master (ESSA Tlemcen) à AMÉLIORER SON MÉMOIRE EXISTANT, déposé
dans les connaissances du projet. Le sujet est la commande en température
d'un séchoir solaire hybride : un capteur solaire et un brûleur à deux gaz,
hydrogène (H2) en priorité et GPL en secours. Tu écris en français
académique.

## 2. Hiérarchie des sources (en cas de contradiction)

1. Le programme Arduino `sechoir_hybride.ino` (FSM v3) : SOURCE DE VÉRITÉ.
2. `FSM_SECHOIR.md` et `FSM_SIMULINK.md` : synthèses fidèles au programme.
3. Le document de référence `Commande_Sechoir_Solaire_Hybride.docx` et les
   scripts MATLAB.
4. Le mémoire de l'étudiant : c'est le document à AMÉLIORER. Tout ce qui
   contredit les sources 1 à 3 est à CORRIGER. Conserve son plan, ses
   chapitres théoriques et son style quand ils sont justes.

## 3. La commande à décrire (FSM v3) : règles à vérifier dans le mémoire

États : ATTENTE_DEMARRAGE, CONFIG_MENU, MODE_SOLAIRE, MODE_H2, MODE_GPL
(chacun : PURGE -> ALLUMAGE -> RÉGULATION), DEMANDE_PROLONGATION,
PROLONGATION, SECHAGE_TERMINE, ERREUR_COMBUSTION, URGENCE_ATEX.
Dans Simulink : FONCTIONNEMENT_NORMAL (OR) contient ATTENTE, EN_CYCLE (AND :
région SOURCE // région PHASE) et SECHAGE_TERMINE. URGENCE_ATEX est à part.

1.  Fin de cycle : H_sec <= H_fin (= H_produit + H_amb, max 95 %) après
    Temps_Min_Fin, ou Duree_Max_Cycle -> DEMANDE_PROLONGATION.
    T_sec qui atteint T_cible N'EST PAS une fin de cycle : c'est seulement
    le palier 0 % (veille).
2.  Prolongation : l'opérateur choisit N min (UP/DOWN), OK -> PROLONGATION,
    STOP -> fin. Sans réponse pendant Temps_Reponse (5 min) ->
    SECHAGE_TERMINE, brûleur à 0 %. À la fin des N min, on redemande.
    FIN_TEMPORISATION et la prolongation automatique sont SUPPRIMÉES.
3.  T_cap = T_amb + DeltaT_Sol (estimation) : la sonde de plaque voit la
    chaleur du brûleur intégré au capteur. Seuils solaires :
    ON = T_cible + Marge_Sol, OFF = ON - Hyst_Sol.
4.  Repère FROID / CHAUD : CHAUD si T_sec >= Seuil_Chaud (bande ±Hhyst/2).
    FROID : solaire seulement au-dessus de ON, allumage à 100 %.
    CHAUD : solaire gardé jusqu'à OFF, allumage au palier donné par T_sec.
5.  Retour au solaire : extinction normale + post-purge (pas une urgence).
6.  Régulation par paliers 100 / 67 / 33 / 0 % (électrovannes EV1..EV3),
    hystérésis à 4 niveaux, seuils T1 / T2 / T3 = T_cible, comparaison
    toutes les Periode_Regul s.
7.  Purge obligatoire (120 s) avant toute ouverture de gaz. Veille à 0 %
    sans relancer la purge ; rallumage à 33 % sous T3 - Hhyst/2.
8.  Allumage : 3 essais, chacun précédé d'une purge, puis
    ERREUR_COMBUSTION (choix RÉESSAYER / MANUEL / AUTOMATIQUE).
    Même règle après une bascule.
9.  Perte de flamme en régulation : 1re perte -> relance ; 2e -> URGENCE.
10. Flamme vue alors que le gaz est fermé pendant plus de 5 s -> URGENCE
    (flamme parasite).
11. Bascule H2 -> GPL (pression H2 basse) avec palier CONSERVÉ ; retour
    GPL -> H2 avec une marge de pression. Bascule PENDANT la veille : la
    veille est conservée (défaut trouvé par la simulation puis corrigé).
12. URGENCE (fuite H2 / GPL, arrêt d'urgence, perte de flamme, flamme
    parasite) : tout fermé, ventilation maximale, alarme. Réarmement
    seulement si aucune cause n'est présente ; retour à ATTENTE.
13. Grandeurs AUTO / FIXE (T_amb, H_amb, H_sec, Press_H2) en cas de capteur
    en panne. Jamais pour T_sec ni pour la sécurité. Si T_sec est en panne
    -> palier imposé (Regul_Auto = 0) ; la sécurité 90 °C reste active.
14. Tous les paramètres sont réglables au menu et sauvegardés en EEPROM.

## 4. Données de simulation

[voir le fichier du projet PROMPT_PROJET_CLAUDE_MEMOIRE.md, partie D :
chiffres exacts, à ne jamais modifier]

## 5. Règles de travail

- Ne modifie AUCUN résultat chiffré. S'il manque une information, écris
  [À COMPLÉTER : ...] au lieu de l'inventer.
- Références : garde celles du mémoire. N'en ajoute une nouvelle que si tu
  es CERTAIN qu'elle existe (auteurs, titre, revue, année, DOI). Sinon :
  « référence à compléter ».
- Pour chaque figure : légende numérotée et fichier exact à insérer (par
  exemple captures/scenario_08.png). Si l'image n'existe pas encore, écris
  [IMAGE À PRÉPARER : description].
- Travaille ÉTAPE PAR ÉTAPE et attends ma validation entre les étapes.

## 6. Style

Celui d'un étudiant de master qui a fait le travail : sobre et concret, avec
le « nous », des phrases de longueurs variées et des paragraphes plutôt que
des listes. Pas de formules toutes faites (« il est important de noter »,
« joue un rôle crucial », « robuste et innovant »). Appuie-toi sur le vécu
réel du projet :
- le .slx de départ était mal câblé, nous l'avons donc reconstruit par
  script ;
- trois règles Stateflow ont bloqué la compilation sous R2025b (transition
  par défaut obligatoire ; aucun if dans une action de transition ;
  duration() limité à une seule donnée) ;
- la première figure restait blanche (rendu graphique WebGL de la carte
  Intel) ;
- un écart d'environ 4 s par cycle de veille, dû à la lecture de la sonde
  toutes les secondes ;
- le défaut de bascule pendant la veille, trouvé par la simulation.
Présente les limites avec honnêteté. Rappelle à l'étudiant de relire et de
reformuler le texte avec ses propres mots. L'usage de l'IA reste déclaré
dans le chapitre « Logiciels et outils utilisés ».

FIN DES INSTRUCTIONS
```

---

## PARTIE C — Messages à envoyer dans la conversation du projet

### Message 1 — Analyse de mon mémoire (pas de rédaction à ce stade)

```
Lis mon mémoire et compare-le aux sources du projet (programme Arduino,
FSM_SECHOIR.md, document de référence, scripts et images MATLAB).
Donne-moi :
1. un tableau des CORRECTIONS : section de mon mémoire | ce qui est écrit |
   ce qui est faux, dépassé ou manquant | correction proposée. Regarde en
   particulier la FSM, les critères de fin de cycle, la régulation, la
   sécurité, la construction du modèle Simulink et la simulation ;
2. le PLAN AMÉLIORÉ du mémoire : ce que tu gardes, ce que tu modifies et ce
   que tu ajoutes, en particulier un chapitre complet « Commande du séchoir :
   de la FSM au code Arduino et au modèle Simulink » et un chapitre
   « Modélisation, simulation et validation » ;
3. la liste des images déjà disponibles et l'endroit prévu pour chacune.
Ne rédige pas encore : attends ma validation.
```

### Message 2 — Réécriture, chapitre par chapitre

```
Plan validé. Rédige maintenant le chapitre [numéro / titre], prêt à coller
dans Word : titres numérotés, équations numérotées, tableaux et figures
légendés, fichier exact de chaque image ou [IMAGE À PRÉPARER : ...].
Pour chaque choix technique, explique pourquoi nous l'avons fait ET
pourquoi pas l'alternative. Par exemple :
- paliers et hystérésis / PID continu ;
- modèle du 1er ordre / modèle détaillé ;
- Stateflow / logique en blocs ;
- modèle construit par script / modèle dessiné à la main ;
- pas fixe / pas variable ;
- validation contre le firmware / vérification visuelle.
Chapitre suivant seulement après ma validation.
```

Chapitres à demander dans l'ordre, par exemple :
1. la commande complète (FSM v3, boucles, sécurité) ;
2. le passage de la FSM au code Arduino ;
3. la construction du modèle Simulink ;
4. la simulation (modèle thermique, conversion en puissance, humidité,
   brûleur, scénarios) ;
5. la validation ;
6. les limites et les perspectives.

### Message 3 — Images manquantes (après la réécriture)

```
Maintenant que le texte est réécrit, fais la liste de TOUTES les images
[IMAGE À PRÉPARER] et de celles qui amélioreraient le mémoire. Pour chacune,
donne :
- le numéro et la légende ;
- le chapitre et l'endroit ;
- COMMENT la produire :
  - soit le code MATLAB R2025b prêt à exécuter sur captures/scenario_NN.mat
    (structure r : r.t, r.T_sec, r.H_sec, r.P_gaz, r.Flame, r.fsm.V_H2,
    r.fsm.V_But, r.fsm.V_Fl_1..3, r.fsm.Etat_LCD, r.sc), avec export PNG
    300 dpi par exportgraphics ;
  - soit la capture à faire (quel modèle, quel bloc, quel instant) ;
  - soit le schéma à dessiner (draw.io / PowerPoint), avec la liste
    exacte des blocs, des flèches et des signaux.
Pense en particulier à :
- le synoptique du système de commande complet (boucle fermée) ;
- les courbes d'humidité (H_sec avec T_sec, effet de T_cible sur la durée
  de séchage) ;
- un zoom sur un cycle de régulation avec les seuils ;
- la puissance moyenne et l'énergie par scénario ;
- des vues zoomées du chart (MODE_H2 : PURGE -> ALLUMAGE -> RÉGULATION).
Je te renverrai les images au fur et à mesure.
```

### Message 4 — Relecture finale

```
Relis l'ensemble : cohérence avec le programme Arduino, numérotation des
figures, des tableaux et des équations, renvois dans le texte, références,
style d'étudiant. Donne la liste des derniers [À COMPLÉTER].
```

---

## PARTIE D — Données de simulation exactes (référence pour Claude)


### D.1 Chaîne de simulation (boucle fermée)

```
Scénario (From Workspace : boutons, consignes, capteurs, défauts)
      │
      ▼
 FSM (chart Stateflow, 0,1 s) ──V_Fl_1..3──► CONVERSION_PUISSANCE ──P_gaz──┐
      ▲   ▲   ▲        │V_H2, V_But                                          │
      │   │   │        ▼                                     P_sol ──► (+) ─┘
      │   │   └── Flame ◄── BRULEUR (retard d'un pas, pannes)      │
      │   └────── H_sec ◄── MODELE_SECHAGE + intégrateur           ▼
      └────────── T_sec ◄──────────────────────────────── MODELE_THERMIQUE ◄── T_amb
```

### D.2 Modèle thermique (1er ordre, paramètres du chapitre 3 du mémoire)

- τ·dT_sec/dt = Kth·(P_gaz + P_sol) + T_amb − T_sec
- Kth = 1/UA = 1/10,2 ≈ **0,098 K/W** ;
  τ = C_eq/UA = 3,60·10⁴/10,2 ≈ **3530 s** (≈ 59 min)
- Conséquence importante : à 100 %, le régime établi serait
  T_amb + 0,098 × 5000 ≈ **+490 °C**. Le brûleur est donc largement
  surdimensionné par rapport aux pertes, ce qui justifie la régulation par
  paliers et la sécurité T_SEC_MAX = 90 °C. Celle-ci est atteinte en
  ≈ 10,5 min à 100 % imposé (scénario 12) et en ≈ 32 min à 33 % imposé
  (scénario 11).

### D.3 Conversion en puissance

- Pnom = **5000 W**. Paliers par électrovannes de rampe :
  - 100 % = EV1 + EV2 + EV3 ;
  - 67 % = EV2 + EV3 ;
  - 33 % = EV3 ;
  - 0 % = aucune.
- Apport solaire : P_sol = ΔT_sol/Kth = 20/0,098 ≈ **204 W** par ciel clair.
  Ce choix est cohérent avec l'estimation du firmware T_cap = T_amb + ΔT_sol
  (ΔT_sol = 20 °C).

### D.4 Régulation (identique au firmware)

- Hystérésis à 4 niveaux (100 / 67 / 33 / 0 %), Hhyst = 5 °C.
- Seuils de palier : T1 = T_init + (T_cible − T_init)/3,
  T2 = T_init + 2(T_cible − T_init)/3, T3 = T_cible.
- Veille à 0 % au-dessus de T3 + Hhyst/2 ; rallumage à 33 % sous
  T3 − Hhyst/2, précédé d'une purge obligatoire de 120 s avant toute
  ouverture de gaz.
- Exemple chiffré (scénario 1, T_cible = 55 °C, T_amb = 25 °C) :
  - START à 10 s, purge, allumage à 100 % à 130 s ;
  - 67 % à 222 s (37,6 °C), 33 % à 335 s (47,6 °C), veille à 597 s (57,5 °C) ;
  - puis cycles 33 % / 0 % entre 52,5 et 57,5 °C : 134 s de chauffe, puis
    ≈ 590 s de veille.
  - Puissance thermique moyenne injectée (modèle) : **≈ 600 W sur la
    première heure, ≈ 300 W en régime de maintien**. Sur tout le scénario 8
    (2 h) : ≈ 455 W, soit ≈ 0,91 kWh.

### D.5 Simulation de l'humidité (modèle illustratif, à présenter comme tel)

- Il n'existe pas encore de modèle de séchage identifié sur le produit. On a
  choisi une **cinétique du premier ordre** (de type modèle de Lewis / loi
  de Newton en séchage en couche mince), avec une vitesse proportionnelle à
  l'excès de température :
  dH/dt = −K·max(T_sec − 35, 0)·(H − H_eq), avec K = 1,14·10⁻⁵ s⁻¹·°C⁻¹,
  H_eq = 20 %HR et H(0) = 90 %HR.
- **But** : faire évoluer H_sec de façon réaliste, pour tester le critère de
  fin de cycle du firmware : H_sec ≤ H_fin = min(H_produit + H_amb, 95)
  = 10 + 35 = 45 %, autorisé seulement après Temps_Min_Fin = 7200 s.
- Résultats :
  - H_sec atteint 45 % vers **81 min**, mais la fin n'est autorisée qu'à
    120 min (scénario 8) : DEMANDE_PROLONGATION à 7210 s (H ≈ 34,6 %),
    puis sans réponse de l'opérateur, TERMINÉ à 7510 s.
  - Scénario 9 : l'opérateur accepte la prolongation (1800 s), puis une
    nouvelle demande arrive.
  - Scénario 10 : capteur d'humidité en mode FIXE (60 %). La fin arrive par
    la durée maximale (5400 s).
- **Limites à écrire honnêtement** : le modèle n'est pas identifié. K,
  H_eq et le seuil de 35 °C sont des valeurs de démonstration. Un modèle de
  séchage en couche mince (Lewis, Page, Henderson-Pabis) et une isotherme
  de sorption du produit seraient à identifier sur le prototype.
  **N'ajoute une référence bibliographique que si tu es sûr qu'elle existe**
  (auteur, titre, revue, année, DOI). Sinon, signale « référence à
  compléter ».

### D.6 Modèle du brûleur et injection des défauts

- La flamme est présente si une vanne de gaz était ouverte au pas
  précédent (retard d'un pas = boucle algébrique évitée), sauf panne
  simulée. La flamme parasite est aussi simulable.
- Défauts injectés par scénario : échec d'allumage, perte de flamme, fuite
  H2 (MQ8), chute de pression H2, arrêt d'urgence, capteur FIXE, palier
  imposé.

### D.7 Choix numériques

- Solveur à pas fixe **ode4, pas 0,1 s** ; chart Stateflow **discret à
  0,1 s**.
- Pourquoi : les sécurités exigent de réagir en moins de 2 s (détection de
  flamme). Les temporisations Stateflow (`after`, `duration`) sont exactes
  au pas près, et le calcul est déterministe et reproductible.
- Durée de calcul : 2 à 12 s par scénario, pour 20 min à 2 h 45 simulées.

### D.8 Les 12 scénarios et la validation croisée (résultats réels)

| N° | Scénario | Changements d'état (Simulink) | Écart max avec le firmware |
|---|---|---|---|
| 1 | Démarrage à froid H2 | H2 10,1 s | 0,1 s |
| 2 | Démarrage solaire (T_amb = 35 °C) | SOLAIRE 10,1 s | 0,1 s |
| 3 | Bascule H2 → GPL (pression 8 → 0,5 bar à 1250 s) | H2 → GPL 1250,1 s, rallumage à 33 % à 1370 s | 0,1 s |
| 4 | Retour au solaire (T_amb en rampe) | H2 → SOLAIRE 2160,1 s | 0,1 s |
| 5 | Échecs d'allumage | 3 essais → ERREUR 382,1 s → reprise 700,1 s | 0,1 s |
| 6 | Pertes de flamme | relance 470 s → URGENCE 700 s → ATTENTE 900 s | 0,1 s |
| 7 | Fuite H2 | URGENCE 1500 s, réarmement refusé à 1700 s, accepté à 1900 s | 0,1 s |
| 8 | Fin par humidité | DEMANDE 7210 s → TERMINÉ 7510 s | 0,1 s |
| 9 | Prolongation acceptée | DEMANDE → PROLONG. 7300 s → DEMANDE 9100 s → TERMINÉ 9400 s | 0,1 s |
| 10 | Humidité FIXE | DEMANDE (durée max) 5410 s → TERMINÉ 5710 s | 0,1 s |
| 11 | Palier imposé 33 % | TERMINÉ (90 °C) 1944,9 s | 0,1 s |
| 12 | Surchauffe (100 % imposé) | TERMINÉ (90 °C) 632,4 s | 0,6 s |

- **Dérive des instants de palier** : environ 4 s par cycle de veille
  (14 s après 1 h). Le firmware lit la sonde T_sec une fois par seconde,
  avec une conversion asynchrone du DS18B20 ; Simulink la lit tous les
  0,1 s. La séquence des états reste identique.
- **Apport majeur de la démarche** : la simulation de référence a révélé
  un **défaut réel du firmware**, que le banc de tests ne couvrait pas.
  Une bascule H2 → GPL pendant la veille à 0 % provoquait un allumage à
  0 % : vanne source et étincelle actives, aucune électrovanne de rampe
  ouverte. Le défaut a été corrigé dans le firmware et dans le chart, et un
  cas de test a été ajouté (banc : 206 vérifications, toutes réussies).
