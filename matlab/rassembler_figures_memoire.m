%% rassembler_figures_memoire.m  —  range toutes les images produites pour le mémoire V4
%
%   rassembler_figures_memoire
%   rassembler_figures_memoire(racine)    racine = dossier memoire_V4_latex
%                                         (défaut : captures/memoire_V4)
%
% Copie chaque image et chaque tableau produits par les scripts (et vos
% captures d'animation) sous le NOM et dans le DOSSIER attendus par le
% mémoire (IMAGES_MANQUANTES.md, partie A et partie C). Si racine n'est pas
% donnée, tout est rangé dans captures/memoire_V4/Figures/... : il suffit
% ensuite de copier ce dossier Figures dans memoire_V4_latex.
% Affiche [ok] ou [MANQUE] pour chaque fichier ; rien n'est supprimé.

function rassembler_figures_memoire(racine)

    dossier = fileparts(mfilename('fullpath'));
    if nargin < 1 || isempty(racine)
        racine = fullfile(dossier, 'captures', 'memoire_V4');
    end
    cap = fullfile(dossier, 'captures');
    mem = fullfile(cap, 'memoire');
    sim = fullfile(cap, 'etudes', 'Figures', 'simulation');

    % {référence, source, destination relative à racine}
    L = {
        'A1',  fullfile(mem, 'chart_hierarchie.png'),        'Figures/simulink/chart_hierarchie.png'
        'A2',  fullfile(mem, 'chart_mode_h2.png'),           'Figures/simulink/chart_mode_h2.png'
        'A3',  fullfile(mem, 'chart_region_phase.png'),      'Figures/simulink/chart_region_phase.png'
        'A4',  fullfile(mem, 'chart_urgence.png'),           'Figures/simulink/chart_urgence.png'
        'A5',  fullfile(dossier, 'figures_fournies', '5.0_banc_tests_terminal.png'), 'Figures/5.0_banc_tests_terminal.png'
        'A6',  fullfile(mem, 'zoom_regulation.png'),         'Figures/simulink/zoom_regulation.png'
        'A7',  fullfile(mem, 'humidite_fin_cycle.png'),      'Figures/simulink/humidite_fin_cycle.png'
        'A8',  fullfile(mem, 'anim_regulation_h2.png'),      'Figures/simulink/anim_regulation_h2.png'
        'A9',  fullfile(mem, 'anim_erreur_combustion.png'),  'Figures/simulink/anim_erreur_combustion.png'
        'A10', fullfile(mem, 'anim_urgence_atex.png'),       'Figures/simulink/anim_urgence_atex.png'
        'A11', fullfile(mem, 'anim_demande_prolongation.png'), 'Figures/simulink/anim_demande_prolongation.png'
        'A12', fullfile(mem, 'ecarts_validation.png'),       'Figures/simulink/ecarts_validation.png'
        'A13', fullfile(mem, 'energie_scenarios.png'),       'Figures/simulink/energie_scenarios.png'
        'A14', fullfile(cap, 'scenario_13.png'),             'Figures/simulink/scenario_13.png'
        '',    fullfile(cap, 'scenario_14.png'),             'Figures/simulink/scenario_14.png'
        '',    fullfile(cap, 'scenario_15.png'),             'Figures/simulink/scenario_15.png'
        '',    fullfile(cap, 'scenario_16.png'),             'Figures/simulink/scenario_16.png'
        '',    fullfile(cap, 'comparaison_13.png'),          'Figures/simulink/comparaison_13.png'
        '',    fullfile(cap, 'comparaison_14.png'),          'Figures/simulink/comparaison_14.png'
        '',    fullfile(cap, 'comparaison_15.png'),          'Figures/simulink/comparaison_15.png'
        '',    fullfile(cap, 'comparaison_16.png'),          'Figures/simulink/comparaison_16.png'
        '',    fullfile(mem, 'modele_global.png'),           'Figures/simulink/modele_global.png'
        '',    fullfile(mem, 'modele_fsm.png'),              'Figures/simulink/modele_fsm.png'
        'A15', fullfile(sim, 'profil_journee.png'),          'Figures/simulation/profil_journee.png'
        'A16', fullfile(sim, 'energie_sources.png'),         'Figures/simulation/energie_sources.png'
        'A17', fullfile(sim, 'tsec_sources.png'),            'Figures/simulation/tsec_sources.png'
        'A18', fullfile(sim, 'modes_tsec.png'),              'Figures/simulation/modes_tsec.png'
        'A19', fullfile(sim, 'modes_energie.png'),           'Figures/simulation/modes_energie.png'
        'A20', fullfile(sim, 'bilan_pertes.png'),            'Figures/simulation/bilan_pertes.png'
        'A21', fullfile(sim, 'sensibilite_retard.png'),      'Figures/simulation/sensibilite_retard.png'
        'A22', fullfile(sim, 'perturbation_porte.png'),      'Figures/simulation/perturbation_porte.png'
        '',    fullfile(sim, 'sensibilite_ua.png'),          'Figures/simulation/sensibilite_ua.png'
        'C',   fullfile(sim, 'tables', 'tab_sources.tex'),   'Figures/simulation/tables/tab_sources.tex'
        'C',   fullfile(sim, 'tables', 'tab_consommation.tex'), 'Figures/simulation/tables/tab_consommation.tex'
        'C',   fullfile(sim, 'tables', 'tab_modes.tex'),     'Figures/simulation/tables/tab_modes.tex'
        'C',   fullfile(sim, 'tables', 'tab_pertes.tex'),    'Figures/simulation/tables/tab_pertes.tex'
        'C',   fullfile(sim, 'tables', 'tab_sensibilite.tex'), 'Figures/simulation/tables/tab_sensibilite.tex'
        };
    for n = 13:16
        L(end + 1, :) = {'', fullfile(dossier, 'reference', sprintf('scenario_%02d_firmware.png', n)), ...
                         sprintf('Figures/reference/scenario_%02d_firmware.png', n)}; %#ok<AGROW>
    end

    fprintf('=== Rassemblement des figures -> %s ===\n', racine);
    manque = {};
    for i = 1:size(L, 1)
        source = L{i, 2};
        morceaux = strsplit(L{i, 3}, '/');
        cible = fullfile(racine, morceaux{:});
        if ~exist(source, 'file')
            fprintf('  [MANQUE] %-4s %s\n', L{i, 1}, source);
            manque{end + 1} = L{i, 3}; %#ok<AGROW>
            continue;
        end
        if ~exist(fileparts(cible), 'dir')
            mkdir(fileparts(cible));
        end
        copyfile(source, cible, 'f');
        fprintf('  [ok]     %-4s %s\n', L{i, 1}, L{i, 3});
    end
    fprintf('=== %d fichiers ranges, %d manquants ===\n', size(L, 1) - numel(manque), numel(manque));
    if isempty(manque)
        fprintf('Copiez le dossier %s dans memoire_V4_latex (fusionner).\n', fullfile(racine, 'Figures'));
    end
end
