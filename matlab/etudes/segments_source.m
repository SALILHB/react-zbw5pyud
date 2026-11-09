%% segments_source.m  —  intervalles pendant lesquels chaque source est active
%
%   S = segments_source(r)
%
% S : matrice [t_debut t_fin code], code = 2 (solaire), 3 (H2), 4 (GPL),
% 8 (erreur de combustion), d'après l'état de la FSM.

function S = segments_source(r)

    t = r.t(:);
    e = round(r.fsm.Etat_LCD(:));
    e(~ismember(e, [2 3 4 8])) = 0;
    k = [1; find(diff(e) ~= 0) + 1];
    fin = [t(k(2:end)); t(end)];
    S = [t(k), fin, e(k)];
    S = S(S(:, 3) ~= 0, :);
end
