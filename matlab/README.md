# Séchoir solaire hybride — Modèle Simulink/Stateflow

Modèle Simulink de la commande, dont le chart Stateflow `FSM` reproduit la
logique du firmware Arduino `sechoir_hybride/` (v3), qui fait foi.

## Contenu

| Fichier | Rôle |
|---|---|
| `Commande_Sechoir_Hybride_corrige.slx` | **Modèle à utiliser** : chart FSM d'origine + corrections physiques (Pnom = 5000, Kth = 0,098, 1/tauth = 1/3530) |
| `Commande_Sechoir_Hybride.slx` | Modèle tel que fourni à l'origine (référence) |
| `completer_fsm_sechoir.m` | Script qui complète le chart FSM via l'API Stateflow |
| `../docs/FSM_SIMULINK.md` | Spécification complète : hiérarchie, données, actions, transitions, correspondance avec le firmware |

## Utilisation

1. Ouvrir `Commande_Sechoir_Hybride_corrige.slx` dans MATLAB/Simulink.
2. Dans le dossier `matlab/`, exécuter :
   `completer_fsm_sechoir('Commande_Sechoir_Hybride_corrige')`
3. Si la sortie affiche `[HIERARCHIE INCORRECTE]` : glisser-déposer les états
   signalés dans leur boîte parente, régler `EN_CYCLE` en décomposition
   *AND (parallel)*, puis relancer le script (il est relançable).
4. Clic droit sur le chart → **Update Chart**.
5. Câbler les deux nouvelles entrées `Btn_Stop` et `Btn_Rearm` (Constant à 0
   pour commencer) ; régler `Tps_Prolongation` **en secondes** (1800 = 30 min).
6. Simuler, puis consulter le Diagnostic Viewer.

## ⚠️ Important

Le script **n'a pas été exécuté dans une vraie session MATLAB** (pas de MATLAB
dans l'environnement de développement). Sa syntaxe et celle des 37 libellés
qu'il génère ont été vérifiées par analyse statique (MISS_HIT), mais pas les
appels d'API Stateflow ni la simulation. Voir `docs/FSM_SIMULINK.md` §8.
