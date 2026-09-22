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
3. Si le script affiche `[STRUCTURE INCOMPLETE]` : le reparentage
   automatique de `MODE_SOLAIRE`/`MODE_H2`/`MODE_GPL`/`PROLONGATION`/
   `FIN_TEMPORISATION` dans les nouvelles régions parallèles `SOURCE`/
   `PHASE` a échoué sur votre version de MATLAB. Glissez-déposez ces 5
   états dans les boîtes `SOURCE`/`PHASE` correspondantes dans l'éditeur
   Stateflow, puis relancez le script (idempotent) pour compléter le reste.
4. Dans Simulink : clic droit sur le chart → **Update Chart**.
5. Recâbler manuellement les entrées manquantes du sous-système `FSM`
   (voir `docs/FSM_SIMULINK.md`, dictionnaire de données §2).

## ⚠️ Important

Ce script **n'a pas été exécuté ni vérifié dans une vraie session MATLAB**
(environnement de développement sans MATLAB/Simulink disponible). Il est
écrit avec l'API Stateflow standard et est idempotent (relançable sans
dupliquer ce qui existe déjà), mais la première exécution doit être
considérée comme un essai à valider, pas comme un résultat garanti.
Consultez le Diagnostic Viewer de Simulink après l'exécution.
