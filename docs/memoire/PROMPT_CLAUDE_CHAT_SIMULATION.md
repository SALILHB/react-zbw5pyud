# Prompt pour Claude (conversation claude.ai) — chapitre « Modélisation et simulation »

> **Mode d'emploi** : ouvrir une nouvelle conversation sur claude.ai, joindre
> les fichiers listés au §2, puis coller tout le texte à partir de la ligne
> « DÉBUT DU PROMPT ».

---

DÉBUT DU PROMPT

## 1. Ton rôle et le contexte

Tu es un encadrant expérimenté en automatique et en énergétique. Tu aides un
étudiant de **master (ESSA Tlemcen)** à rédiger son mémoire, en **français
académique**. Le sujet est la **commande en température d'un séchoir solaire
hybride** : un capteur solaire et un brûleur à deux gaz, hydrogène (H2) en
priorité et GPL en secours.

Le travail est déjà fait et validé. Ta mission est de le **mettre en valeur
dans le mémoire** : expliquer, justifier, illustrer. Tu n'as rien à inventer.

Principes du projet, à respecter dans tout le texte :

- **Le programme Arduino (firmware) est la source de vérité.** Il contient une
  machine à états (FSM). Le modèle Simulink/Stateflow a été construit pour
  reproduire **exactement** ce firmware, puis validé contre lui.
- Le modèle Simulink est **construit entièrement par un script**
  (`construire_modele.m`). Il n'y a aucune édition manuelle, et le modèle est
  reproductible à l'identique.
- Toutes les simulations ont été faites sous **MATLAB R2025b** (Simulink +
  Stateflow).

## 2. Fichiers joints

1. **`Commande_Sechoir_Solaire_Hybride.docx`** : le mémoire actuel (17
   chapitres, bibliographie [1]–[35], annexes). Les chapitres 14 (« Modèle
   Simulink / Stateflow et guide de simulation ») et 15 (« Validation »)
   existent déjà, en version courte.
2. **`matlab.zip`** : le dossier MATLAB complet, avec :
   - les scripts :
     - `construire_modele.m` : construit le modèle ;
     - `scenario_sechoir.m` : définit les 12 scénarios ;
     - `appliquer_scenario.m` ;
     - `lancer_simulation.m` ;
     - `tracer_scenario.m` ;
     - `journal_scenario.m` ;
     - `comparer_scenario.m` ;
     - `capturer_modele.m` ;
     - `alleger_resultat.m` ;
     - `exporter_figure.m` ;
   - `GUIDE_SIMULATION.md` : le guide pas à pas ;
   - `captures/` : **les résultats de simulation MATLAB** :
     - `scenario_01.png` … `scenario_12.png` : figure à 4 graphes par
       scénario (T_sec / T_cible / T_amb et seuils T1-T2 ; puissance de gaz,
       vanne H2 / GPL et flamme ; état de la FSM ; humidité H_sec et seuil
       H_fin) ;
     - `comparaison_01.png` … `comparaison_12.png` : superposition Simulink /
       firmware (T_sec et état) ;
     - `scenario_NN.mat` : signaux simulés (structure `r` : `r.t`, `r.T_sec`,
       `r.H_sec`, `r.P_gaz`, `r.Flame`, `r.fsm.V_H2`, `r.fsm.V_But`,
       `r.fsm.V_Fl_1..3`, `r.fsm.Etat_LCD`, `r.sc` = scénario) ;
     - `modele_global.png`, `modele_fsm.png`, `modele_thermique.png`,
       `chart_fsm.png` : images du modèle Simulink et du chart Stateflow.
       Le chart complet est très dense : il faut le mettre **en annexe**. Dans
       le corps du mémoire, utilise la figure de hiérarchie des états, qui
       existe déjà.
   - `reference/` : les **résultats attendus**, produits par le firmware
     Arduino réel, compilé sur PC et bouclé sur le même modèle physique
     (`scenario_NN.csv`, `scenario_NN_firmware.png`).

## 3. Données techniques exactes (à utiliser telles quelles, sans les modifier)

### 3.1 Chaîne de simulation (boucle fermée)

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

### 3.2 Modèle thermique (1er ordre, paramètres du chapitre 3 du mémoire)

- τ·dT_sec/dt = Kth·(P_gaz + P_sol) + T_amb − T_sec
- Kth = 1/UA = 1/10,2 ≈ **0,098 K/W** ;
  τ = C_eq/UA = 3,60·10⁴/10,2 ≈ **3530 s** (≈ 59 min)
- Conséquence importante : à 100 %, le régime établi serait
  T_amb + 0,098 × 5000 ≈ **+490 °C**. Le brûleur est donc largement
  surdimensionné par rapport aux pertes, ce qui justifie la régulation par
  paliers et la sécurité T_SEC_MAX = 90 °C. Celle-ci est atteinte en
  ≈ 10,5 min à 100 % imposé (scénario 12) et en ≈ 32 min à 33 % imposé
  (scénario 11).

### 3.3 Conversion en puissance

- Pnom = **5000 W**. Paliers par électrovannes de rampe :
  - 100 % = EV1 + EV2 + EV3 ;
  - 67 % = EV2 + EV3 ;
  - 33 % = EV3 ;
  - 0 % = aucune.
- Apport solaire : P_sol = ΔT_sol/Kth = 20/0,098 ≈ **204 W** par ciel clair.
  Ce choix est cohérent avec l'estimation du firmware T_cap = T_amb + ΔT_sol
  (ΔT_sol = 20 °C).

### 3.4 Régulation (identique au firmware)

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

### 3.5 Simulation de l'humidité (modèle illustratif, à présenter comme tel)

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

### 3.6 Modèle du brûleur et injection des défauts

- La flamme est présente si une vanne de gaz était ouverte au pas
  précédent (retard d'un pas = boucle algébrique évitée), sauf panne
  simulée. La flamme parasite est aussi simulable.
- Défauts injectés par scénario : échec d'allumage, perte de flamme, fuite
  H2 (MQ8), chute de pression H2, arrêt d'urgence, capteur FIXE, palier
  imposé.

### 3.7 Choix numériques

- Solveur à pas fixe **ode4, pas 0,1 s** ; chart Stateflow **discret à
  0,1 s**.
- Pourquoi : les sécurités exigent de réagir en moins de 2 s (détection de
  flamme). Les temporisations Stateflow (`after`, `duration`) sont exactes
  au pas près, et le calcul est déterministe et reproductible.
- Durée de calcul : 2 à 12 s par scénario, pour 20 min à 2 h 45 simulées.

### 3.8 Les 12 scénarios et la validation croisée (résultats réels)

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

## 4. Ce que je te demande

Rédige une version **complète et professionnelle** du chapitre de
modélisation et de simulation. Il remplacera ou enrichira les chapitres 14
et 15 actuels. Tu es libre d'améliorer la structure. Voici ce qui doit y
figurer :

1. **Objectifs de la simulation** : pourquoi simuler avant de tester sur le
   prototype (sécurité du gaz et de l'H2, coût, reproductibilité, test des
   défauts impossibles à provoquer sans risque).
2. **Architecture du système de commande complet** : décris la boucle
   fermée du §3.1 bloc par bloc (rôle, entrées, sorties). Propose **une
   figure de synthèse du système de commande** (synoptique) et dis
   exactement comment la réaliser :
   - soit en utilisant `modele_global.png` ;
   - soit en dessinant un synoptique propre (draw.io / Visio / PowerPoint),
     avec la liste précise des blocs, des flèches et des signaux à
     représenter.
3. **Chaque étape de modélisation, avec son rôle et ses équations** :
   - modèle thermique ;
   - conversion en puissance ;
   - apport solaire ;
   - séchage et humidité ;
   - brûleur et capteur de flamme ;
   - scénarios et injection de défauts ;
   - chart Stateflow ;
   - solveur ;
   - enregistrement et figures.
4. **Justification des choix, en comparant avec les alternatives** : pour
   chaque choix, explique pourquoi nous l'avons pris **et pas l'autre**.
   Par exemple :
   - modèle du 1er ordre / modèle multizone ou CFD ;
   - régulation par paliers et hystérésis / PID continu (vannes TOR, sécurité
     gaz, purges) ;
   - Stateflow / logique en blocs Simulink ou code MATLAB ;
   - modèle construit par script / modèle dessiné à la main ;
   - pas fixe / pas variable ;
   - validation croisée contre le firmware réel / simple vérification
     visuelle.
5. **Avantages et limites de la méthode**. Pour les avantages : traçabilité
   code ↔ modèle, reproductibilité, test des défauts sans danger, défaut
   réel trouvé. Pour les limites : modèles non identifiés, pas de modèle de
   rayonnement solaire, échantillonnage différent de la sonde.
6. **Principe des paramètres** : tableau des paramètres (valeur, unité,
   origine : mesure, cahier des charges, valeur de démonstration) et rôle
   de chacun. Mets l'accent sur l'humidité : comment elle est simulée,
   pourquoi, et ce qu'il faudrait mesurer pour identifier le vrai modèle.
7. **Résultats, scénario par scénario** : pour les scénarios les plus
   parlants (1, 3, 4, 5, 6 ou 7, 8, 9, 12), un paragraphe d'analyse qui
   renvoie à la figure `scenario_NN.png` et à la figure
   `comparaison_NN.png`.
8. **Graphes d'humidité** : analyse des courbes H_sec (scénarios 1, 8, 9 et
   10). Propose aussi 2 ou 3 **graphes supplémentaires utiles**, avec le
   **code MATLAB prêt à exécuter** qui les produit à partir de
   `captures/scenario_NN.mat` (structure `r` décrite au §2). Par exemple :
   - H_sec et T_sec sur le même graphe (deux axes) ;
   - l'effet de T_cible sur la durée de séchage ;
   - la puissance moyenne et l'énergie par scénario ;
   - le cycle de régulation zoomé avec les seuils.
   Le code doit exporter en PNG 300 dpi (`exportgraphics`) et fonctionner
   sous R2025b.
9. **Tableau de validation croisée** (§3.8) et conclusion du chapitre.

## 5. Forme attendue

- **Français académique**, avec « nous », des phrases claires et aucune
  exagération.
- Numérotation des sections, **légendes** de figures et de tableaux
  (« Figure N : … », « Tableau N : … »), équations numérotées.
- Pour chaque figure : indique **le fichier exact à insérer** (par exemple
  `captures/scenario_08.png`) et **l'endroit** dans le texte.
- Ne change **aucun résultat chiffré** du §3. Si une information manque,
  écris **[À COMPLÉTER : …]** au lieu de l'inventer.
- Références : utilise celles du mémoire ([1]–[35]) quand elles
  conviennent. Pour une nouvelle référence, donne l'entrée complète (auteurs,
  titre, revue, année, DOI ou lien) **uniquement si tu es certain qu'elle
  existe**.
- Livre le résultat dans cet ordre :
  1. le **plan** du chapitre ;
  2. le **texte complet**, prêt à coller dans Word ;
  3. la **liste des figures** à insérer (fichier → section) ;
  4. le **code MATLAB** des graphes supplémentaires ;
  5. les **points [À COMPLÉTER]** et tes questions.

FIN DU PROMPT
