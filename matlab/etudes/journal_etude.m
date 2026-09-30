%% journal_etude.m  —  ajoute des lignes au compte rendu d'une étude (UTF-8)
%
%   journal_etude(fichier, format, ...)      comme fprintf, dans le fichier ET à l'écran
%   journal_etude(fichier)                   vide le fichier

function journal_etude(fichier, varargin)

    if isempty(varargin)
        fid = fopen(fichier, 'w', 'n', 'UTF-8');
        fclose(fid);
        return;
    end
    texte = sprintf(varargin{:});
    fid = fopen(fichier, 'a', 'n', 'UTF-8');
    fprintf(fid, '%s', texte);
    fclose(fid);
    fprintf('%s', texte);
end
