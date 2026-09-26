# Séchoir solaire hybride — Modèle Simulink/Stateflow

Modèle Simulink de la commande, dont le chart Stateflow `FSM` reproduit la
logique du firmware Arduino `sechoir_hybride/` (v3), qui fait foi.

Version cible : **MATLAB R2025b** (Simulink + Stateflow).

**Pour simuler : suivre [`GUIDE_SIMULATION.md`](GUIDE_SIMULATION.md)**
(4 commandes : `preparer_simulation`, `lancer_simulation(1:12)`,
`comparer_scenario(n)`, `capturer_modele`).

## Contenu

| Fichier | Rôle |
|---|---|
| `Commande_Sechoir_Hybride_corrige.slx` | Modèle de départ : chart FSM d'origine + corrections physiques (Pnom = 5000, Kth = 0,098, 1/tauth = 1/3530). Jamais modifié par les scripts |
| `Commande_Sechoir_Hybride.slx` | Modèle tel que fourni à l'origine (référence) |
| `completer_fsm_sechoir.m` | Écrit la logique du firmware dans le chart FSM via l'API Stateflow |
| `preparer_simulation.m` | Construit `Simulation_Sechoir_Hybride.slx` : chart complété, câblage des 24 entrées, brûleur, séchage, apport solaire, scénarios, enregistrement, solveur |
| `scenario_sechoir.m` | Les 12 scénarios de simulation (tableau 14.4 du mémoire) |
| `appliquer_scenario.m` | Charge un scénario dans le modèle |
| `lancer_simulation.m` | Simule, affiche la chronologie, enregistre `captures/scenario_NN.png` |
| `tracer_scenario.m`, `journal_scenario.m` | Figure à 4 graphes et chronologie d'un scénario |
| `comparer_scenario.m` | Validation croisée Simulink / firmware |
| `capturer_modele.m` | Images du modèle et du chart pour le mémoire |
| `reference/` | Résultats attendus : firmware réel + même modèle physique (`make -C tests reference`), avec leurs figures (`tracer_references`) |
| `charger_reference.m`, `tracer_references.m`, `exporter_scenario.m` | Lecture, tracé et export des scénarios de référence |
| `../docs/FSM_SIMULINK.md` | Spécification du chart : hiérarchie, données, actions, transitions, correspondance avec le firmware |

## ⚠️ Important

Les scripts MATLAB **n'ont pas été exécutés dans une vraie session MATLAB**
(pas de MATLAB dans l'environnement de développement). Ils ont été vérifiés
par analyse statique (MISS_HIT) ; les fonctions de tracé et de lecture des
références ont été exécutées sous GNU Octave. Les appels à l'API
Simulink/Stateflow et la simulation elle-même restent à valider : chaque
étape affiche `[ok]` ou l'erreur exacte.
