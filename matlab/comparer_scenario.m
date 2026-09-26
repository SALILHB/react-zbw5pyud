%% comparer_scenario.m  —  validation croisée Simulink / firmware d'un scénario
%
%   comparer_scenario(n)
%
% Superpose la simulation Simulink (captures/scenario_NN.mat, écrit par
% lancer_simulation) et la référence firmware (reference/scenario_NN.csv),
% liste les changements d'état des deux côtés avec l'écart de temps, et
% enregistre captures/comparaison_NN.png.

function comparer_scenario(n)

    dossier = fileparts(mfilename('fullpath'));
    donnees = load(fullfile(dossier, 'captures', sprintf('scenario_%02d.mat', n)));
    s = donnees.r;
    f = charger_reference(n);

    % Fenêtre séparée : dans le bureau MATLAB R2025, une figure ancrée ignore
    % Position, et l'image exportée n'aurait pas la bonne taille.
    fig = figure('Color', 'w', 'WindowStyle', 'normal', 'Position', [80 80 1000 650], ...
                 'Name', sprintf('Comparaison scenario %d', n));
    a1 = subplot(2, 1, 1);
    plot(s.t / 60, s.T_sec, 'LineWidth', 1.8, 'Color', [0.17 0.42 0.69]);
    hold on;
    plot(f.t / 60, f.T_sec, '--', 'LineWidth', 1.4, 'Color', [0.75 0.34 0.13]);
    ylabel(lat(['T_{sec} (' char(176) 'C)']));
    title(lat(sprintf('Sc%snario %d : %s - Simulink / firmware', char(233), n, s.sc.nom)));
    legend({'Simulink (Stateflow)', 'Firmware Arduino'}, 'Location', 'best');
    grid on;
    a2 = subplot(2, 1, 2);
    stairs(s.t / 60, s.fsm.Etat_LCD, 'LineWidth', 1.8, 'Color', [0.17 0.42 0.69]);
    hold on;
    stairs(f.t / 60, f.fsm.Etat_LCD, '--', 'LineWidth', 1.4, 'Color', [0.75 0.34 0.13]);
    set(gca, 'YTick', [0 2 3 4 5 6 7 8 9], 'YTickLabel', {'ATTENTE', 'SOLAIRE', 'H2', 'GPL', ...
        'DEMANDE', 'PROLONG.', lat(['TERMIN' char(201)]), 'ERREUR', 'URGENCE'});
    ylim([-0.5 9.5]);
    xlabel('Temps (min)');
    grid on;
    linkaxes([a1 a2], 'x');
    drawnow;

    fichier = fullfile(dossier, 'captures', sprintf('comparaison_%02d.png', n));
    if ~isempty(which('exportgraphics'))
        exportgraphics(fig, fichier, 'Resolution', 300);
    else
        print(fig, fichier, '-dpng', '-r300');
    end

    [ts, es] = changements(s);
    [tf, ef] = changements(f);
    fprintf('--- Scenario %d : changements d''etat (Simulink / firmware) ---\n', n);
    for k = 1:max(numel(ts), numel(tf))
        if k <= numel(ts) && k <= numel(tf)
            fprintf('  %2d  %-9s %8.1f s | %-9s %8.1f s | ecart %+6.1f s\n', k, ...
                nomCourt(es(k)), ts(k), nomCourt(ef(k)), tf(k), ts(k) - tf(k));
        elseif k <= numel(ts)
            fprintf('  %2d  %-9s %8.1f s | (absent cote firmware)\n', k, nomCourt(es(k)), ts(k));
        else
            fprintf('  %2d  (absent cote Simulink) | %-9s %8.1f s\n', k, nomCourt(ef(k)), tf(k));
        end
    end
    fprintf('  T_sec max : Simulink %.1f C, firmware %.1f C\n', max(s.T_sec), max(f.T_sec));
    fprintf('Figure : captures/comparaison_%02d.png\n', n);
end

function [t, e] = changements(r)
    etat = round(r.fsm.Etat_LCD(:));
    k = [1; find(diff(etat) ~= 0) + 1];
    t = r.t(k);
    e = etat(k);
end

function s = nomCourt(code)
    noms = {'ATTENTE', 'MENU', 'SOLAIRE', 'H2', 'GPL', 'DEMANDE', 'PROLONG.', 'TERMINE', 'ERREUR', 'URGENCE'};
    if code >= 0 && code <= 9
        s = noms{code + 1};
    else
        s = sprintf('code %d', code);
    end
end

function s = lat(s)
    % Texte avec accents écrits par leur code (char(233) = é) : correct tel
    % quel dans MATLAB ; converti en UTF-8 pour Octave.
    if exist('OCTAVE_VERSION', 'builtin')
        s = native2unicode(uint8(s), 'latin1');
    end
end
