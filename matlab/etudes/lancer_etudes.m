%% lancer_etudes.m  —  lance les études E1 à E4 du mémoire V4
%
%   lancer_etudes                         moteur par défaut (Simulink sous MATLAB)
%   lancer_etudes(moteur)                 'simulink' ou 'programme'
%   lancer_etudes(moteur, racine)         racine = dossier memoire_V4_latex :
%                                         figures et tableaux écrits à leur place
%
% Moteur 'simulink' : lancer une fois construire_modele_etudes avant.
% Comptes rendus chiffrés : captures/etudes/resultats_E1.txt ... E4.txt
% (à reporter dans RESULTATS_SIMULATION.md).

function lancer_etudes(moteur, racine)

    if nargin < 1
        moteur = '';
    end
    if nargin < 2
        racine = '';
    end
    ici = fileparts(mfilename('fullpath'));
    addpath(ici);
    addpath(fileparts(ici));
    debut = tic;
    etude_E1_sources(moteur, racine);
    etude_E2_modes(moteur, racine);
    etude_E3_pertes(moteur, racine);
    etude_E4_sensibilite(moteur, racine);
    fprintf('=== Etudes E1 a E4 terminees en %.0f s ===\n', toc(debut));
end
