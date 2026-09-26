%% tracer_scenario.m  —  figure d'un scénario simulé (4 graphes, axe du temps en minutes)
%
%   fig = tracer_scenario(r)     r : résultat de lancer_simulation (ou captures/scenario_NN.mat)
%
%   1. T_sec, T_cible, T_amb et les seuils de palier T1, T2, T3
%   2. puissance de gaz injectée (% de Pnom), source ouverte (H2 / GPL), flamme
%   3. état de la FSM (Etat_LCD)
%   4. humidité de l'air extrait H_sec et seuil de fin H_fin

function fig = tracer_scenario(r)

    sc = r.sc;
    r = alleger_resultat(r);   % ~4000 points au lieu de 36 000 par heure
    t = r.t / 60;
    Tc = valeurSignal(sc.signaux.T_cible, r.t);
    Ta = valeurSignal(sc.signaux.T_amb, r.t);
    Ha = valeurSignal(sc.signaux.H_amb, r.t);
    Hp = valeurSignal(sc.signaux.H_produit, r.t);
    PNOM = 5000;

    % Fenêtre séparée : dans le bureau MATLAB R2025, une figure ancrée ignore
    % Position, et l'image exportée n'aurait pas la bonne taille.
    fig = figure('Color', 'w', 'WindowStyle', 'normal', 'Position', taille_figure(1000, 980), ...
                 'Name', sprintf('Scenario %d', sc.num));

    % --- 1. Températures
    a1 = subplot(4, 1, 1);
    plot(t, r.T_sec, 'LineWidth', 1.6, 'Color', [0.17 0.42 0.69]);
    hold on;
    stairs(t, Tc, '--', 'LineWidth', 1.2, 'Color', [0.75 0.34 0.13]);
    plot(t, Ta, ':', 'LineWidth', 1.2, 'Color', [0.4 0.4 0.4]);
    legendes = {'T_{sec}', 'T_{cible}', 'T_{amb}'};
    if sc.param.Regul_Auto == 1
        tStart = premierAppui(sc.signaux.Btn_Start);
        Tinit = interp1(r.t, r.T_sec, tStart);
        Tcib = interp1(r.t, Tc, tStart);
        if Tcib > Tinit + 3
            for k = 1:2
                Tk = Tinit + k * (Tcib - Tinit) / 3;
                plot([t(1) t(end)], [Tk Tk], ':', 'Color', [0.6 0.6 0.6]);
                text(t(end), Tk, sprintf(' T%d', k), 'FontSize', 8, 'Color', [0.45 0.45 0.45]);
            end
        end
    end
    if max(r.T_sec) > 0.8 * sc.param.T_SEC_MAX_SECURITE
        Tm = sc.param.T_SEC_MAX_SECURITE;
        plot([t(1) t(end)], [Tm Tm], '-', 'Color', [0.77 0.19 0.19]);
        legendes{end + 1} = 'T_{sec} max';
    end
    ylabel(lat(['Temp' char(233) 'rature (' char(176) 'C)']));
    titre = sprintf('Sc%snario %d : %s', char(233), sc.num, sc.nom);
    if isfield(r, 'source')
        titre = [titre ' (' r.source ')'];
    end
    title(lat(titre));
    legend(legendes, 'Location', 'best');
    grid on;

    % --- 2. Puissance et source
    a2 = subplot(4, 1, 2);
    pct = 100 * r.P_gaz / PNOM;
    stairs(t, pct, 'LineWidth', 1.4, 'Color', [0.18 0.52 0.35]);
    hold on;
    yH2 = 108 * ones(size(t));
    yH2(r.fsm.V_H2 == 0) = NaN;
    yGPL = 104 * ones(size(t));
    yGPL(r.fsm.V_But == 0) = NaN;
    plot(t, yH2, '-', 'LineWidth', 4, 'Color', [0.17 0.42 0.69]);
    plot(t, yGPL, '-', 'LineWidth', 4, 'Color', [0.75 0.34 0.13]);
    stairs(t, 15 * r.Flame, '-', 'LineWidth', 0.8, 'Color', [0.55 0.55 0.55]);
    ylim([0 115]);
    set(gca, 'YTick', [0 33 67 100]);
    ylabel('Puissance (% Pnom)');
    legend({'palier de gaz', 'vanne H_2', 'vanne GPL', 'flamme (0/15)'}, 'Location', 'best');
    grid on;

    % --- 3. États
    a3 = subplot(4, 1, 3);
    stairs(t, r.fsm.Etat_LCD, 'LineWidth', 1.4, 'Color', [0.42 0.27 0.76]);
    codes = [0 2 3 4 5 6 7 8 9];
    noms = {'ATTENTE', 'SOLAIRE', 'H2', 'GPL', 'DEMANDE', 'PROLONG.', ...
            lat(['TERMIN' char(201)]), 'ERREUR', 'URGENCE'};
    set(gca, 'YTick', codes, 'YTickLabel', noms);
    ylim([-0.5 9.5]);
    ylabel(lat([char(201) 'tat FSM']));
    grid on;

    % --- 4. Humidité
    a4 = subplot(4, 1, 4);
    plot(t, r.H_sec, 'LineWidth', 1.6, 'Color', [0.17 0.42 0.69]);
    hold on;
    stairs(t, min(Hp + Ha, 95), '--', 'LineWidth', 1.2, 'Color', [0.75 0.34 0.13]);
    legendes = {lat(['H_{sec} (mesur' char(233) 'e)']), 'H_{fin}'};
    if sc.param.Fixe_H_sec == 1
        Hv = sc.param.Val_H_sec;
        plot([t(1) t(end)], [Hv Hv], '-.', 'LineWidth', 1.2, 'Color', [0.4 0.4 0.4]);
        legendes{end + 1} = ['H_{sec} FIXE (' num2str(Hv) ' %)'];
    end
    ylabel(lat(['Humidit' char(233) ' (% HR)']));
    xlabel('Temps (min)');
    legend(legendes, 'Location', 'best');
    grid on;

    linkaxes([a1 a2 a3 a4], 'x');
    xlim(a1, [t(1) t(end)]);
    drawnow;   % rendu terminé avant l'export (affichage asynchrone des figures)
end

function v = valeurSignal(m, t)
    % Valeur d'un signal [t valeur] (maintenue entre deux points) aux instants t.
    v = zeros(size(t));
    for k = 1:size(m, 1)
        v(t >= m(k, 1)) = m(k, 2);
    end
end

function t0 = premierAppui(m)
    k = find(m(:, 2) ~= 0, 1);
    if isempty(k)
        t0 = 0;
    else
        t0 = m(k, 1);
    end
end

function s = lat(s)
    % Texte avec accents écrits par leur code (char(233) = é) : correct tel
    % quel dans MATLAB ; converti en UTF-8 pour Octave.
    if exist('OCTAVE_VERSION', 'builtin')
        s = native2unicode(uint8(s), 'latin1');
    end
end
