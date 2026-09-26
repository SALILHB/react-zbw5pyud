%% charger_reference.m  —  courbes de référence d'un scénario (firmware réel)
%
%   r = charger_reference(n)
%
% Lit reference/scenario_NN.csv, produit par le firmware Arduino compilé sur
% PC avec le même modèle physique que Simulink (make -C tests reference), et
% le met sous la même forme que le résultat de lancer_simulation : on peut
% donc appeler tracer_scenario(r) et journal_scenario(r) dessus.

function r = charger_reference(n)

    dossier = fileparts(mfilename('fullpath'));
    fichier = fullfile(dossier, 'reference', sprintf('scenario_%02d.csv', n));
    M = dlmread(fichier, ',', 1, 0);
    % colonnes : t,T_sec,H_sec,P_gaz,Flame,V_Fl_1,V_Fl_2,V_Fl_3,V_H2,V_But,Etat
    % Le fichier ne contient qu'un point toutes les 10 s et à chaque
    % changement : rééchantillonnage à 0,5 s (valeur maintenue pour les
    % grandeurs logiques, interpolation linéaire pour T_sec et H_sec).
    [tf, k] = unique(M(:, 1), 'last');
    M = M(k, :);
    t = (0:0.5:tf(end))';
    r.t = t;
    r.T_sec = interp1(tf, M(:, 2), t, 'linear');
    r.H_sec = interp1(tf, M(:, 3), t, 'linear');
    L = interp1(tf, M(:, 4:11), t, 'previous');
    r.P_gaz = L(:, 1);
    r.Flame = L(:, 2);
    r.fsm.V_Fl_1 = L(:, 3);
    r.fsm.V_Fl_2 = L(:, 4);
    r.fsm.V_Fl_3 = L(:, 5);
    r.fsm.V_H2 = L(:, 6);
    r.fsm.V_But = L(:, 7);
    r.fsm.Etat_LCD = L(:, 8);
    r.sc = scenario_sechoir(n);
    r.source = 'firmware';
end
