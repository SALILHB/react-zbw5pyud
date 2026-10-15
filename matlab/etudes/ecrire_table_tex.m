%% ecrire_table_tex.m  —  corps d'un tableau LaTeX lu par le mémoire
%
%   ecrire_table_tex(fichier, lignes, nCol)
%
% lignes : cellule de lignes, chaque ligne étant une cellule de nCol textes.
% Écrit « a & b & c \\ \hline » par ligne, en UTF-8, sans en-tête. Vérifie le
% nombre de colonnes (sinon la compilation du mémoire échouerait) et échappe
% les caractères %, _ et # qui ne le sont pas déjà.

function ecrire_table_tex(fichier, lignes, nCol)

    dossier = fileparts(fichier);
    if ~isempty(dossier) && ~exist(dossier, 'dir')
        mkdir(dossier);
    end
    fid = fopen(fichier, 'w', 'n', 'UTF-8');
    if fid < 0
        error('ecrire_table_tex:fichier', 'Impossible d''ecrire %s', fichier);
    end
    for i = 1:numel(lignes)
        l = lignes{i};
        if numel(l) ~= nCol
            fclose(fid);
            error('ecrire_table_tex:colonnes', 'Ligne %d : %d colonnes au lieu de %d.', i, numel(l), nCol);
        end
        l = regexprep(l, '(?<!\\)([%_#])', '\\$1');
        fprintf(fid, '%s \\\\ \\hline\n', strjoin(l, ' & '));
    end
    fclose(fid);
    fprintf('  [ok] %s\n', fichier);
end
