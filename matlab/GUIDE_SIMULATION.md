# Guide de simulation — commande complète (FSM + modèle thermique)

Ce guide mène de l'ouverture de MATLAB aux figures du mémoire. Il y a
**4 commandes** à taper, toutes dans le dossier `matlab/`.

> Version cible : **MATLAB R2025b**, avec **Simulink** et **Stateflow**
> (le modèle `.slx` fourni a été enregistré avec R2025b). Vérifier les
> produits installés avec la commande `ver` : les lignes *Simulink* et
> *Stateflow* doivent apparaître. Récupérer le dossier `matlab/` complet.

---

## Étape 0 — Ouvrir MATLAB dans le bon dossier

Dans MATLAB, *Current Folder* → aller dans `matlab/` (ou taper
`cd 'C:\...\matlab'`). Tous les fichiers ci-dessous doivent être visibles.

| Fichier | Rôle |
|---|---|
| `Commande_Sechoir_Hybride_corrige.slx` | modèle de départ (**jamais modifié**) |
| `completer_fsm_sechoir.m` | écrit la logique du firmware dans le chart Stateflow |
| `preparer_simulation.m` | construit le modèle de simulation complet |
| `scenario_sechoir.m` | les 12 scénarios (boutons, consignes, défauts, paramètres) |
| `lancer_simulation.m` | simule, affiche la chronologie, enregistre les figures |
| `comparer_scenario.m` | compare Simulink à la référence du firmware |
| `capturer_modele.m` | exporte les images du modèle et du chart |
| `reference/scenario_NN.csv` | résultats attendus (firmware réel, même modèle physique) |

---

## Étape 1 — Construire le modèle : `preparer_simulation`

```matlab
preparer_simulation
```

Le script crée `Simulation_Sechoir_Hybride.slx` et affiche une ligne `[ok]`
par étape. Sortie attendue (abrégée) :

```
=== Preparation du modele de simulation : Simulation_Sechoir_Hybride ===
  [ok] Commande_Sechoir_Hybride_corrige.slx copie en Simulation_Sechoir_Hybride.slx
=== Completion du chart : Simulation_Sechoir_Hybride/FSM/Chart ===
  ...
  [ok] hierarchie conforme
  [ok] libelles ecrits
  [ok] NN transitions creees
  [ok] chart execute toutes les 0.1 s
  [ok] anciens ports et constantes supprimes
  [ok] 24 entrees du chart cablees (T_sec, H_sec, Flame, T_amb via ports, le reste via From Workspace)
  [ok] 11 sorties du chart reliees aux ports du sous-systeme FSM
  [ok] modele physique, bruleur, sechage, enregistrement et Scope ajoutes
  [ok] solveur ode4, pas fixe 0.1 s
  [ok] le modele compile sans erreur
=== Simulation_Sechoir_Hybride.slx enregistre. Etape suivante : lancer_simulation(1) ===
```

**Pourquoi ce câblage ?** Dans le modèle fourni, les ports du sous-système
`FSM` n'étaient reliés ni au chart ni aux sorties : le chart ne recevait rien
et aucune vanne ne pouvait s'ouvrir. Le script relie les 24 entrées du chart
et ajoute ce qui manquait pour boucler la simulation :

| Bloc ajouté | Rôle |
|---|---|
| `BRULEUR` | flamme = vanne de gaz ouverte (au pas précédent), sauf panne simulée |
| `MODELE_SECHAGE` + intégrateur `H_sec` | humidité de l'air extrait (modèle illustratif) |
| `sc_P_sol` + `Puissance_totale` | apport du capteur solaire, ajouté à la puissance du gaz |
| `sc_*` (From Workspace) | boutons, consignes, capteurs et défauts du scénario |
| `SUIVI` (Scope) | T_sec, H_sec et état en direct |
| `log_*` (To Workspace) | enregistrement pour les figures |

**Si une ligne affiche `[ERREUR]` ou `[HIERARCHIE INCORRECTE]`** : copiez-moi
toute la sortie (et le message du *Diagnostic Viewer* s'il s'ouvre).
En cas de `[HIERARCHIE INCORRECTE]` : ouvrir `Simulation_Sechoir_Hybride`,
glisser-déposer les états signalés dans leur boîte parente, régler `EN_CYCLE`
en *Decomposition → AND (parallel)*, enregistrer, puis
`preparer_simulation('Simulation_Sechoir_Hybride', 'Simulation_Sechoir_Hybride')`.

---

## Étape 2 — Simuler : `lancer_simulation`

```matlab
lancer_simulation(1)        % un scénario
lancer_simulation(1:12)     % les douze (quelques minutes)
```

Pour chaque scénario, la fenêtre de commande affiche la **chronologie**
(changements d'état et de puissance) et le script enregistre
`captures/scenario_NN.png` (figure à 4 graphes, 300 dpi) et
`captures/scenario_NN.mat` (signaux).

### Résultats attendus (référence firmware, même modèle physique)

| N° | Scénario | Changements d'état attendus | T_sec max |
|---|---|---|---|
| 1 | Démarrage à froid H2 | H2 10 s ; paliers 100 % (130 s) → 67 % (222 s) → 33 % (335 s) → 0 % (597 s) ; rallumages à 33 % | 57,5 °C |
| 2 | Démarrage solaire | SOLAIRE 10 s, aucune vanne ouverte | 47,8 °C |
| 3 | Bascule H2 → GPL | H2 10 s → GPL 1250 s ; rallumage GPL à 33 % à 1370 s | 57,5 °C |
| 4 | Retour au solaire | H2 10 s → SOLAIRE 2160 s | 57,5 °C |
| 5 | Échecs d'allumage | H2 10 s → ERREUR 382 s → H2 700 s (OK) ; rallumage 820 s | 57,5 °C |
| 6 | Pertes de flamme | H2 10 s ; relance 470 s → URGENCE 700 s → ATTENTE 900 s | 62,5 °C |
| 7 | Fuite H2 | H2 10 s → URGENCE 1500 s → ATTENTE 1900 s (réarmement à 1700 s refusé) → H2 2000 s | 57,5 °C |
| 8 | Fin par humidité | H2 10 s → DEMANDE 7210 s → TERMINÉ 7510 s → ATTENTE 7810 s | 57,5 °C |
| 9 | Prolongation acceptée | … DEMANDE 7210 s → PROLONGATION 7300 s → DEMANDE 9100 s → TERMINÉ 9400 s → ATTENTE 9700 s | 57,5 °C |
| 10 | H_sec FIXE | H2 10 s → DEMANDE 5410 s (durée max) → TERMINÉ 5710 s → ATTENTE 6010 s | 57,5 °C |
| 11 | Palier imposé 33 % | H2 10 s → TERMINÉ 1945 s (surchauffe 90 °C) → ATTENTE 2245 s | 90 °C |
| 12 | Surchauffe (100 % imposé) | H2 10 s → TERMINÉ 633 s → ATTENTE 933 s | 90 °C |

Des écarts de **quelques secondes** sur les changements de palier sont
normaux : le firmware lit les sondes une fois par seconde, Simulink tous les
0,1 s. Un changement d'état **absent**, **en plus** ou décalé de plus de
~10 s signale une différence de logique : envoyez-moi la chronologie.

---

## Étape 3 — Valider : `comparer_scenario`

```matlab
comparer_scenario(1)
for n = 1:12, comparer_scenario(n); end
```

Superpose T_sec et l'état **Simulink** (trait plein) et **firmware** (tirets),
liste l'écart de temps de chaque changement d'état, et enregistre
`captures/comparaison_NN.png`. C'est la figure de **validation croisée** du
mémoire (chapitre 15).

---

## Étape 4 — Images du modèle : `capturer_modele`

```matlab
capturer_modele
```

Enregistre `captures/modele_global.png`, `modele_fsm.png`,
`modele_thermique.png` et `chart_fsm.png` (300 dpi, plus nets qu'une capture
d'écran). Avant, pour un joli diagramme : ouvrir le modèle, *Format → Auto
Arrange* (ou Ctrl+Maj+A) si les blocs se chevauchent, puis enregistrer.

---

## Captures à faire à l'écran (animation Stateflow)

L'état actif surligné n'existe que pendant la simulation. Pour le figer :

1. `open_system('Simulation_Sechoir_Hybride')`, puis
   `appliquer_scenario('Simulation_Sechoir_Hybride', scenario_sechoir(7))`
   (charge le scénario et règle le *Stop Time*, sans lancer la simulation).
2. Ouvrir le chart (double-clic sur `FSM`, puis sur `Chart`). Dans l'onglet
   *Simulation*, choisir la vitesse d'animation **Slow** ou **Medium**
   (menu *Animation Speed* / *Debug*, selon l'agencement de la barre
   d'outils de R2025b).
3. **Méthode la plus sûre — point d'arrêt sur l'état** : clic droit sur l'état
   à montrer (par exemple `URGENCE_ATEX`) → *Add Breakpoint* / *Set
   Breakpoint on Entry*. Cliquer **Run** : la simulation s'arrête à l'entrée
   dans l'état, qui est surligné. Faire la capture, puis **Stop** (et retirer
   le point d'arrêt : clic droit → *Clear Breakpoint*).
4. Autre méthode, pour une date précise : dans le *Stop Time* de la barre
   d'outils, mettre l'instant voulu (par exemple `1600`) et cliquer **Run** ;
   ou lancer la simulation en animation lente et cliquer **Pause** au bon
   moment.
5. Capture : `Win + Maj + S` (Windows).

Les noms exacts des menus peuvent différer légèrement dans R2025b : si un
menu n'est pas trouvé, décrivez-moi ce que vous voyez (ou envoyez une capture
de la barre d'outils) et je vous indique le bon chemin.

Instants intéressants :

| Capture | Scénario | Point d'arrêt sur l'état / pause à |
|---|---|---|
| Combustion H2, régulation | 1 | `REGULATION` (dans `MODE_H2`) / 400 s |
| Purge de bascule vers GPL | 3 | `PURGE` (dans `MODE_GPL`) / 1300 s |
| MODE_SOLAIRE | 2 | `MODE_SOLAIRE` / 600 s |
| ERREUR_COMBUSTION | 5 | `ERREUR_COMBUSTION` / 500 s |
| URGENCE_ATEX | 7 | `URGENCE_ATEX` / 1600 s |
| DEMANDE_PROLONGATION (régions parallèles actives) | 9 | `DEMANDE_PROLONGATION` / 7250 s |
| PROLONGATION | 9 | `PROLONGATION` / 8000 s |

Autres captures utiles : le **Scope SUIVI** (double-clic) après une
simulation, et le **Model Explorer** (Ctrl+H) sur le chart pour la liste des
données et paramètres.

---

## Liste des figures pour le mémoire

| Fichier | Contenu | Chapitre |
|---|---|---|
| `modele_global.png` | boucle fermée FSM → puissance → thermique | 14.1 |
| `modele_fsm.png` | câblage du chart | 14.2 |
| `chart_fsm.png` | chart Stateflow complet | 5 et 14 |
| `scenario_01.png` | démarrage et régulation par paliers | 4 et 14 |
| `scenario_03.png`, `scenario_04.png` | bascule GPL, retour au solaire | 7 |
| `scenario_05.png` à `scenario_07.png` | défauts et sécurité | 8 |
| `scenario_08.png`, `scenario_09.png` | fin de cycle, prolongation | 6 |
| `comparaison_NN.png` | validation Simulink / firmware | 15 |
| captures d'animation | états actifs | 5 et 14 |

---

## Modèle physique (identique dans Simulink et dans la référence firmware)

| Élément | Équation | Valeurs |
|---|---|---|
| Puissance gaz | P = Pnom × (1 ; 0,67 ; 0,33 ; 0) selon EV1..EV3 | Pnom = 5000 W |
| Chambre | τ dT_sec/dt = Kth (P + P_sol) + T_amb − T_sec | Kth = 0,098 K/W ; τ = 3530 s |
| Séchage (illustratif) | dH/dt = −K · max(T_sec − 35, 0) · (H − 20) | K = 1,14·10⁻⁵ |
| Brûleur | flamme = gaz ouvert au pas précédent ET pas de panne, OU parasite | — |

Remarque pour le mémoire : avec Kth = 0,098 K/W, 5000 W donneraient +490 °C
en régime établi ; la régulation par paliers est donc indispensable, et la
sécurité `T_SEC_MAX_SECURITE` (90 °C) est atteinte en ~10 min à 100 % imposé
(scénario 12).

## Ajouter un scénario

Copier un `case` de `scenario_sechoir.m` et modifier `s.<signal>` (matrice
`[t valeur]`, valeur maintenue jusqu'au point suivant) ou `p.<paramètre>`
(valeurs du chart, durées en secondes). Aides : `cst(v)`,
`marches([t1 v1; t2 v2])`, `impulsions([t1 t2])` (appuis de 1 s).
