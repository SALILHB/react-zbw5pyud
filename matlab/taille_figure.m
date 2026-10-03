%% taille_figure.m  —  position d'une figure qui tient dans l'écran
%
%   pos = taille_figure(largeur, hauteur)
%
% Réduit la taille demandée si l'écran est plus petit (portable), pour que la
% fenêtre et l'image exportée soient complètes.

function pos = taille_figure(largeur, hauteur)
    ecran = get(0, 'ScreenSize');
    largeur = min(largeur, ecran(3) - 80);
    hauteur = min(hauteur, ecran(4) - 120);
    pos = [40, max(40, ecran(4) - hauteur - 80), largeur, hauteur];
end
