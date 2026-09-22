# Séchoir solaire hybride — Modèle Simulink/Stateflow

Complète `sechoir_hybride/` (firmware Arduino) avec la même FSM, cette fois
sous forme de modèle Simulink pour la validation par simulation (mémoire).

## Contenu

| Fichier | Rôle |
|---|---|
| `Commande_Sechoir_Hybride.slx` | Modèle fourni par l'opérateur (partiellement construit) |
| `completer_fsm_sechoir.m` | Script qui complète le chart Stateflow via l'API officielle |
| `../docs/FSM_SIMULINK.md` | Spécification complète (dictionnaire de données, états, transitions, avis critiques) |

## Utilisation

1. Ouvrir `Commande_Sechoir_Hybride.slx` dans MATLAB/Simulink.
2. Exécuter `completer_fsm_sechoir` (ou `completer_fsm_sechoir('AutreNomDeModele')`
   si le modèle a été renommé).
3. Dans Simulink : clic droit sur le chart → **Update Chart**.
4. Recâbler manuellement les entrées manquantes du sous-système `FSM`
   (voir `docs/FSM_SIMULINK.md`, Avis n°2).

## ⚠️ Important

Ce script **n'a pas été exécuté ni vérifié dans une vraie session MATLAB**
(environnement de développement sans MATLAB/Simulink disponible). Il est
écrit avec l'API Stateflow standard et est idempotent (relançable sans
dupliquer ce qui existe déjà), mais la première exécution doit être
considérée comme un essai à valider, pas comme un résultat garanti.
Consultez le Diagnostic Viewer de Simulink après l'exécution.
