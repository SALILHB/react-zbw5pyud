%% etude_E1_sources.m  —  E1 : journée type, énergie par source, absence d'une source
%
%   etude_E1_sources
%   etude_E1_sources(moteur)               'simulink' (défaut MATLAB) ou 'programme'
%   etude_E1_sources(moteur, racine)       racine = dossier memoire_V4_latex :
%                                          figures et tableaux écrits à leur place
%   etude_E1_sources(moteur, racine, m0)   stock d'hydrogène des cas C4 et C5 (g)
%
% Cas C1 à C8 (cas_etude), cycle de 10 h de 8 h à 18 h. Produit :
%   Figures/simulation/profil_journee.png        cas C1 : T_amb, T_cap estimée,
%                                                seuils, P_sol, T_sec et source
%   Figures/simulation/energie_sources.png       énergies par source, f_sol
%   Figures/simulation/tsec_sources.png          T_sec de C1, C3, C4, C6, C8
%   Figures/simulation/tables/tab_sources.tex    7 colonnes
%   Figures/simulation/tables/tab_consommation.tex  5 colonnes
%   captures/etudes/resultats_E1.txt             chiffres clés (RESULTATS_SIMULATION.md)
%   captures/etudes/E1.mat                       résultats complets
%
% m0 par défaut : 28 g. Avec Press_H2 = 8 bar x (1 - m/m0), la pression passe
% sous Press_H2_Min (2 bar, 25 % du stock) quand 21 g ont été consommés : dans
% le cas C1, c'est au dernier rallumage de la matinée (vers 11 h), juste avant
% le passage au solaire. Le script affiche la consommation de C1 pour contrôle.

function res = etude_E1_sources(moteur, racine, m0)

    if nargin < 1
        moteur = '';
    end
    if nargin < 2
        racine = '';
    end
    if nargin < 3 || isempty(m0)
        m0 = 28;
    end
    ajouterChemins();
    fr = chemin_sortie('', 'resultats_E1.txt');
    journal_etude(fr);
    journal_etude(fr, '=== E1 : journee type et disponibilite des sources ===\n');

    cas = {'C1', 'C2', 'C3', 'C4', 'C5', 'C6', 'C7', 'C8'};
    R = cell(1, numel(cas));
    B = cell(1, numel(cas));
    for i = 1:numel(cas)
        tic;
        R{i} = simuler_etude(cas_etude(cas{i}, m0), moteur);
        B{i} = bilan_etude(R{i});
        fprintf('  %s simule (%s) en %.1f s\n', cas{i}, R{i}.moteur, toc);
    end
    moteurUtilise = R{1}.moteur;

    % --- Contrôle du choix de m0 sur la consommation de C1
    r1 = R{1};
    journal_etude(fr, 'Moteur : %s\n', moteurUtilise);
    journal_etude(fr, 'Hypotheses : humidite fixee a 90 %% (cycle de 10 h complet), eta_b = %.2f, P_sol,max = %.0f W\n', ...
                  r1.sc.etude.eta, 20 / 0.098);
    journal_etude(fr, 'Consommation d''hydrogene de C1 : %.1f g au total ; %.1f g a 10 h ; %.1f g a 11 h\n', ...
                  r1.m_H2(end), valeurA(r1, 2 * 3600, r1.m_H2), valeurA(r1, 3 * 3600, r1.m_H2));
    journal_etude(fr, 'm0 = %g g : bascule a 2 bar apres %.1f g consommes\n\n', m0, 0.75 * m0);

    % --- Tableau de synthèse
    journal_etude(fr, '%-4s %7s %7s %7s %7s %7s %6s %6s %6s %7s %7s %6s %6s\n', 'Cas', 'E_sol', 'E_soltot', ...
                  'E_H2', 'E_GPL', 'f_sol%', 'bande%', 'rall.', 'Tmax', 'H2 g', 'GPL g', 'CO2kg', 'hors');
    for i = 1:numel(cas)
        b = B{i};
        journal_etude(fr, '%-4s %7.2f %7.2f %7.2f %7.2f %7.1f %6.1f %6d %6.1f %7.1f %7.1f %6.3f %6.0f\n', cas{i}, ...
                      b.E_sol, b.E_sol_total, b.E_H2, b.E_GPL, 100 * b.f_sol, b.bande, b.rallumages, b.T_max, ...
                      b.m_H2, b.m_GPL, b.CO2, b.hors);
    end
    journal_etude(fr, '\nEvenements :\n');
    for i = 1:numel(cas)
        journal_etude(fr, '  %s (%s) : %s\n', cas{i}, R{i}.sc.nom, evenement(R{i}, B{i}, false));
    end
    eC7 = B{7}.E_gaz;
    journal_etude(fr, '\nEconomie due au solaire : E_gaz(C7) - E_gaz(C1) = %.2f - %.2f = %.2f kWh (%.0f %%)\n', ...
                  eC7, B{1}.E_gaz, eC7 - B{1}.E_gaz, 100 * (eC7 - B{1}.E_gaz) / max(eC7, eps));

    % --- Tableaux du mémoire
    lignes = cell(1, numel(cas));
    for i = 1:numel(cas)
        b = B{i};
        lignes{i} = {cas{i}, nombre_fr(b.E_sol, 2), nombre_fr(b.E_H2, 2), nombre_fr(b.E_GPL, 2), ...
                     nombre_fr(100 * b.f_sol, 0), nombre_fr(b.bande, 0), sprintf('%d', b.rallumages)};
    end
    ecrire_table_tex(chemin_sortie(racine, 'Figures/simulation/tables/tab_sources.tex'), lignes, 7);
    for i = 1:numel(cas)
        b = B{i};
        lignes{i} = {cas{i}, nombre_fr(b.m_H2, 1), nombre_fr(b.m_GPL, 1), nombre_fr(b.CO2, 3), ...
                     evenement(R{i}, B{i}, true)};
    end
    ecrire_table_tex(chemin_sortie(racine, 'Figures/simulation/tables/tab_consommation.tex'), lignes, 5);

    % --- Figures
    figureProfil(R{1}, chemin_sortie(racine, 'Figures/simulation/profil_journee.png'));
    figureEnergies(cas, B, chemin_sortie(racine, 'Figures/simulation/energie_sources.png'));
    choix = [1 3 4 6 8];
    figureTsec(cas(choix), R(choix), chemin_sortie(racine, 'Figures/simulation/tsec_sources.png'));

    res.cas = cas;
    res.bilans = B;
    res.m0 = m0;
    res.moteur = moteurUtilise;
    resultats = alleger(R); %#ok<NASGU>
    save(chemin_sortie('', 'E1.mat'), 'res', 'resultats');
    journal_etude(fr, '=== E1 termine ===\n');
end

%% =====================================================================
function ajouterChemins()
    ici = fileparts(mfilename('fullpath'));
    addpath(ici);
    addpath(fileparts(ici));
end

function v = valeurA(r, t, x)
    k = find(r.t <= t, 1, 'last');
    v = x(k);
end

function s = evenement(r, b, latex)
    % Événement notable du cas, en quelques mots.
    e = {};
    manuel = r.sc.signaux.Mode_Auto(1, 2) == 0;
    if manuel && ~isnan(b.t_solaire) && isnan(b.t_H2)
        e{end + 1} = 'solaire seul (manuel)';
    elseif manuel
        e{end + 1} = 'hydrogène seul (manuel)';
    else
        if ~isnan(b.t_GPL) && isnan(b.t_H2)
            e{end + 1} = 'GPL dès le départ';
        elseif ~isnan(b.t_GPL)
            e{end + 1} = ['bascule GPL à ' heure_jour(b.t_GPL)];
        end
        if ~isnan(b.t_erreur)
            e{end + 1} = ['erreur de combustion à ' heure_jour(b.t_erreur)];
        end
        if ~isnan(b.t_solaire)
            e{end + 1} = ['solaire à ' heure_jour(b.t_solaire)];
        elseif isnan(b.t_erreur)
            e{end + 1} = 'pas de solaire';
        end
    end
    if b.hors >= 10
        e{end + 1} = sprintf('%.0f min sous 45 °C', b.hors);
    end
    s = strjoin(e, ' ; ');
    if latex
        s = strrep(s, '_', '\_');
        s = strrep(s, '%', '\%');
    end
end

function R = alleger(R)
    for i = 1:numel(R)
        R{i}.sc = rmfield(R{i}.sc, 'signaux');
    end
end

function h = heures(t)
    h = 8 + t / 3600;
end

function bandesSources(r, yl)
    S = segments_source(r);
    for k = 1:size(S, 1)
        c = couleurCode(S(k, 3));
        patch(heures([S(k, 1) S(k, 2) S(k, 2) S(k, 1)]), [yl(1) yl(1) yl(2) yl(2)], c, ...
              'FaceAlpha', 0.18, 'EdgeColor', 'none');
    end
end

function c = couleurCode(code)
    switch code
        case 2
            c = couleur_etude('solaire');
        case 3
            c = couleur_etude('h2');
        case 4
            c = couleur_etude('gpl');
        otherwise
            c = couleur_etude('gris');
    end
end

function h = carre(c)
    h = patch(NaN(1, 4), NaN(1, 4), c, 'FaceAlpha', 0.35, 'EdgeColor', 'none');
end

%% ---------------------------------------------------------------------
function figureProfil(r, fichier)
    h = heures(r.t);
    Tcap = r.T_amb + r.sc.param.DeltaT_Sol;
    Ton = 55 + r.sc.param.Marge_Sol;
    Toff = Ton - r.sc.param.Hyst_Sol;
    fig = figure_etude(15, 16);

    a1 = subplot(3, 1, 1);
    hold on;
    p1 = plot(h, r.T_amb, 'LineWidth', 1.4, 'Color', couleur_etude('gris'));
    p2 = plot(h, Tcap, 'LineWidth', 1.4, 'Color', couleur_etude('solaire'));
    p3 = plot([8 18], [Ton Ton], '--', 'LineWidth', 1.0, 'Color', couleur_etude('rouge'));
    p4 = plot([8 18], [Toff Toff], ':', 'LineWidth', 1.2, 'Color', couleur_etude('rouge'));
    ylabel('Température (°C)');
    legend([p1 p2 p3 p4], {'T_{amb}', 'T_{cap} estimée = T_{amb} + 20', 'seuil ON (55 °C)', ...
            'seuil OFF (50 °C)'}, 'Location', 'southeast', 'FontSize', 8);
    ylim([20 62]);
    grid on;
    title('Journée claire (cas C1)');

    a2 = subplot(3, 1, 2);
    stairs(h, r.P_sol, 'LineWidth', 1.4, 'Color', couleur_etude('solaire'));
    ylabel('P_{sol} (W)');
    ylim([0 230]);
    grid on;

    a3 = subplot(3, 1, 3);
    hold on;
    yl = [20 65];
    patch([8 18 18 8], [52.5 52.5 57.5 57.5], couleur_etude('bande'), 'EdgeColor', 'none', 'FaceAlpha', 0.6);
    bandesSources(r, yl);
    p = plot(h, r.T_sec, 'LineWidth', 1.4, 'Color', [0.1 0.1 0.1]);
    hs = [carre(couleur_etude('solaire')), carre(couleur_etude('h2')), carre(couleur_etude('gpl'))];
    legend([p hs], {'T_{sec}', 'solaire', 'hydrogène', 'GPL'}, 'Location', 'southwest', ...
           'Orientation', 'horizontal', 'FontSize', 8);
    ylim(yl);
    ylabel('T_{sec} (°C)');
    xlabel('Heure de la journée (h)');
    grid on;
    box on;
    for a = [a1 a2 a3]
        set(a, 'XLim', [8 18], 'XTick', 8:1:18);
    end
    enregistrer_etude(fig, fichier);
end

function figureEnergies(cas, B, fichier)
    n = numel(cas);
    E = zeros(n, 3);
    for i = 1:n
        E(i, :) = [B{i}.E_sol, B{i}.E_H2, B{i}.E_GPL];
    end
    couleurs = {couleur_etude('solaire'), couleur_etude('h2'), couleur_etude('gpl')};
    fig = figure_etude(15, 9);
    hold on;
    for i = 1:n
        bas = 0;
        for j = 1:3
            if E(i, j) > 0
                patch([i - 0.35, i + 0.35, i + 0.35, i - 0.35], [bas bas bas + E(i, j) bas + E(i, j)], ...
                      couleurs{j}, 'EdgeColor', 'none');
            end
            bas = bas + E(i, j);
        end
        text(i, bas, sprintf('%.0f %%', 100 * B{i}.f_sol), 'HorizontalAlignment', 'center', ...
             'VerticalAlignment', 'bottom', 'FontSize', 9);
    end
    hs = zeros(1, 3);
    for j = 1:3
        hs(j) = patch(NaN(1, 4), NaN(1, 4), couleurs{j}, 'EdgeColor', 'none');
    end
    legend(hs, {'solaire (mode solaire)', 'hydrogène', 'GPL'}, 'Location', 'northeast');
    set(gca, 'XTick', 1:n, 'XTickLabel', cas, 'XLim', [0.4 n + 0.6]);
    yl = ylim;
    ylim([0 1.18 * yl(2)]);
    ylabel('Énergie utile (kWh)');
    title('Énergie par source sur 10 h (f_{sol} au-dessus des barres)');
    grid on;
    box on;
    enregistrer_etude(fig, fichier);
end

function figureTsec(cas, R, fichier)
    styles = {'-', '--', '-.', '-', ':'};
    couleurs = {[0.1 0.1 0.1], couleur_etude('gpl'), couleur_etude('violet'), couleur_etude('solaire'), ...
                couleur_etude('rouge')};
    fig = figure_etude(15, 12);
    a1 = axes('Position', [0.10 0.36 0.86 0.56]);
    hold on;
    patch([8 18 18 8], [45 45 70 70], [0.94 0.94 0.94], 'EdgeColor', 'none');
    patch([8 18 18 8], [52.5 52.5 57.5 57.5], couleur_etude('bande'), 'EdgeColor', 'none');
    hs = zeros(1, numel(cas));
    for i = 1:numel(cas)
        hs(i) = plot(heures(R{i}.t), R{i}.T_sec, styles{i}, 'LineWidth', 1.3, 'Color', couleurs{i});
    end
    legend(hs, cas, 'Location', 'south', 'Orientation', 'horizontal');
    ylabel('T_{sec} (°C)');
    ylim([20 75]);
    set(a1, 'XLim', [8 18], 'XTick', 8:18, 'XTickLabel', {});
    text(17.95, 55, 'bande 52,5-57,5 °C', 'HorizontalAlignment', 'right', 'FontSize', 8, ...
         'Color', couleur_etude('vert'));
    text(17.95, 68.5, 'plage admissible 45-70 °C', 'HorizontalAlignment', 'right', 'FontSize', 8, ...
         'Color', couleur_etude('gris'));
    title('T_{sec} selon les sources disponibles');
    grid on;
    box on;

    a2 = axes('Position', [0.10 0.10 0.86 0.22]);
    hold on;
    for i = 1:numel(cas)
        S = segments_source(R{i});
        y = numel(cas) - i + 1;
        for k = 1:size(S, 1)
            patch(heures([S(k, 1) S(k, 2) S(k, 2) S(k, 1)]), [y - 0.35 y - 0.35 y + 0.35 y + 0.35], ...
                  couleurCode(S(k, 3)), 'EdgeColor', 'none');
        end
    end
    set(a2, 'YTick', 1:numel(cas), 'YTickLabel', fliplr(cas), 'YLim', [0.4 numel(cas) + 0.6], ...
        'XLim', [8 18], 'XTick', 8:18);
    xlabel('Heure de la journée (h) — source active : jaune solaire, bleu H_2, orange GPL, gris erreur');
    box on;
    enregistrer_etude(fig, fichier);
end
