%% capturer_modele.m  —  images du modèle pour le mémoire
%
%   capturer_modele()
%   capturer_modele(modele)
%
% Enregistre dans captures/ :
%   modele_global.png      diagramme principal (boucle fermée)
%   modele_fsm.png         sous-système FSM (câblage du chart)
%   modele_thermique.png   sous-système MODELE_THERMIQUE
%   chart_fsm.png          chart Stateflow complet
% Les images sont vectorisées puis rendues à 300 dpi : meilleures qu'une
% capture d'écran. Pour l'animation Stateflow (état actif surligné), voir
% matlab/GUIDE_SIMULATION.md : elle se capture à l'écran.

function capturer_modele(modele)

    if nargin < 1
        modele = 'Simulation_Sechoir_Hybride';
    end
    dossier = fileparts(mfilename('fullpath'));
    sortie = fullfile(dossier, 'captures');
    if ~exist(sortie, 'dir')
        mkdir(sortie);
    end
    if ~bdIsLoaded(modele)
        load_system(fullfile(dossier, [modele '.slx']));
    end

    exporterSysteme(modele, fullfile(sortie, 'modele_global.png'));
    exporterSysteme([modele '/FSM'], fullfile(sortie, 'modele_fsm.png'));
    exporterSysteme([modele '/MODELE_THERMIQUE'], fullfile(sortie, 'modele_thermique.png'));

    chart = [modele '/FSM/Chart'];
    fichier = fullfile(sortie, 'chart_fsm.png');
    try
        sfprint(chart, 'png', fichier, 1);
        fprintf('  [ok] %s\n', fichier);
    catch e
        fprintf('  [sfprint indisponible : %s] essai avec print\n', e.message);
        exporterSysteme(chart, fichier);
    end
end

function exporterSysteme(sys, fichier)
    try
        open_system(sys);
        print(['-s' sys], '-dpng', '-r300', fichier);
        fprintf('  [ok] %s\n', fichier);
    catch e
        try
            saveas(get_param(sys, 'Handle'), fichier, 'png');
            fprintf('  [ok, saveas] %s\n', fichier);
        catch e2
            fprintf('  [ERREUR] %s : %s / %s\n', sys, e.message, e2.message);
        end
    end
end
