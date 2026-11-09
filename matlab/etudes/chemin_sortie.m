%% chemin_sortie.m  —  chemin d'un fichier produit par les études
%
%   f = chemin_sortie(racine, relatif)
%
% racine : dossier memoire_V4_latex (les fichiers vont directement à leur
% place) ou, si vide, matlab/captures/etudes (même arborescence).
% relatif : par exemple 'Figures/simulation/profil_journee.png'.

function f = chemin_sortie(racine, relatif)

    if nargin < 1 || isempty(racine)
        racine = fullfile(fileparts(fileparts(mfilename('fullpath'))), 'captures', 'etudes');
    end
    morceaux = strsplit(relatif, '/');
    f = fullfile(racine, morceaux{:});
    dossier = fileparts(f);
    if ~exist(dossier, 'dir')
        mkdir(dossier);
    end
end
