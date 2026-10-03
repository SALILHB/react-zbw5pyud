%% couleur_etude.m  —  couleurs communes aux figures des études
%
%   c = couleur_etude(nom)
%   nom : bleu, orange, vert, rouge, violet, gris, solaire, h2, gpl, erreur, bande

function c = couleur_etude(nom)

    switch lower(nom)
        case {'bleu', 'h2'}
            c = [0.17 0.42 0.69];
        case {'orange', 'gpl'}
            c = [0.75 0.34 0.13];
        case 'vert'
            c = [0.18 0.52 0.35];
        case {'rouge', 'erreur'}
            c = [0.77 0.19 0.19];
        case 'violet'
            c = [0.42 0.27 0.76];
        case 'gris'
            c = [0.45 0.45 0.45];
        case 'solaire'
            c = [0.93 0.66 0.10];
        case 'bande'
            c = [0.80 0.90 0.80];
        otherwise
            error('couleur_etude:nom', 'Couleur %s inconnue.', nom);
    end
end
