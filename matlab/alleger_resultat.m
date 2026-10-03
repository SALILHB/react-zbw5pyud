%% alleger_resultat.m  —  réduit le nombre de points d'un résultat pour le tracé
%
%   r = alleger_resultat(r)
%   r = alleger_resultat(r, nMax)     (défaut : environ 4000 points)
%
% La simulation enregistre un point tous les 0,1 s (36 000 points par heure).
% Pour les figures, on garde un point régulier sur k ET tous les instants où
% un signal logique change (état, puissance, vannes, flamme) : les marches et
% les impulsions courtes restent exactes, et la figure reste légère à
% afficher (rendu WebGL des figures depuis MATLAB R2025).

function r = alleger_resultat(r, nMax)

    if nargin < 2
        nMax = 4000;
    end
    n = numel(r.t);
    if n <= nMax
        return;
    end
    k = ceil(n / nMax);
    logiques = [r.P_gaz(:), r.Flame(:), r.fsm.Etat_LCD(:), r.fsm.V_H2(:), r.fsm.V_But(:)];
    changements = find(any(diff(logiques) ~= 0, 2));
    garder = unique([1:k:n, changements(:)', changements(:)' + 1, n]);

    r.t = r.t(garder);
    r.T_sec = r.T_sec(garder);
    r.H_sec = r.H_sec(garder);
    r.P_gaz = r.P_gaz(garder);
    r.Flame = r.Flame(garder);
    noms = fieldnames(r.fsm);
    for i = 1:numel(noms)
        r.fsm.(noms{i}) = r.fsm.(noms{i})(garder);
    end
end
