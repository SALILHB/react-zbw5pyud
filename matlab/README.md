# Séchoir solaire hybride — Modèle Simulink/Stateflow

Modèle Simulink de la commande, dont le chart Stateflow `FSM` reproduit la
logique du firmware Arduino `sechoir_hybride/` (v3), qui fait foi.

Version cible : **MATLAB R2025b** (Simulink + Stateflow).

**Pour simuler : suivre [`GUIDE_SIMULATION.md`](GUIDE_SIMULATION.md)**
(4 commandes : `construire_modele`, `lancer_simulation(1:12)`,
`comparer_scenario(n)`, `capturer_modele`).

## Contenu

| Fichier | Rôle |
|---|---|
| `construire_modele.m` | Construit à partir de zéro `Simulation_Sechoir_Hybride.slx` : chart Stateflow (logique du firmware v3), câblage, modèle physique (puissance, thermique, séchage, brûleur), scénarios, enregistrement, solveur |
| `scenario_sechoir.m` | Les 12 scénarios de simulation (tableau 14.4 du mémoire) |
| `appliquer_scenario.m` | Charge un scénario dans le modèle |
| `lancer_simulation.m` | Simule, affiche la chronologie, enregistre `captures/scenario_NN.png` |
| `tracer_scenario.m`, `journal_scenario.m` | Figure à 4 graphes et chronologie d'un scénario |
| `comparer_scenario.m` | Validation croisée Simulink / firmware |
| `capturer_modele.m` | Images du modèle et du chart pour le mémoire |
| `preparer_figures_memoire.m` | Figures complémentaires du mémoire V3 depuis les `.mat` (zoom de régulation, humidité, énergie, écarts de validation) |
| `capturer_chart_lisible.m` | Vues lisibles du chart (hiérarchie, MODE_H2, PHASE, URGENCE) sur une copie temporaire du modèle |
| `retracer_scenarios.m` | Retrace les figures de scénario depuis les `.mat`, sans Simulink |
| `outils/schemas_chart.py` | Vues lisibles de MODE_H2, de la région PHASE et de URGENCE_ATEX (A2 à A4) : schéma et conditions exactes du chart (`python3 schemas_chart.py dossier`) |
| `rassembler_figures_memoire.m` | Range toutes les images et tableaux produits sous les noms attendus par `memoire_V4_latex` |
| `A_FAIRE_DANS_MATLAB.md` | Marche à suivre dans MATLAB pour les images et études du mémoire V4 |
| `RESULTATS_SIMULATION.md` | Chiffres, hypothèses et observations des études D3 et E1 à E4 |
| `etudes/` | Études E1 à E4 (journée type, modes de conduite, pertes, sensibilité) : `construire_modele_etudes` (copie du modèle), `lancer_etudes`, `etude_E1_sources` … `etude_E4_sensibilite` ; moteur `'simulink'` ou `'programme'` (programme Arduino réel, `tests/simulation_etudes.cpp`) |
| `reference/` | Résultats attendus : firmware réel + même modèle physique (`make -C tests reference`), avec leurs figures (`tracer_references`) |
| `charger_reference.m`, `tracer_references.m`, `exporter_scenario.m` | Lecture, tracé et export des scénarios de référence |
| `ancien/` | Première approche (compléter le `.slx` fourni), abandonnée |
| `../docs/FSM_SIMULINK.md` | Spécification du chart : hiérarchie, données, actions, transitions, correspondance avec le firmware |

## ⚠️ Important

Les scripts MATLAB **n'ont pas été exécutés dans une vraie session MATLAB**
(pas de MATLAB dans l'environnement de développement). Ils ont été vérifiés
par analyse statique (MISS_HIT) ; les fonctions de tracé et de lecture des
références ont été exécutées sous GNU Octave. Les appels à l'API
Simulink/Stateflow et la simulation elle-même restent à valider : chaque
étape affiche `[ok]` ou l'erreur exacte.
