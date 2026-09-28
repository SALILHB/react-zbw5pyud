%% preparer_figures_memoire.m  —  figures complémentaires du mémoire V3 (images A5 à A8)
%
%   preparer_figures_memoire
%
% Lit les résultats déjà simulés (captures/scenario_NN.mat), SANS relancer
% Simulink, et écrit dans captures/memoire/ (PNG 300 dpi, 15 cm de large,
% police lisible une fois la figure réduite à la largeur du texte) :
%
%   zoom_regulation.png      A5  scénario 1, cycle de veille et de rallumage à 33 %
%   humidite_fin_cycle.png   A6  scénario 8, H_sec et T_sec, critère de fin de cycle
%   energie_scenarios.png    A7  puissance moyenne et énergie de gaz par scénario
%   energie_scenarios.csv        (mêmes valeurs, en tableau)
%   ecarts_validation.png    A8  écart maximal Simulink / programme Arduino par scénario
%   ecarts_validation.csv
%
% Si un fichier scenario_NN.mat manque, la figure utilise la référence du
% programme Arduino (reference/scenario_NN.csv) et le signale : [firmware].
% Fonctionne sous MATLAB R2025b et GNU Octave.

function preparer_figures_memoire()

    dossier = fileparts(mfilename('fullpath'));
    sortie = fullfile(dossier, 'captures', 'memoire');
    if ~exist(sortie, 'dir')
        mkdir(sortie);
    end
    fprintf('=== Figures du memoire -> %s ===\n', sortie);

    figureZoomRegulation(dossier, sortie);
    figureHumidite(dossier, sortie);
    figureEnergie(dossier, sortie);
    figureEcarts(dossier, sortie);
end

%% =====================================================================
%  A5 — Zoom sur un cycle de régulation (scénario 1)
%  =====================================================================
function figureZoomRegulation(dossier, sortie)
    [r, source] = charger(dossier, 1);
    fenetre = [580 1400];
    k = r.t >= fenetre(1) & r.t <= fenetre(2);
    t = r.t(k);
    T = r.T_sec(k);
    pct = round(100 * r.P_gaz(k) / 5000);
    T3 = valeurA(r.sc.signaux.T_cible, fenetre(1));
    dh = r.sc.param.Hhyst / 2;

    fig = nouvelleFigure(15, 11);
    a1 = subplot(2, 1, 1);
    hold on;
    ligneH(a1, T3 + dh, ':', GRIS());
    ligneH(a1, T3, '--', ORANGE());
    ligneH(a1, T3 - dh, ':', GRIS());
    plot(t, T, 'LineWidth', 1.6, 'Color', BLEU());
    text(fenetre(2), T3 + dh, sprintf(' %.1f', T3 + dh), 'FontSize', 8, 'Color', GRIS(), 'VerticalAlignment', 'middle');
    text(fenetre(2), T3, lat(sprintf(' T3 = %g %cC', T3, char(176))), 'FontSize', 8, 'Color', ORANGE(), 'VerticalAlignment', 'middle');
    text(fenetre(2), T3 - dh, sprintf(' %.1f', T3 - dh), 'FontSize', 8, 'Color', GRIS(), 'VerticalAlignment', 'middle');
    ylabel(lat(['T_{sec} (' char(176) 'C)']));
    ylim([T3 - 4, T3 + 5]);
    grid on;

    a2 = subplot(2, 1, 2);
    hold on;
    stairs(t, pct, 'LineWidth', 1.6, 'Color', VERT());
    ylim([-5 45]);
    set(a2, 'YTick', [0 33]);
    ylabel('Gaz (% Pnom)');
    xlabel('Temps (s)');
    grid on;

    % Coupures (passage à 0 %) et rallumages (0 % -> palier) dans la fenêtre
    d = diff(pct);
    coupures = t(find(d < 0 & pct(2:end) == 0) + 1);
    rallumages = t(find(d > 0 & pct(1:end-1) == 0) + 1);
    evts = sortrows([coupures(:), zeros(numel(coupures), 1); rallumages(:), ones(numel(rallumages), 1)]);
    for i = 1:size(evts, 1)
        for a = [a1 a2]
            axes(a); %#ok<LAXES>
            yl = ylim;
            plot([evts(i, 1) evts(i, 1)], yl, ':', 'Color', [0.3 0.3 0.3]);
        end
        axes(a2); %#ok<LAXES>
        if evts(i, 2) == 0
            etiquette = sprintf('coupure %.0f s', evts(i, 1));
        else
            etiquette = sprintf('rallumage %.0f s', evts(i, 1));
        end
        text(evts(i, 1), 42 - 12 * mod(i + 1, 2), [' ' etiquette], 'FontSize', 8, 'VerticalAlignment', 'top');
    end
    % Durées de veille (coupure -> rallumage) et de chauffe (rallumage -> coupure)
    axes(a1); %#ok<LAXES>
    yDuree = T3 + 4;
    for i = 1:size(evts, 1) - 1
        t0 = evts(i, 1);
        t1 = evts(i + 1, 1);
        if evts(i, 2) == 0
            libelle = sprintf('veille %.0f s', t1 - t0);
        else
            libelle = sprintf('chauffe %.0f s', t1 - t0);
        end
        plot([t0 t1], [yDuree yDuree], '-', 'Color', [0.3 0.3 0.3], 'LineWidth', 0.8);
        plot([t0 t0 NaN t1 t1], [yDuree - 0.3, yDuree + 0.3, NaN, yDuree - 0.3, yDuree + 0.3], '-', 'Color', [0.3 0.3 0.3]);
        text((t0 + t1) / 2, yDuree + 0.15, libelle, 'FontSize', 8, 'HorizontalAlignment', 'center', ...
             'VerticalAlignment', 'bottom');
    end
    title(lat(sprintf('Sc%snario 1 : cycle de veille et de rallumage%s', char(233), marque(source))));
    linkaxes([a1 a2], 'x');
    xlim(a1, fenetre);
    enregistrer(fig, fullfile(sortie, 'zoom_regulation.png'));
    for i = 1:size(evts, 1)
        fprintf('      %s a %.1f s\n', ternaire(evts(i, 2) == 0, 'coupure  ', 'rallumage'), evts(i, 1));
    end
end

%% =====================================================================
%  A6 — Humidité et critère de fin de cycle (scénario 8)
%  =====================================================================
function figureHumidite(dossier, sortie)
    [r, source] = charger(dossier, 8);
    sc = r.sc;
    tm = r.t / 60;
    Hfin = min(valeurA(sc.signaux.H_produit, 0) + valeurA(sc.signaux.H_amb, 0), 95);
    tStart = premierAppui(sc.signaux.Btn_Start);
    tMin = (tStart + sc.param.Temps_Min_Fin) / 60;
    etat = round(r.fsm.Etat_LCD(:));
    tDem = premierInstant(r.t, etat == 5);
    tTer = premierInstant(r.t, etat == 7);
    tAtt = premierInstant(r.t(r.t > tTer), etat(r.t > tTer) == 0);
    if isnan(tAtt)
        tAtt = r.t(end);
    end
    kPassage = find(r.H_sec <= Hfin, 1);

    fig = nouvelleFigure(15, 9.5);
    a1 = axes('Position', [0.10 0.14 0.74 0.74]);
    hold on;
    yl = [20 110];   % marge en haut pour la légende
    hDem = bande(tDem / 60, tTer / 60, yl, ORANGE());
    hTer = bande(tTer / 60, tAtt / 60, yl, VIOLET());
    hH = plot(tm, r.H_sec, 'LineWidth', 1.8, 'Color', BLEU());
    hHfin = plot([tm(1) tm(end)], [Hfin Hfin], '--', 'LineWidth', 1.2, 'Color', ORANGE());
    plot([tMin tMin], yl, '-.', 'Color', [0.3 0.3 0.3]);
    text(tMin, yl(1) + 2, 'Temps\_Min\_Fin ', 'FontSize', 8, 'VerticalAlignment', 'bottom', ...
         'HorizontalAlignment', 'right');
    if ~isempty(kPassage)
        plot(tm(kPassage), r.H_sec(kPassage), 'o', 'Color', BLEU(), 'MarkerFaceColor', BLEU());
        text(tm(kPassage), r.H_sec(kPassage), ...
             sprintf('  H_{sec} = H_{fin} (%.0f min)', tm(kPassage)), 'FontSize', 8, 'VerticalAlignment', 'bottom');
    end
    hT = plot(NaN, NaN, '-', 'LineWidth', 1.0, 'Color', ROUGE());   % entrée de légende
    ylim(yl);
    set(a1, 'YTick', 20:20:100);
    xlim([tm(1) tm(end)]);
    ylabel(lat(['Humidit' char(233) ' de l''air extrait (% HR)']));
    xlabel('Temps (min)');
    grid on;
    legend([hH hHfin hT hDem hTer], {'H_{sec}', lat(sprintf('H_{fin} = %g %%', Hfin)), ...
        lat('T_{sec} (axe de droite)'), 'DEMANDE', lat(['TERMIN' char(201)])}, ...
        'Location', 'north', 'FontSize', 8, 'NumColumns', 3);
    title(lat(sprintf('Sc%snario 8 : humidit%s et fin de cycle%s', char(233), char(233), marque(source))));

    a2 = axes('Position', get(a1, 'Position'), 'Color', 'none', 'YAxisLocation', 'right', ...
              'XTick', [], 'Box', 'off');
    hold(a2, 'on');
    plot(a2, tm, r.T_sec, 'LineWidth', 1.0, 'Color', ROUGE());
    ylim(a2, [20 90]);
    set(a2, 'YTick', 20:10:70);
    xlim(a2, [tm(1) tm(end)]);
    ylabel(a2, lat(['T_{sec} (' char(176) 'C)']));
    enregistrer(fig, fullfile(sortie, 'humidite_fin_cycle.png'));
    if ~isempty(kPassage)
        fprintf('      H_sec <= H_fin (%g %%) a %.1f min ; Temps_Min_Fin a %.1f min\n', Hfin, tm(kPassage), tMin);
    end
    fprintf('      DEMANDE a %.1f s, TERMINE a %.1f s\n', tDem, tTer);
end

%% =====================================================================
%  A7 — Puissance moyenne et énergie de gaz par scénario
%  =====================================================================
function figureEnergie(dossier, sortie)
    numeros = scenariosDisponibles(dossier);
    n = numel(numeros);
    Pmoy = zeros(1, n);
    E = zeros(1, n);
    duree = zeros(1, n);
    sources = cell(1, n);
    fid = fopen(fullfile(sortie, 'energie_scenarios.csv'), 'w');
    fprintf(fid, 'scenario,source,duree_s,puissance_moyenne_W,energie_gaz_kWh\n');
    for i = 1:n
        [r, sources{i}] = charger(dossier, numeros(i));
        J = energie(r.t, r.P_gaz, r.t(1), r.t(end));
        duree(i) = r.t(end) - r.t(1);
        Pmoy(i) = J / duree(i);
        E(i) = J / 3.6e6;
        fprintf(fid, '%d,%s,%.0f,%.1f,%.3f\n', numeros(i), sources{i}, duree(i), Pmoy(i), E(i));
    end
    fclose(fid);

    fig = nouvelleFigure(15, 11);
    subplot(2, 1, 1);
    barresParSource(numeros, Pmoy, sources);
    ylabel('Puissance moyenne (W)');
    etiquettesBarres(numeros, Pmoy, '%.0f');
    title(lat(['Puissance de gaz moyenne et ' char(233) 'nergie par sc' char(233) 'nario']));
    if any(strcmp(sources, 'firmware'))
        yl = ylim;
        text(max(numeros) + 0.5, yl(2), lat([sprintf('fonc%s : Simulink ; clair : r%sf%srence Arduino', ...
             char(233), char(233), char(233)) ' ']), 'FontSize', 7, 'HorizontalAlignment', 'right', ...
             'VerticalAlignment', 'top', 'Color', GRIS());
    end
    grid on;
    subplot(2, 1, 2);
    barresParSource(numeros, E, sources);
    ylabel(lat([char(201) 'nergie de gaz (kWh)']));
    xlabel(lat(['Sc' char(233) 'nario']));
    etiquettesBarres(numeros, E, '%.2f');
    grid on;
    enregistrer(fig, fullfile(sortie, 'energie_scenarios.png'));
    fprintf('  [ok] %s\n', fullfile(sortie, 'energie_scenarios.csv'));

    % Valeurs de contrôle citées dans le mémoire
    if any(numeros == 1)
        r = charger(dossier, 1);
        fprintf('      controle scenario 1, 1re heure : %.0f W (attendu ~600 W)\n', ...
            energie(r.t, r.P_gaz, 0, 3600) / 3600);
    end
    i8 = find(numeros == 8);
    if ~isempty(i8)
        r = charger(dossier, 8);
        tDem = premierInstant(r.t, round(r.fsm.Etat_LCD(:)) == 5);
        J = energie(r.t, r.P_gaz, 0, tDem);
        fprintf(['      controle scenario 8, jusqu''a la demande (%.0f s) : %.0f W, %.2f kWh ' ...
                 '(attendu ~455 W, ~0,91 kWh)\n'], tDem, J / tDem, J / 3.6e6);
        fprintf('      scenario 8 complet (%.0f s) : %.0f W, %.2f kWh\n', duree(i8), Pmoy(i8), E(i8));
    end
end

%% =====================================================================
%  A8 — Écart maximal Simulink / programme Arduino par scénario
%  =====================================================================
function figureEcarts(dossier, sortie)
    numeros = 1:16;
    ecart = nan(1, numel(numeros));
    identique = true(1, numel(numeros));
    fid = fopen(fullfile(sortie, 'ecarts_validation.csv'), 'w');
    fprintf(fid, 'scenario,changements_etat,ecart_max_s,sequence_identique\n');
    for i = 1:numel(numeros)
        n = numeros(i);
        fMat = fullfile(dossier, 'captures', sprintf('scenario_%02d.mat', n));
        fRef = fullfile(dossier, 'reference', sprintf('scenario_%02d.csv', n));
        if ~exist(fMat, 'file') || ~exist(fRef, 'file')
            continue;
        end
        d = load(fMat);
        [ts, es] = changements(d.r);
        [tf, ef] = changements(charger_reference(n));
        m = min(numel(ts), numel(tf));
        identique(i) = numel(ts) == numel(tf) && all(es(1:m) == ef(1:m));
        ecart(i) = max(abs(ts(1:m) - tf(1:m)));
        fprintf(fid, '%d,%d,%.2f,%d\n', n, numel(ts), ecart(i), identique(i));
    end
    fclose(fid);

    fig = nouvelleFigure(15, 8);
    hold on;
    for i = 1:numel(numeros)
        if isnan(ecart(i))
            text(numeros(i), 0.2, lat(['non simul' char(233)]), 'Rotation', 90, 'FontSize', 7, 'Color', GRIS());
            continue;
        end
        couleur = BLEU();
        if ~identique(i)
            couleur = ROUGE();
        elseif ecart(i) > 0.15
            couleur = ORANGE();
        end
        barre(numeros(i), ecart(i), couleur);
        text(numeros(i), ecart(i), sprintf('%.1f', ecart(i)), 'FontSize', 8, ...
             'HorizontalAlignment', 'center', 'VerticalAlignment', 'bottom');
    end
    plot([0.4 numel(numeros) + 0.6], [0.1 0.1], '--', 'Color', [0.3 0.3 0.3]);
    text(numel(numeros) + 0.6, 0.1, ' 0,1 s', 'FontSize', 8, 'VerticalAlignment', 'bottom', ...
         'HorizontalAlignment', 'right');
    xlim([0.4 numel(numeros) + 0.6]);
    ylim([0 max(0.8, max([ecart 0]) * 1.25)]);
    set(gca, 'XTick', numeros);
    xlabel(lat(['Sc' char(233) 'nario']));
    ylabel(lat([char(201) 'cart maximal (s)']));
    title(lat([char(201) 'cart entre le mod' char(232) 'le Stateflow et le programme Arduino']));
    grid on;
    box on;
    enregistrer(fig, fullfile(sortie, 'ecarts_validation.png'));
    fprintf('  [ok] %s\n', fullfile(sortie, 'ecarts_validation.csv'));
    for i = find(~isnan(ecart))
        fprintf('      scenario %2d : ecart max %.1f s%s\n', numeros(i), ecart(i), ...
            ternaire(identique(i), '', '  (SEQUENCE DIFFERENTE)'));
    end
end

%% =====================================================================
%  Outils
%  =====================================================================
function [r, source] = charger(dossier, n)
    f = fullfile(dossier, 'captures', sprintf('scenario_%02d.mat', n));
    if exist(f, 'file')
        d = load(f);
        r = d.r;
        source = 'Simulink';
    else
        r = charger_reference(n);
        source = 'firmware';
        fprintf('  [info] captures/scenario_%02d.mat absent : reference du programme Arduino utilisee\n', n);
    end
end

function numeros = scenariosDisponibles(dossier)
    numeros = [];
    for n = 1:16
        if exist(fullfile(dossier, 'captures', sprintf('scenario_%02d.mat', n)), 'file') || ...
           exist(fullfile(dossier, 'reference', sprintf('scenario_%02d.csv', n)), 'file')
            numeros(end + 1) = n; %#ok<AGROW>
        end
    end
end

function J = energie(t, P, t0, t1)
    % Intégrale de P (maintenue entre deux échantillons) de t0 à t1, en joules.
    t = t(:);
    P = P(:);
    dt = diff(t);
    debut = t(1:end-1);
    fin = t(2:end);
    recouvrement = max(0, min(fin, t1) - max(debut, t0));
    J = sum(P(1:end-1) .* recouvrement .* (dt > 0));
end

function [t, e] = changements(r)
    etat = round(r.fsm.Etat_LCD(:));
    k = [1; find(diff(etat) ~= 0) + 1];
    t = r.t(k);
    e = etat(k);
end

function v = valeurA(m, t)
    v = m(1, 2);
    for k = 1:size(m, 1)
        if m(k, 1) <= t
            v = m(k, 2);
        end
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

function t0 = premierInstant(t, masque)
    k = find(masque, 1);
    if isempty(k)
        t0 = NaN;
    else
        t0 = t(k);
    end
end

function fig = nouvelleFigure(largeurCm, hauteurCm)
    % Taille réelle de la figure dans le mémoire : police lisible après
    % réduction à la largeur du texte.
    fig = figure('Color', 'w', 'WindowStyle', 'normal', 'Units', 'centimeters', ...
                 'Position', [2 2 largeurCm hauteurCm]);
    set(fig, 'DefaultAxesFontSize', 9, 'DefaultTextFontSize', 9);
end

function ligneH(a, y, style, couleur)
    axes(a); %#ok<LAXES>
    plot([0 1e6], [y y], style, 'Color', couleur, 'LineWidth', 1.0);
end

function h = bande(t0, t1, yl, couleur)
    if isnan(t0) || isnan(t1)
        h = plot(NaN, NaN, 's', 'MarkerFaceColor', couleur, 'MarkerEdgeColor', 'none');
        return;
    end
    h = patch([t0 t1 t1 t0], [yl(1) yl(1) yl(2) yl(2)], couleur, ...
              'FaceAlpha', 0.18, 'EdgeColor', 'none');
end

function barresParSource(numeros, valeurs, sources)
    hold on;
    for i = 1:numel(numeros)
        couleur = BLEU();
        if strcmp(sources{i}, 'firmware')
            couleur = [0.62 0.74 0.87];   % clair : valeur de la référence Arduino
        end
        barre(numeros(i), valeurs(i), couleur);
    end
    xlim([0.4 max(numeros) + 0.6]);
    set(gca, 'XTick', numeros);
    box on;
end

function barre(x, y, couleur)
    % Barre dessinée comme un rectangle : rendu identique sous MATLAB et Octave.
    patch([x - 0.35, x + 0.35, x + 0.35, x - 0.35], [0 0 y y], couleur, 'EdgeColor', 'none');
end

function etiquettesBarres(numeros, valeurs, format)
    for i = 1:numel(numeros)
        text(numeros(i), valeurs(i), sprintf(format, valeurs(i)), 'FontSize', 7, ...
             'HorizontalAlignment', 'center', 'VerticalAlignment', 'bottom');
    end
    yl = ylim;
    ylim([0 yl(2) * 1.12]);
end

function enregistrer(fig, fichier)
    exporter_figure(fig, fichier);
    fprintf('  [ok] %s\n', fichier);
end

function s = marque(source)
    if strcmp(source, 'Simulink')
        s = '';
    else
        s = ' [firmware]';
    end
end

function s = ternaire(c, a, b)
    if c
        s = a;
    else
        s = b;
    end
end

function c = BLEU()
    c = [0.17 0.42 0.69];
end

function c = ORANGE()
    c = [0.75 0.34 0.13];
end

function c = VERT()
    c = [0.18 0.52 0.35];
end

function c = ROUGE()
    c = [0.77 0.19 0.19];
end

function c = VIOLET()
    c = [0.42 0.27 0.76];
end

function c = GRIS()
    c = [0.45 0.45 0.45];
end

function s = lat(s)
    % Accents écrits par leur code (char(233) = é) : corrects dans MATLAB ;
    % convertis en UTF-8 pour Octave.
    if exist('OCTAVE_VERSION', 'builtin')
        s = native2unicode(uint8(s), 'latin1');
    end
end
