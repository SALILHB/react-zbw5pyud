# À faire dans MATLAB R2025b — images manquantes du mémoire V3

Tout se fait dans le dossier `matlab/`, avec le modèle déjà construit
(`Simulation_Sechoir_Hybride.slx`) et vos résultats `captures/scenario_01..12.mat`.

## 0. Fichiers nouveaux ou modifiés à copier dans votre dossier `matlab/`

| Fichier | Nouveau / modifié | Rôle |
|---|---|---|
| `scenario_sechoir.m` | modifié | ajoute les scénarios 13 à 16. Les scénarios 1 à 12 sont inchangés |
| `preparer_figures_memoire.m` | nouveau | produit les figures A5, A6, A7 et A8 à partir de vos fichiers `.mat` |
| `capturer_chart_lisible.m` | nouveau | produit les figures A1 à A4 et les vues du modèle sans l'aperçu miniature |
| `retracer_scenarios.m` | nouveau | retrace les figures `scenario_NN.png` depuis les `.mat` (nouvelle légende de la flamme) |
| `tracer_scenario.m` | modifié | légende « flamme (échelle ×15) » |
| `reference/scenario_13..16.csv` et `_firmware.png` | nouveaux | référence du programme Arduino pour les scénarios 13 à 16 |

## 1. Commandes, dans l'ordre

```matlab
lancer_simulation(13:16)                      % A9 : captures/scenario_13.png, etc.
for n = 13:16, comparer_scenario(n); end      % comparaisons Simulink / programme
retracer_scenarios(1:16)                      % facultatif : nouvelle légende de la flamme
preparer_figures_memoire                      % A5, A6, A7, A8 -> captures/memoire/
capturer_chart_lisible                        % A1 à A4 + modele_global, modele_fsm -> captures/memoire/
```

Il n'est pas nécessaire de relancer `construire_modele` : les scénarios 13 à
16 n'utilisent que des entrées qui existent déjà dans le modèle.

**Contrôles affichés par `preparer_figures_memoire`** (ils doivent
correspondre aux valeurs du mémoire) :

- scénario 1, première heure : **≈ 600 W** (test sur vos données : 599 W) ;
- scénario 8, jusqu'à la demande : **≈ 455 W et ≈ 0,91 kWh** (test : 454 W,
  0,91 kWh) ;
- scénario 8 : H_sec passe sous H_fin à **≈ 81 min** ; DEMANDE à 7210 s,
  TERMINÉ à 7510 s ;
- écarts Simulink / programme : **0,1 s** (0,6 s au scénario 12).

Si une valeur s'écarte nettement, envoyez-moi la sortie **avant** de modifier
le mémoire.

`capturer_chart_lisible` travaille sur une copie temporaire : votre modèle
n'est pas modifié. Si une ligne affiche `[ERREUR]`, copiez-la-moi. Les autres
images sont quand même produites.

## 2. Captures d'animation à faire à la main (A10 à A13)

1. `open_system('Simulation_Sechoir_Hybride')`, puis
   `appliquer_scenario('Simulation_Sechoir_Hybride', scenario_sechoir(N))`
   avec le numéro N du tableau ci-dessous.
2. Ouvrez le chart (double-clic sur `FSM`, puis sur `Chart`). Vitesse
   d'animation : *Slow*.
3. Clic droit sur l'état indiqué → *Set Breakpoint on Entry*, puis **Run**.
4. Capture (`Win + Maj + S`), recadrée sur le chart. Enregistrez-la sous le
   nom indiqué, puis **Stop** et retirez le point d'arrêt.

| Réf. | N | Point d'arrêt sur l'entrée de… | Instant attendu | Fichier |
|---|---|---|---|---|
| A10 | 1 | `REGULATION` (dans `MODE_H2`) | 130 s (1er arrêt) ; *Continue* jusqu'à ≈ 1189 s pour un rallumage à 33 % | `anim_regulation_h2.png` |
| A11 | 5 | `ERREUR_COMBUSTION` | 382 s | `anim_erreur_combustion.png` |
| A12 | 7 | `URGENCE_ATEX` | 1500 s | `anim_urgence_atex.png` |
| A13 | 9 | `DEMANDE_PROLONGATION` | 7210 s. `REGULATION` ou `PURGE` reste actif dans l'autre région : les deux régions parallèles sont surlignées | `anim_demande_prolongation.png` |

## 3. Où déposer chaque image dans `memoire_V3_latex/`

| Fichier produit | Destination dans `memoire_V3_latex/` |
|---|---|
| `captures/memoire/chart_hierarchie.png` | `Figures/simulink/chart_hierarchie.png` (A1) |
| `captures/memoire/chart_mode_h2.png` | `Figures/simulink/chart_mode_h2.png` (A2) |
| `captures/memoire/chart_region_phase.png` | `Figures/simulink/chart_region_phase.png` (A3) |
| `captures/memoire/chart_urgence.png` | `Figures/simulink/chart_urgence.png` (A4) |
| `captures/memoire/zoom_regulation.png` | `Figures/simulink/zoom_regulation.png` (A5) — **déjà fournie**, tracée depuis votre `scenario_01.mat` |
| `captures/memoire/humidite_fin_cycle.png` | `Figures/simulink/humidite_fin_cycle.png` (A6) |
| `captures/memoire/energie_scenarios.png` | `Figures/simulink/energie_scenarios.png` (A7) |
| `captures/memoire/ecarts_validation.png` | `Figures/simulink/ecarts_validation.png` (A8) |
| `captures/scenario_13.png` | `Figures/simulink/scenario_13.png` (A9) |
| vos 4 captures d'animation | `Figures/simulink/anim_*.png` (A10 à A13) |
| `5.0_banc_tests_terminal.png` | `Figures/5.0_banc_tests_terminal.png` (A14) — **déjà fournie** (sortie réelle du banc) |
| `captures/memoire/modele_global.png`, `modele_fsm.png` | remplacent `Figures/simulink/modele_global.png` et `modele_fsm.png` (sans aperçu miniature) |
| `captures/scenario_NN.png` retracées | remplacent `Figures/simulink/scenario_NN.png` (facultatif) |

Il suffit de déposer l'image sous le bon nom et de recompiler : le cadre
rouge « FIGURE À COMPLÉTER » est remplacé automatiquement.

## 4. Scénarios 13 à 16 : résultats du programme Arduino (Tableau 6.8)

Ces instants ont été **mesurés en faisant tourner le programme Arduino
réel** (`make -C tests reference`, même modèle physique que Simulink). Comparez
les instants Simulink à ceux-ci avec `comparer_scenario(13..16)`.

| N° | Réglage exact | Résultat du programme Arduino |
|---|---|---|
| 13 | `Press_H2` : 8 → 0,5 bar à 900 s (pendant la veille commencée à 597 s) | GPL à 900 s. **Aucune ouverture de gaz** pendant la purge ni ensuite, tant que T_sec > 52,5 °C. Rallumage GPL à **33 % à 1189 s** (T_sec = 52,5 °C) |
| 14 | Flamme vue gaz fermé de 800 à 820 s (veille) ; réarmement à 900 s | **URGENCE à 805 s** (5 s après l'apparition de la flamme, cause 5) ; retour à ATTENTE à 900 s |
| 15 | Comme le scénario 3 (GPL à 1250 s), puis `Press_H2` = 8 bar à 2000 s | GPL à 1250 s, rallumage à 33 % à 1370 s. **Retour sur H2 à 2000 s** (pendant une veille), purge, puis rallumage H2 à **33 % à 2120 s** (T_sec = 52,0 °C) |
| 16 | AU enfoncé de 400 à 600 s ; réarmement à 500 s et à 700 s ; START à 800 s | **URGENCE à 400 s** (cause 3). Réarmement **refusé à 500 s** (AU encore enfoncé), **accepté à 700 s** → ATTENTE. Relance à 800 s : allumage à 100 % à 920 s (régime CHAUD, T_sec = 46,7 °C < T1) |

## 5. Ce qu'il faut me renvoyer

- la sortie de `lancer_simulation(13:16)`, de `comparer_scenario(13..16)` et
  de `preparer_figures_memoire` (les lignes de contrôle) ;
- la sortie de `capturer_chart_lisible`, et les 4 images `chart_*.png` si
  elles ne sont pas lisibles.
