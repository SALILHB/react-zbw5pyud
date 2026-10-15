%% etude_E4_sensibilite.m  —  E4 : sensibilité aux paramètres et perturbations
%
%   etude_E4_sensibilite
%   etude_E4_sensibilite(moteur)            'simulink' (défaut MATLAB) ou 'programme'
%   etude_E4_sensibilite(moteur, racine)    racine = dossier memoire_V4_latex
%
% Cas de base : scénario 1 (T_amb = 25 °C, T_cible = 55 °C), prolongé à 2 h
% pour disposer d'au moins trois cycles de veille dans chaque variante.
% Régime établi : à partir de la première coupure (entrée en veille, ≈ 600 s).
% Grandeurs par variante : amplitude (max - min de T_sec, °C), période
% moyenne entre deux rallumages (min), rallumages par heure, puissance de gaz
% moyenne P (W). Une variante à la fois.
% Produit :
%   Figures/simulation/sensibilite_retard.png    retards de mesure 0, 10, 30, 60 s
%   Figures/simulation/sensibilite_ua.png        UA -30 %, nominal, +30 %
%   Figures/simulation/perturbation_porte.png    porte ouverte 60 s à 1800 s
%   Figures/simulation/tables/tab_sensibilite.tex   6 colonnes
%   captures/etudes/resultats_E4.txt (avec les valeurs théoriques t_on, t_off)

function res = etude_E4_sensibilite(moteur, racine)

    if nargin < 1
        moteur = '';
    end
    if nargin < 2
        racine = '';
    end
    ici = fileparts(mfilename('fullpath'));
    addpath(ici);
    addpath(fileparts(ici));
    fr = chemin_sortie('', 'resultats_E4.txt');
    journal_etude(fr);
    journal_etude(fr, '=== E4 : sensibilite (scenario 1 prolonge a 2 h) ===\n');

    base = cas_etude('E4');
    base.StopTime = 7200;
    UA0 = base.etude.UA;
    C0 = base.etude.Ceq;

    % Variantes : {paramètre, valeur affichée, réglage}
    V = {
        'Conductance UA (W/K)', '7,1 (-30 %)', @(sc) regler(sc, 'UA', 0.7 * UA0)
        'Conductance UA (W/K)', '10,2 (nominal)', @(sc) sc
        'Conductance UA (W/K)', '13,3 (+30 %)', @(sc) regler(sc, 'UA', 1.3 * UA0)
        'UA + renouvellement d''air (W/K)', '34 (80 m³/h)', @(sc) regler(sc, 'UA', UA0 + 23.7)
        'UA + renouvellement d''air (W/K)', '69 (200 m³/h)', @(sc) regler(sc, 'UA', UA0 + 59.2)
        'Température ambiante (°C)', '15', @(sc) ambiante(sc, 15, 0)
        'Température ambiante (°C)', '35 (réglages par défaut)', @(sc) ambiante(sc, 35, 0)
        'Température ambiante (°C)', '35 (Marge\_Sol = 20 °C)', @(sc) ambiante(sc, 35, 20)
        'Capacité thermique', 'C\_eq × 2', @(sc) regler(sc, 'Ceq', 2 * C0)
        'Retard de mesure (s)', '10', @(sc) regler(sc, 'retard', 10)
        'Retard de mesure (s)', '30', @(sc) regler(sc, 'retard', 30)
        'Retard de mesure (s)', '60', @(sc) regler(sc, 'retard', 60)
        'Bruit de mesure (°C)', '± 0,25', @(sc) bruit(sc, 0.25)
        'Bruit de mesure (°C)', '± 0,5', @(sc) bruit(sc, 0.5)
        };
    n = size(V, 1);
    R = cell(1, n);
    M = cell(1, n);
    lignes = cell(1, n);
    journal_etude(fr, '%-34s %-26s %9s %9s %9s %8s\n', 'Parametre', 'Valeur', 'ampl(C)', 'per(min)', ...
                  'rall/h', 'P(W)');
    for i = 1:n
        sc = V{i, 3}(base);
        R{i} = simuler_etude(sc, moteur);
        M{i} = regime(R{i});
        m = M{i};
        journal_etude(fr, '%-34s %-26s %9.2f %9.1f %9.1f %8.0f\n', V{i, 1}, V{i, 2}, m.amplitude, ...
                      m.periode, m.rallumages_h, m.P_moy);
        lignes{i} = {V{i, 1}, V{i, 2}, nombre_fr(m.amplitude, 1), nombre_fr(m.periode, 1), ...
                     nombre_fr(m.rallumages_h, 1), nombre_fr(m.P_moy, 0)};
    end
    journal_etude(fr, 'Moteur : %s ; regime etabli a partir de %.0f s (cas nominal)\n', R{2}.moteur, M{2}.t0);
    ecrire_table_tex(chemin_sortie(racine, 'Figures/simulation/tables/tab_sensibilite.tex'), lignes, 6);

    theorie(fr, UA0, C0, 25);
    for i = [1 3 4 5 6 9]
        journal_etude(fr, '  (%s = %s) : ', V{i, 1}, V{i, 2});
        sc = V{i, 3}(base);
        theorieCourte(fr, sc.etude.UA, sc.etude.Ceq, sc.signaux.T_amb(1, 2));
    end
    for i = 10:12
        journal_etude(fr, 'Retard %s s : depassement haut %.2f C (max %.2f), bas %.2f C (min %.2f) ; attendu ~ pente x retard\n', ...
                      V{i, 2}, M{i}.T_max - 57.5, M{i}.T_max, 52.5 - M{i}.T_min, M{i}.T_min);
    end
    for i = 1:n
        if M{i}.rallumages_h == 0
            journal_etude(fr, 'Sans veille ni rallumage : %s = %s -> paliers %s %% apres %.0f s, T_sec finale %.2f C\n', ...
                          V{i, 1}, V{i, 2}, mat2str(M{i}.paliers), M{i}.t0, M{i}.T_fin);
        end
    end
    solaire = bilan_etude(R{7});
    journal_etude(fr, 'T_amb = 35 C avec les reglages par defaut : T_cap estimee = 55 C >= seuil ON ; MODE_SOLAIRE des %s s, energie de gaz %.2f kWh\n', ...
                  nombre_fr(solaire.t_solaire, 0), solaire.E_gaz);

    figureRetard(R(2), R(10:12), chemin_sortie(racine, 'Figures/simulation/sensibilite_retard.png'));
    figureUA(R(1:3), {'7,1 W/K', '10,2 W/K (nominal)', '13,3 W/K'}, ...
             chemin_sortie(racine, 'Figures/simulation/sensibilite_ua.png'));
    porte = etudePorte(base, moteur, fr, chemin_sortie(racine, 'Figures/simulation/perturbation_porte.png'));

    res.variantes = V(:, 1:2);
    res.mesures = M;
    res.porte = porte;
    save(chemin_sortie('', 'E4.mat'), 'res');
    journal_etude(fr, '=== E4 termine ===\n');
end

%% =====================================================================
function sc = regler(sc, champ, valeur)
    sc.etude.(champ) = valeur;
end

function sc = ambiante(sc, Tamb, marge)
    sc.signaux.T_amb = [0 Tamb; 1 Tamb];
    sc.Tsec0 = Tamb;
    sc.param.Marge_Sol = marge;
end

function sc = bruit(sc, amplitude)
    sc.signaux.Bruit_T = bruit_mesure(amplitude, sc.StopTime);
    sc.etude.quantif = 1;
end

function m = regime(r)
    % Grandeurs du régime établi, à partir de la première entrée en veille.
    t = r.t(:);
    P = r.P_gaz(:);
    k0 = find(t > 130 & P == 0 & [0; P(1:end - 1)] > 0, 1);
    if isempty(k0)
        % Jamais de veille (palier 33 % insuffisant, ou pas de combustion) :
        % régime établi pris sur la seconde moitié de la simulation.
        k0 = find(t >= t(end) / 2, 1);
    end
    m.t0 = t(k0);
    m.T_fin = r.T_sec(end);
    m.paliers = unique(round(100 * P(t >= m.t0) / 5000))';
    fen = t >= m.t0;
    T = r.T_sec(fen);
    m.amplitude = max(T) - min(T);
    m.T_max = max(T);
    m.T_min = min(T);
    spark = r.fsm.Spark(:) ~= 0;
    fronts = t(find(diff([0; spark]) > 0));
    fronts = fronts(fronts >= m.t0);
    duree = t(end) - m.t0;
    m.rallumages_h = numel(fronts) / (duree / 3600);
    if numel(fronts) >= 2
        m.periode = mean(diff(fronts)) / 60;
    else
        m.periode = NaN;
    end
    w = [diff(t); 0];
    m.P_moy = sum(P(fen) .* w(fen)) / duree;
end

function theorie(fr, UA, Ceq, Tamb)
    journal_etude(fr, 'Valeurs theoriques (modele du 1er ordre, palier 33 %% = 1650 W, bande 52,5-57,5 C) :\n  nominal : ');
    theorieCourte(fr, UA, Ceq, Tamb);
end

function theorieCourte(fr, UA, Ceq, Tamb)
    tau = Ceq / UA;
    Tinf = Tamb + 1650 / UA;
    if Tinf <= 57.5
        journal_etude(fr, 'T_inf(33 %%) = %.1f C < 57,5 C : le palier 33 %% ne suffit plus\n', Tinf);
        return;
    end
    ton = tau * log((Tinf - 52.5) / (Tinf - 57.5));
    toff = tau * log((57.5 - Tamb) / (52.5 - Tamb));
    journal_etude(fr, 'tau = %.0f s, t_on = %.0f s, t_off = %.0f s, periode = %.1f min, P = %.0f W, pente chauffe %.4f K/s, refroidissement %.4f K/s\n', ...
                  tau, ton, toff, (ton + toff) / 60, 1650 * ton / (ton + toff), (Tinf - 57.5) / tau, (52.5 - Tamb) / tau);
end

function figureRetard(ref, R, fichier)
    retards = [0 10 30 60];
    tous = [ref, R];
    styles = {'-', '--', '-.', ':'};
    couleurs = {[0.1 0.1 0.1], couleur_etude('bleu'), couleur_etude('violet'), couleur_etude('rouge')};
    fig = figure_etude(15, 9);
    hold on;
    hB = patch([8 45 45 8], [52.5 52.5 57.5 57.5], couleur_etude('bande'), 'EdgeColor', 'none');
    hs = zeros(1, 4);
    noms = cell(1, 4);
    for i = 1:4
        hs(i) = plot(tous{i}.t / 60, tous{i}.T_sec, styles{i}, 'LineWidth', 1.3, 'Color', couleurs{i});
        noms{i} = sprintf('retard %d s', retards(i));
    end
    legend([hs hB], [noms, {'bande 52,5-57,5 °C'}], 'Location', 'southoutside', 'NumColumns', 5, 'FontSize', 8);
    xlim([8 45]);
    ylim([49 62]);
    xlabel('Temps (min)');
    ylabel('T_{sec} vraie (°C)');
    title('Cycle de veille selon le retard de la mesure de T_{sec}');
    grid on;
    box on;
    enregistrer_etude(fig, fichier);
end

function figureUA(R, noms, fichier)
    styles = {'--', '-', ':'};
    couleurs = {couleur_etude('bleu'), [0.1 0.1 0.1], couleur_etude('rouge')};
    fig = figure_etude(15, 8);
    hold on;
    patch([0 120 120 0], [52.5 52.5 57.5 57.5], couleur_etude('bande'), 'EdgeColor', 'none');
    hs = zeros(1, 3);
    for i = 1:3
        hs(i) = plot(R{i}.t / 60, R{i}.T_sec, styles{i}, 'LineWidth', 1.3, 'Color', couleurs{i});
    end
    % (pas de « % » dans la légende : l'export d'Octave échoue)
    legende = cellfun(@(x) ['UA = ' x], noms, 'UniformOutput', false);
    legend(hs, legende, 'Location', 'southoutside', 'NumColumns', 3, 'FontSize', 8);
    xlim([0 120]);
    ylim([20 62]);
    set(gca, 'XTick', 0:15:120);
    xlabel('Temps (min)');
    ylabel('T_{sec} (°C)');
    title('Cycle de veille selon la conductance UA');
    grid on;
    box on;
    enregistrer_etude(fig, fichier);
end

function p = etudePorte(base, moteur, fr, fichier)
    tP = 1800;
    sc = base;
    sc.signaux.UA_facteur = [0 1; tP 10; tP + 60 1];
    r = simuler_etude(sc, moteur);
    t = r.t;
    T = r.T_sec;
    apres = t >= tP;
    [p.T_min, k] = min(T + 1e3 * ~apres .* ones(size(T)));
    p.t_min = t(k);
    p.T_avant = interp1(t, T, tP);
    p.chute = p.T_avant - p.T_min;
    spark = r.fsm.Spark(:) ~= 0;
    fronts = t(find(diff([0; spark]) > 0));
    p.t_rallumage = min([fronts(fronts >= tP); NaN]);
    kSous = find(apres & T < 52.5, 1);
    if isempty(kSous)
        p.t_retour = NaN;
    else
        kRetour = find(t > t(kSous) & T >= 52.5, 1);
        p.t_retour = t(kRetour);
    end
    P = round(100 * r.P_gaz / 5000);
    paliers = unique(P(apres & t <= tP + 900))';
    journal_etude(fr, ['Porte ouverte de %d a %d s (UA x 10) : T_sec %.2f -> %.2f C (chute %.2f C, minimum a %.0f s) ; ' ...
                       'rallumage a %s s ; retour dans la bande a %s s (%s s apres l''ouverture) ; paliers utilises apres : %s %%\n'], ...
                  tP, tP + 60, p.T_avant, p.T_min, p.chute, p.t_min, nombre_fr(p.t_rallumage, 0), ...
                  nombre_fr(p.t_retour, 0), nombre_fr(p.t_retour - tP, 0), mat2str(paliers));

    fig = figure_etude(15, 11);
    a1 = subplot(2, 1, 1);
    hold on;
    fen = [(tP - 600) / 60, (tP + 1500) / 60];
    patch([tP tP + 60 tP + 60 tP] / 60, [40 40 64 64], [0.85 0.85 0.85], 'EdgeColor', 'none');
    patch([fen(1) fen(2) fen(2) fen(1)], [52.5 52.5 57.5 57.5], couleur_etude('bande'), 'EdgeColor', 'none');
    plot(t / 60, T, 'LineWidth', 1.4, 'Color', couleur_etude('bleu'));
    text((tP + 60) / 60, 62, ' porte ouverte 60 s', 'FontSize', 9);
    if ~isnan(p.t_retour)
        plot(p.t_retour / 60, 52.5, 'o', 'Color', couleur_etude('vert'), 'MarkerFaceColor', couleur_etude('vert'));
        text(p.t_retour / 60, 51, sprintf(' retour dans la bande (%s min)', nombre_fr(p.t_retour / 60, 1)), ...
             'FontSize', 9, 'VerticalAlignment', 'top');
    end
    ylabel('T_{sec} (°C)');
    ylim([40 64]);
    title('Ouverture de porte pendant la régulation (UA × 10 pendant 60 s)');
    grid on;
    box on;
    a2 = subplot(2, 1, 2);
    hold on;
    stairs(t / 60, P, 'LineWidth', 1.4, 'Color', couleur_etude('vert'));
    if ~isnan(p.t_rallumage)
        plot([p.t_rallumage p.t_rallumage] / 60, [-5 110], ':', 'Color', couleur_etude('gris'));
        text(p.t_rallumage / 60, 90, sprintf(' rallumage %s min', nombre_fr(p.t_rallumage / 60, 1)), 'FontSize', 9);
    end
    ylim([-5 110]);
    set(gca, 'YTick', [0 33 67 100]);
    ylabel('Palier (% P_{nom})');
    xlabel('Temps (min)');
    grid on;
    box on;
    for a = [a1 a2]
        set(a, 'XLim', fen);
    end
    enregistrer_etude(fig, fichier);
end
