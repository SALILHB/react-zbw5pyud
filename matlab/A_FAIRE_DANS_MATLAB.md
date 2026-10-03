# À faire dans MATLAB R2025b — mémoire V4 (demandes D1 à D5 et E1 à E4)

Tout se lance depuis le dossier `matlab/`, avec le modèle validé
`Simulation_Sechoir_Hybride.slx` déjà construit. Dans les commandes, remplacez
`M` par le chemin de votre dossier `memoire_V4_latex`, par exemple :

```matlab
M = 'D:\memoire master claud\projet_claude\memoire_V4_latex';
```

## 0. Ce qui est déjà fait

Les études **E1 à E4 ont déjà été simulées** avec le programme Arduino réel et
le même modèle physique (moteur « programme »). Les 10 figures et 5 tableaux
sont dans `memoire_V4_simulation.zip`, à décompresser dans `memoire_V4_latex/`.
Les chiffres et les observations sont dans `RESULTATS_SIMULATION.md`.

L'étape 2 ci-dessous refait les mêmes études **avec Simulink**, pour que les
figures du mémoire viennent du modèle Stateflow. Les valeurs doivent être
identiques à un pas de calcul près (0,1 s).

Fichiers nouveaux à copier dans votre dossier `matlab/` : tout le dossier
`matlab/etudes/` et `RESULTATS_SIMULATION.md`. Le dossier `tests/` n'est
utile que pour le moteur « programme » (compilateur g++).

## 1. Demandes D1 à D5 (figures du modèle validé)

```matlab
lancer_simulation(13:16)                      % D3 : captures/scenario_13..16.png
for n = 13:16, comparer_scenario(n); end      % D3 : comparaison_13..16
preparer_figures_memoire                      % D2 : zoom, humidité, énergie, écarts
capturer_chart_lisible                        % D1 : 4 vues du chart + modèles sans aperçu
```

- **D5** (banc de tests) : l'image `Figures/5.0_banc_tests_terminal.png` est
  déjà fournie. Elle reproduit la sortie réelle de `make -C tests run` :
  22 cas, 206 vérifications, 0 échec.
- **D4** (captures d'animation, à faire à la main) :
  1. `open_system('Simulation_Sechoir_Hybride')`, puis
     `appliquer_scenario('Simulation_Sechoir_Hybride', scenario_sechoir(N))`.
  2. Ouvrez le chart (FSM → Chart) et réglez l'animation sur *Slow*.
  3. Clic droit sur l'état → *Set Breakpoint on Entry*, puis **Run**.
  4. Au point d'arrêt, faites la capture (Win + Maj + S), puis **Stop** et
     retirez le point d'arrêt.

| Fichier | N | Point d'arrêt sur l'entrée de… | Arrêt vers | Remarque |
|---|---|---|---|---|
| `anim_regulation_h2.png` | 1 | `REGULATION` (MODE_H2) | 130 s | REGULATION reste actif jusqu'à 597 s (donc aussi à ≈ 400 s) |
| `anim_erreur_combustion.png` | 5 | `ERREUR_COMBUSTION` | 382 s | actif jusqu'à l'appui OK à 700 s (donc à ≈ 500 s) |
| `anim_urgence_atex.png` | 7 | `URGENCE_ATEX` | 1500 s | actif jusqu'au réarmement de 1900 s (donc à ≈ 1600 s) |
| `anim_demande_prolongation.png` | 9 | `DEMANDE_PROLONGATION` | 7210 s | REGULATION ou PURGE reste surligné dans l'autre région |

## 2. Études E1 à E4 avec Simulink

```matlab
addpath('etudes')
construire_modele_etudes          % une seule fois : crée Etudes_Sechoir.slx (copie)
lancer_etudes('simulink', M)      % E1 à E4 : figures et tableaux écrits dans M
```

**`construire_modele_etudes`** copie le modèle validé, sans le modifier, et
ajoute dans la copie :
- UA et C_eq réglables, et la porte ;
- le retard, le bruit et la quantification de la mesure ;
- le GPL indisponible ;
- le stock d'hydrogène.

Il se termine par une **vérification** : avec les réglages par défaut, les
scénarios 1, 3 et 8 doivent donner les mêmes changements de palier que la
référence. Les trois lignes doivent afficher `[ok]` avec un écart ≤ 0,1 s.
Sinon, **envoyez-moi toute la sortie** avant de continuer.

**`lancer_etudes`** dure quelques minutes (8 journées de 10 h, 14 variantes
de 2 h). Il écrit :
- dans `M/Figures/simulation/` : `profil_journee.png`, `energie_sources.png`,
  `tsec_sources.png`, `modes_tsec.png`, `modes_energie.png`, `bilan_pertes.png`,
  `sensibilite_retard.png`, `sensibilite_ua.png` (facultative),
  `perturbation_porte.png` ;
- dans `M/Figures/simulation/tables/` : `tab_sources.tex`,
  `tab_consommation.tex`, `tab_modes.tex`, `tab_pertes.tex`,
  `tab_sensibilite.tex` ;
- dans `captures/etudes/` : `resultats_E1.txt` à `resultats_E4.txt`.
  **Comparez-les** aux valeurs de `RESULTATS_SIMULATION.md`.

On peut aussi lancer une seule étude, par exemple `etude_E1_sources('simulink', M)`.

Contrôles rapides :

| Étude | Valeur attendue (moteur programme) |
|---|---|
| E1, C1 | E_H2 = 0,60 kWh ; solaire à 11 h 58 ; 6 allumages |
| E1, C4 | bascule GPL à 10 h 55 (m0 = 28 g) |
| E2, M1 | 0,91 kWh ; 98 % dans la bande ; 10 allumages |
| E3 | énergie fournie 2,06 kWh ; parois 1,90 kWh |
| E4, nominal | période 12,2 min ; P̄ = 304 W ; porte : retour dans la bande à 1900 s |

## 3. Où va chaque fichier dans `memoire_V4_latex/`

| Fichier produit | Destination |
|---|---|
| `captures/memoire/chart_hierarchie.png`, `chart_mode_h2.png`, `chart_region_phase.png`, `chart_urgence.png` | `Figures/simulink/` (A1 à A4) |
| `captures/memoire/modele_global.png`, `modele_fsm.png` | remplacent `Figures/simulink/modele_global.png` et `modele_fsm.png` |
| `captures/memoire/zoom_regulation.png`, `humidite_fin_cycle.png`, `ecarts_validation.png`, `energie_scenarios.png` | `Figures/simulink/` (A6, A7, A12, A13) |
| `captures/scenario_13.png` à `scenario_16.png`, `comparaison_13..16.png` | `Figures/simulink/` (A14) |
| vos 4 captures d'animation | `Figures/simulink/anim_*.png` (A8 à A11) |
| figures et tableaux des études | déjà écrits à leur place par `lancer_etudes('simulink', M)` (A15 à A22, Partie C) |

## 4. Ce qu'il faut me renvoyer

- la sortie de `construire_modele_etudes` (lignes `[ok]` / `[ERREUR]`, vérification) ;
- `captures/etudes/resultats_E1.txt` à `resultats_E4.txt` ;
- la sortie de `comparer_scenario(13..16)` et de `preparer_figures_memoire` ;
- le PDF compilé, ou la liste des cadres « FIGURE À COMPLÉTER » qui restent.
