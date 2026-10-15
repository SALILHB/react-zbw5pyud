%% etude_E3_pertes.m  —  E3 : bilan des pertes sur la journée type (cas C1, 10 h)
%
%   etude_E3_pertes
%   etude_E3_pertes(moteur)            'simulink' (défaut MATLAB) ou 'programme'
%   etude_E3_pertes(moteur, racine)    racine = dossier memoire_V4_latex
%
% Postes (kWh). Le modèle ne représente que les deux premiers ; les trois
% autres sont des ESTIMATIONS calculées sur les signaux simulés :
%   1. échauffement de la chambre      C_eq (T_sec,fin - T_sec,0)
%   2. pertes par les parois           intégrale de UA (T_sec - T_amb)
%   3. renouvellement d'air            intégrale de m.cp (PWM_Ext/255) (T_sec - T_amb)
%      hors phases de purge, pour 80 et 200 m3/h (m.cp = rho cp Q / 3600)
%   4. air chaud évacué en purge       intégrale de m.cp,max (T_sec - T_amb) pendant
%      les phases de purge (m.cp,max : 200 m3/h, débit de purge non connu) ;
%      purge = gaz fermé en MODE_H2 / MODE_GPL (purges ET veilles à 0 %, où le
%      programme garde la ventilation de purge), ou post-purge en MODE_SOLAIRE
%   5. évaporation (cycle complet)     9,33 kg x 2,37 MJ/kg
% Part (%) : rapportée à l'énergie fournie dans le modèle (gaz + solaire).
% Produit : Figures/simulation/bilan_pertes.png,
%           Figures/simulation/tables/tab_pertes.tex (3 colonnes),
%           captures/etudes/resultats_E3.txt

function res = etude_E3_pertes(moteur, racine)

    if nargin < 1
        moteur = '';
    end
    if nargin < 2
        racine = '';
    end
    ici = fileparts(mfilename('fullpath'));
    addpath(ici);
    addpath(fileparts(ici));
    fr = chemin_sortie('', 'resultats_E3.txt');
    journal_etude(fr);
    journal_etude(fr, '=== E3 : bilan des pertes (cas C1, 10 h) ===\n');

    r = simuler_etude(cas_etude('C1'), moteur);
    e = r.sc.etude;
    t = r.t(:);
    w = [diff(t); 0];
    dT = r.T_sec(:) - r.T_amb(:);
    etat = round(r.fsm.Etat_LCD(:));
    gazFerme = r.fsm.V_H2(:) == 0 & r.fsm.V_But(:) == 0;
    purge = (ismember(etat, [3 4]) & gazFerme) | (etat == 2 & r.fsm.PWM_Ext(:) == 200);
    J = 1 / 3.6e6;
    rhoCp = 1.06 * 1006;
    mcp = @(Q) rhoCp * Q / 3600;          % W/K pour un débit Q en m3/h

    E.fourni = sum((r.P_gaz(:) + r.P_sol(:)) .* w) * J;
    E.chambre = e.Ceq * (r.T_sec(end) - r.T_sec(1)) * J;
    E.parois = sum(e.UA * dT .* w) * J;
    ventil = r.fsm.PWM_Ext(:) / 255;
    E.air80 = sum(mcp(80) * ventil .* dT .* w .* ~purge) * J;
    E.air200 = sum(mcp(200) * ventil .* dT .* w .* ~purge) * J;
    E.air80_tout = sum(mcp(80) * ventil .* dT .* w) * J;
    E.air200_tout = sum(mcp(200) * ventil .* dT .* w) * J;
    E.purge = sum(mcp(200) * dT .* w .* purge) * J;
    E.evap = 9.33 * 2.37e6 * J;
    dureePurge = sum(w .* purge) / 60;
    duree = (t(end) - t(1)) / 3600;

    journal_etude(fr, 'Moteur : %s\n', r.moteur);
    journal_etude(fr, 'm.cp : %.1f W/K (80 m3/h) ; %.1f W/K (200 m3/h) ; rho = 1,06 kg/m3, cp = 1006 J/(kg.K)\n', ...
                  mcp(80), mcp(200));
    journal_etude(fr, 'Energie fournie dans le modele (gaz + solaire) : %.2f kWh\n', E.fourni);
    journal_etude(fr, 'Controle : chambre + parois = %.2f kWh (ecart %.1f %%)\n', E.chambre + E.parois, ...
                  100 * (E.chambre + E.parois - E.fourni) / E.fourni);
    journal_etude(fr, 'Phases de purge (purges + veilles + post-purge) : %.0f min sur %.1f h\n', dureePurge, duree);
    postes = {'Échauffement de la chambre', E.chambre; ...
              'Pertes par les parois', E.parois; ...
              'Renouvellement d''air, 80 m³/h (estimation)', E.air80; ...
              'Renouvellement d''air, 200 m³/h (estimation)', E.air200; ...
              'Air chaud évacué pendant les purges (estimation)', E.purge; ...
              'Évaporation, cycle complet (estimation)', E.evap};
    lignes = cell(1, size(postes, 1) + 1);
    for i = 1:size(postes, 1)
        part = 100 * postes{i, 2} / E.fourni;
        journal_etude(fr, '  %-50s %7.2f kWh  %6.0f %%\n', postes{i, 1}, postes{i, 2}, part);
        lignes{i} = {postes{i, 1}, nombre_fr(postes{i, 2}, 2), nombre_fr(part, 0)};
    end
    lignes{end} = {'Énergie fournie dans le modèle (gaz + solaire)', nombre_fr(E.fourni, 2), '100'};
    journal_etude(fr, '  (renouvellement d''air sur tout le cycle, purges comprises : %.2f / %.2f kWh)\n', ...
                  E.air80_tout, E.air200_tout);
    for Q = [80 200]
        total = E.chambre + E.parois + E.purge + E.evap + ternaire(Q == 80, E.air80, E.air200);
        journal_etude(fr, 'Si les postes 3 a 5 etaient a la charge du bruleur (%d m3/h) : %.2f kWh, soit %.0f W en moyenne sur %.1f h\n', ...
                      Q, total, 1000 * total / duree, duree);
    end
    journal_etude(fr, 'Puissance moyenne fournie dans le modele : %.0f W\n', 1000 * E.fourni / duree);
    ecrire_table_tex(chemin_sortie(racine, 'Figures/simulation/tables/tab_pertes.tex'), lignes, 3);

    figureBilan(postes, E.fourni, chemin_sortie(racine, 'Figures/simulation/bilan_pertes.png'));
    res = E;
    save(chemin_sortie('', 'E3.mat'), 'res');
    journal_etude(fr, '=== E3 termine ===\n');
end

%% =====================================================================
function s = ternaire(c, a, b)
    if c
        s = a;
    else
        s = b;
    end
end

function figureBilan(postes, fourni, fichier)
    n = size(postes, 1);
    valeurs = cell2mat(postes(:, 2));
    noms = {'Échauffement de la chambre', 'Parois', 'Air neuf 80 m³/h*', 'Air neuf 200 m³/h*', ...
            'Purges et veilles*', 'Évaporation*'};
    couleurs = {couleur_etude('bleu'), couleur_etude('bleu'), couleur_etude('orange'), ...
                couleur_etude('orange'), couleur_etude('violet'), couleur_etude('vert')};
    fig = figure_etude(15, 9);
    hold on;
    for i = 1:n
        y = n - i + 1;
        c = couleurs{i};
        if i > 2
            c = 0.45 * c + 0.55;   % estimation : couleur éclaircie
        end
        patch([0 valeurs(i) valeurs(i) 0], [y - 0.35 y - 0.35 y + 0.35 y + 0.35], c, ...
              'EdgeColor', couleurs{i});
        text(valeurs(i), y, ['  ' nombre_fr(valeurs(i), 2) ' kWh'], 'VerticalAlignment', 'middle', ...
             'FontSize', 9);
    end
    plot([fourni fourni], [0.4 n + 0.6], '--', 'Color', couleur_etude('rouge'), 'LineWidth', 0.8);
    text(fourni, n + 0.6, [' fourni (modèle) : ' nombre_fr(fourni, 2) ' kWh'], 'Color', couleur_etude('rouge'), ...
         'VerticalAlignment', 'bottom', 'FontSize', 9);
    set(gca, 'YTick', 1:n, 'YTickLabel', fliplr(noms), 'YLim', [0.4 n + 1.1]);
    xlim([0 1.3 * max(valeurs)]);
    xlabel('Énergie sur la journée (kWh) — * estimation, hors modèle');
    title('Bilan des pertes, journée claire (cas C1, 10 h)');
    grid on;
    box on;
    enregistrer_etude(fig, fichier);
end
