%% enregistrer_etude.m  —  enregistre une figure d'étude (PNG 300 dpi) et la ferme
%
%   enregistrer_etude(fig, fichier)

function enregistrer_etude(fig, fichier)

    dossier = fileparts(fichier);
    if ~exist(dossier, 'dir')
        mkdir(dossier);
    end
    exporter_figure(fig, fichier);
    close(fig);
    fprintf('  [ok] %s\n', fichier);
end
