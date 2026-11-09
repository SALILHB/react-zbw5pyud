%% etude_E2_modes.m  —  E2 : commande automatique, conduite manuelle, absence de régulation
%
%   etude_E2_modes
%   etude_E2_modes(moteur)            moteur du cas M1 : 'simulink' ou 'programme'
%   etude_E2_modes(moteur, racine)    racine = dossier memoire_V4_latex
%
% Conditions du scénario 8 (T_amb = 25 °C, T_sec initiale 25 °C, T_cible =
% 55 °C) sur 2 h. M1 = chart (commande automatique) ; M2a, M2b, M3, M4 =
% simuler_pilote (même modèle thermique). Produit :
%   Figures/simulation/modes_tsec.png, modes_energie.png
%   Figures/simulation/tables/tab_modes.tex        7 colonnes
%   captures/etudes/resultats_E2.txt, E2.mat

function res = etude_E2_modes(moteur, racine)

    if nargin < 1
        moteur = '';
    end
    if nargin < 2
        racine = '';
    end
    ici = fileparts(mfilename('fullpath'));
    addpath(ici);
    addpath(fileparts(ici));
    fr = chemin_sortie('', 'resultats_E2.txt');
    journal_etude(fr);
    journal_etude(fr, '=== E2 : modes de conduite (conditions du scenario 8, 2 h) ===\n');

    sc = cas_etude('M1');
    modes = {'M1', 'M2a', 'M2b', 'M3', 'M4'};
    R = cell(1, numel(modes));
    R{1} = simuler_etude(sc, moteur);
    for i = 2:numel(modes)
        R{i} = simuler_pilote(modes{i}, sc);
    end
    journal_etude(fr, 'Moteur de M1 : %s ; M2 a M4 : pilote (simuler_pilote)\n\n', R{1}.moteur);

    B = cell(1, numel(modes));
    for i = 1:numel(modes)
        B{i} = bilan_etude(R{i});
    end
    E1 = B{1}.E_gaz;
    journal_etude(fr, '%-4s %7s %8s %9s %8s %8s %6s %10s %10s\n', 'Mode', 'Tmax', 'bande%', 'hors(min)', ...
                  'E(kWh)', 'ecart%', 'allum.', 't>70 (min)', 't>90 (min)');
    lignes = cell(1, numel(modes));
    for i = 1:numel(modes)
        b = B{i};
        ecart = 100 * (b.E_gaz - E1) / E1;
        t70 = premier(R{i}.t, R{i}.T_sec > 70) / 60;
        t90 = premier(R{i}.t, R{i}.T_sec >= 90) / 60;
        journal_etude(fr, '%-4s %7.1f %8.1f %9.0f %8.2f %8.0f %6d %10.1f %10.1f\n', modes{i}, b.T_max, b.bande, ...
                      b.hors, b.E_gaz, ecart, b.rallumages, t70, t90);
        lignes{i} = {modes{i}, nombre_fr(b.T_max, 1), nombre_fr(b.bande, 0), nombre_fr(b.hors, 0), ...
                     nombre_fr(b.E_gaz, 2), ternaire(i == 1, '--', nombre_fr(ecart, 0)), ...
                     sprintf('%d', b.rallumages)};
    end
    journal_etude(fr, '(t>70, t>90 : premier instant, en min, ou NaN)\n');
    ecrire_table_tex(chemin_sortie(racine, 'Figures/simulation/tables/tab_modes.tex'), lignes, 7);

    noms = {'M1 automatique', 'M2a manuel 15 min', 'M2b manuel 30 min', 'M3 thermostat TOR', ...
            'M4 sans régulation'};
    figureTsec(R, noms, chemin_sortie(racine, 'Figures/simulation/modes_tsec.png'));
    figureEnergie(R, noms, chemin_sortie(racine, 'Figures/simulation/modes_energie.png'));

    res.modes = modes;
    res.bilans = B;
    save(chemin_sortie('', 'E2.mat'), 'res');
    journal_etude(fr, '=== E2 termine ===\n');
end

%% =====================================================================
function t0 = premier(t, masque)
    k = find(masque, 1);
    if isempty(k)
        t0 = NaN;
    else
        t0 = t(k);
    end
end

function s = ternaire(c, a, b)
    if c
        s = a;
    else
        s = b;
    end
end

function [styles, couleurs] = apparence()
    styles = {'-', '--', '-.', '-', ':'};
    couleurs = {[0.1 0.1 0.1], couleur_etude('bleu'), couleur_etude('violet'), couleur_etude('vert'), ...
                couleur_etude('rouge')};
end

function figureTsec(R, noms, fichier)
    [styles, couleurs] = apparence();
    fig = figure_etude(15, 12);
    hold on;
    hPlage = patch([0 120 120 0], [45 45 70 70], [0.94 0.94 0.94], 'EdgeColor', 'none');
    hBande = patch([0 120 120 0], [52.5 52.5 57.5 57.5], couleur_etude('bande'), 'EdgeColor', 'none');
    hs = zeros(1, numel(R));
    Tmax = 0;
    for i = 1:numel(R)
        hs(i) = plot(R{i}.t / 60, R{i}.T_sec, styles{i}, 'LineWidth', 1.3, 'Color', couleurs{i});
        Tmax = max(Tmax, max(R{i}.T_sec));
    end
    legend([hs hBande hPlage], [noms, {'bande 52,5-57,5 °C', 'plage 45-70 °C'}], ...
           'Location', 'southoutside', 'NumColumns', 3, 'FontSize', 8);
    xlim([0 120]);
    ylim([20 10 * ceil((Tmax + 5) / 10)]);
    set(gca, 'XTick', 0:15:120);
    xlabel('Temps (min)');
    ylabel('T_{sec} (°C)');
    title('T_{sec} selon le mode de conduite (conditions du scénario 8)');
    grid on;
    box on;
    enregistrer_etude(fig, fichier);
end

function figureEnergie(R, noms, fichier)
    [styles, couleurs] = apparence();
    fig = figure_etude(15, 8);
    hold on;
    hs = zeros(1, numel(R));
    for i = 1:numel(R)
        t = R{i}.t(:);
        E = [0; cumsum(R{i}.P_gaz(1:end - 1) .* diff(t))] / 3.6e6;
        hs(i) = plot(t / 60, E, styles{i}, 'LineWidth', 1.3, 'Color', couleurs{i});
    end
    legend(hs, noms, 'Location', 'eastoutside', 'FontSize', 8);
    xlim([0 120]);
    set(gca, 'XTick', 0:15:120);
    xlabel('Temps (min)');
    ylabel('Énergie de gaz cumulée (kWh)');
    title('Énergie consommée selon le mode de conduite');
    grid on;
    box on;
    enregistrer_etude(fig, fichier);
end
