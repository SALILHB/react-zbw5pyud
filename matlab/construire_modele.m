%% construire_modele.m  —  construit le modèle de simulation À PARTIR DE ZÉRO
%
%   construire_modele()
%   construire_modele(nom)          (défaut : 'Simulation_Sechoir_Hybride')
%
% Crée un modèle Simulink NEUF (aucun fichier .slx existant n'est réutilisé) :
%
%   FSM (sous-système)
%     Chart Stateflow : logique du firmware Arduino v3 (sechoir_hybride.ino,
%       qui fait foi), langage d'action MATLAB, exécuté toutes les 0,1 s :
%         FONCTIONNEMENT_NORMAL (OR)
%           ATTENTE_DEMARRAGE
%           EN_CYCLE (AND, deux régions parallèles)
%             SOURCE : MODE_SOLAIRE / ERREUR_COMBUSTION / MODE_H2 / MODE_GPL
%                      (MODE_H2 et MODE_GPL : PURGE -> ALLUMAGE -> REGULATION)
%             PHASE  : NORMAL / DEMANDE_PROLONGATION / PROLONGATION
%           SECHAGE_TERMINE
%         URGENCE_ATEX
%     Entrées du scénario (From Workspace, variables sc_<Nom>)
%   CONVERSION_PUISSANCE   électrovannes -> puissance (Pnom = 5000 W)
%   MODELE_THERMIQUE       tau dT/dt = Kth (P + P_sol) + T_amb - T  (Kth = 0,098 ; tau = 3530 s)
%   MODELE_SECHAGE + H_sec humidité de l'air extrait (modèle illustratif)
%   BRULEUR                capteur de flamme simulé (pannes, flamme parasite)
%   SUIVI (Scope) et log_* (To Workspace)
%
% Puis compile le modèle et enregistre <nom>.slx dans ce dossier.
% Suite : lancer_simulation(1). Spécification : docs/FSM_SIMULINK.md.
%
% Version cible : MATLAB R2025b + Simulink + Stateflow.

function construire_modele(nom)

    if nargin < 1
        nom = 'Simulation_Sechoir_Hybride';
    end
    dossier = fileparts(mfilename('fullpath'));
    TE = 0.1;   % pas de calcul (s) : réactivité de la sécurité flamme

    fprintf('=== Construction du modele %s (a partir de zero) ===\n', nom);

    % --- 0. Nouveau modèle vide (l'ancien fichier du même nom est remplacé)
    if bdIsLoaded(nom)
        close_system(nom, 0);
    end
    fichier = fullfile(dossier, [nom '.slx']);
    if exist(fichier, 'file')
        delete(fichier);
    end
    new_system(nom);
    fprintf('  [ok] modele vide cree\n');

    % --- 1. Chart Stateflow dans le sous-système FSM
    fsm = [nom '/FSM'];
    add_block('built-in/Subsystem', fsm, 'Position', [420 60 620 560]);
    chartPath = [fsm '/Chart'];
    add_block('sflib/Chart', chartPath);
    chart = find(sfroot, '-isa', 'Stateflow.Chart', 'Path', chartPath);
    chart.ActionLanguage = 'MATLAB';
    chart.ChartUpdate = 'DISCRETE';
    chart.SampleTime = num2str(TE);
    fprintf('  [ok] chart cree (langage MATLAB, periode %g s)\n', TE);

    creerDonnees(chart);
    h = creerEtats(chart);
    ecrireActionsEtats(h);
    ajouterTransitions(chart, h);

    % --- 2. Entrées du chart (scénario) et sorties du sous-système FSM
    sc = scenario_sechoir(1);
    appliquer_scenario(nom, sc);   % variables sc_* nécessaires à la compilation
    ordreSorties = cablerChart(fsm, chart, chartPath, sc, TE);

    % --- 3. Modèle physique, brûleur, séchage, enregistrement
    construireModelePhysique(nom, fsm, ordreSorties, TE);

    % --- 4. Solveur, mise en page, compilation, enregistrement
    set_param(nom, 'SolverType', 'Fixed-step', 'Solver', 'ode4', ...
        'FixedStep', num2str(TE), 'StopTime', num2str(sc.StopTime), ...
        'ReturnWorkspaceOutputs', 'on');
    fprintf('  [ok] solveur ode4, pas fixe %g s\n', TE);
    try
        Simulink.BlockDiagram.arrangeSystem(fsm);
        Simulink.BlockDiagram.arrangeSystem(nom);
    catch
        % mise en page automatique indisponible : sans conséquence
    end
    save_system(nom, fichier);
    try
        set_param(nom, 'SimulationCommand', 'update');
        fprintf('  [ok] le modele compile sans erreur\n');
    catch e
        fprintf('  [ERREUR] compilation : %s\n', e.message);
        afficherCauses(e, '    ');
        fprintf('  (copiez-moi toute cette liste)\n');
    end
    save_system(nom, fichier);
    open_system(nom);
    fprintf('=== %s.slx enregistre. Etape suivante : lancer_simulation(1) ===\n', nom);
end

%% =====================================================================
%  1. Données du chart
%  =====================================================================

function creerDonnees(chart)
    % Nom, portée, type, valeur initiale. Les entrées et sorties sont créées
    % dans l'ordre de leurs ports. Toutes les DURÉES sont en SECONDES (le
    % menu du firmware affiche certaines durées en minutes : x60).
    specs = {
        % --- Entrées : mesures (ports 1 à 4, reliées au modèle physique)
        'T_sec',               'Input',  'double', []
        'H_sec',               'Input',  'double', []
        'Flame',               'Input',  'uint8',  []
        'T_amb',               'Input',  'double', []
        % --- Entrées : boutons et consignes opérateur (scénario)
        'Btn_Start',           'Input',  'uint8',  []
        'Btn_Stop',            'Input',  'uint8',  []
        'Btn_OK',              'Input',  'uint8',  []
        'Btn_UP',              'Input',  'uint8',  []
        'Btn_DOWN',            'Input',  'uint8',  []
        'Btn_SELECT',          'Input',  'uint8',  []   % bouton MENU du firmware
        'Btn_Rearm',           'Input',  'uint8',  []
        'Mode_Auto',           'Input',  'uint8',  []
        'Choix_Manuel',        'Input',  'uint8',  []   % 1 solaire, 2 H2, 3 GPL
        'T_cible',             'Input',  'double', []
        'H_produit',           'Input',  'double', []
        'Tps_Prolongation',    'Input',  'double', []   % s
        % --- Entrées : capteurs d'environnement et de sécurité (scénario)
        'H_amb',               'Input',  'double', []
        'Press_H2',            'Input',  'double', []   % bar
        'MQ8_H2',              'Input',  'double', []
        'MQ6_But',             'Input',  'double', []
        'AU_Manuel',           'Input',  'uint8',  []   % = AU_Urgence du firmware
        % --- Sorties (actionneurs)
        'V_Fl_1',              'Output', 'uint8',  0    % EV1
        'V_Fl_2',              'Output', 'uint8',  0    % EV2
        'V_Fl_3',              'Output', 'uint8',  0    % EV3
        'V_H2',                'Output', 'uint8',  0
        'V_But',               'Output', 'uint8',  0
        'Spark',               'Output', 'uint8',  0
        'PWM_Inj',             'Output', 'uint8',  0    % = PWM_Distrib
        'PWM_Ext',             'Output', 'uint8',  0
        'PWM_Purge',           'Output', 'uint8',  0
        'Etat_LCD',            'Output', 'uint8',  0    % codes de enum EtatFSM
        'Buzzer',              'Output', 'uint8',  0
        % --- Paramètres opérateur (menu du firmware)
        'T_init_manuel',       'Local', 'double', 25
        'Hhyst',               'Local', 'double', 5
        'Temps_Min_Fin',       'Local', 'double', 7200    % s (120 min)
        'Duree_Max_Cycle',     'Local', 'double', 36000   % s (600 min)
        'Temps_Reponse',       'Local', 'double', 300     % s
        'Temps_Purge',         'Local', 'double', 120     % s
        'Temps_Allumage',      'Local', 'double', 4       % s
        'Periode_Regul',       'Local', 'double', 1       % s
        'Regul_Auto',          'Local', 'uint8',  1
        'Palier_Impose',       'Local', 'uint8',  1       % 0=100 %, 1=67 %, 2=33 %
        'DeltaT_Sol',          'Local', 'double', 20      % T_cap = T_amb + DeltaT_Sol
        'Marge_Sol',           'Local', 'double', 0       % ON = T_cible + Marge_Sol
        'Hyst_Sol',            'Local', 'double', 5       % OFF = ON - Hyst_Sol
        'Seuil_Chaud',         'Local', 'double', 40      % repère FROID / CHAUD
        'Seuil_MQ8',           'Local', 'double', 350
        'Seuil_MQ6',           'Local', 'double', 350
        'Press_H2_Min',        'Local', 'double', 2
        'Marge_Retour_H2',     'Local', 'double', 0.5
        'Fixe_T_amb',          'Local', 'uint8',  0
        'Val_T_amb',           'Local', 'double', 25
        'Fixe_H_amb',          'Local', 'uint8',  0
        'Val_H_amb',           'Local', 'double', 40
        'Fixe_H_sec',          'Local', 'uint8',  0
        'Val_H_sec',           'Local', 'double', 60
        'Fixe_Press',          'Local', 'uint8',  0
        'Val_Press',           'Local', 'double', 5
        % --- Constantes techniques du firmware
        'MAX_ECHECS',          'Local', 'uint16', 3
        'MAX_PERTES_FLAMME',   'Local', 'uint8',  1
        'T_SEC_MAX_SECURITE',  'Local', 'double', 90
        'DELAI_FLAMME_PARASITE','Local','double', 5       % s
        % --- Variables internes
        'T_init',              'Local', 'double', 25
        'T1_seuil',            'Local', 'double', 0
        'T2_seuil',            'Local', 'double', 0
        'T3_seuil',            'Local', 'double', 0
        'H_fin',               'Local', 'double', 0
        'T_amb_e',             'Local', 'double', 25      % grandeurs effectives
        'H_amb_e',             'Local', 'double', 40      %  (mesure ou valeur
        'H_sec_e',             'Local', 'double', 90      %   fixe saisie)
        'Press_e',             'Local', 'double', 0
        'T_cap_est',           'Local', 'double', 0       % T_amb_e + DeltaT_Sol
        'regime_chaud',        'Local', 'uint8',  0
        'Mode_Auto_Eff',       'Local', 'uint8',  0
        'Mode_Auto_prev',      'Local', 'uint8',  255     % 255 : synchro au 1er pas
        'palier',              'Local', 'uint8',  0       % 0=100,1=67,2=33,3=0 %
        'Source_Active',       'Local', 'uint8',  0       % 0=sol,1=H2,2=GPL
        'raison_purge',        'Local', 'uint8',  0       % 0=dem,1=echec,2=bascule,3=palier0
        'nb_echecs_allumage',  'Local', 'uint16', 0
        'nb_pertes_flamme',    'Local', 'uint8',  0
        'Choix_Erreur',        'Local', 'uint8',  0
        'Btn_SELECT_prev',     'Local', 'uint8',  0
        'Btn_UP_prev',         'Local', 'uint8',  0
        'Btn_DOWN_prev',       'Local', 'uint8',  0
        'cause_urgence',       'Local', 'uint8',  0       % 1=H2,2=GPL,3=AU,4=perte,5=parasite
        'post_purge',          'Local', 'uint8',  0
        't_cycle',             'Local', 'double', 0       % s depuis Btn_Start
        't_derniere_regul',    'Local', 'double', 0
        'motif_demande',       'Local', 'uint8',  0       % 0=humidite,1=duree max,2=fin prolong.
        'humidite_deja_atteinte','Local','uint8', 0
        'duree_prolongation',  'Local', 'double', 1800    % s, N choisi
    };
    for i = 1:size(specs, 1)
        d = Stateflow.Data(chart);
        d.Name = specs{i,1};
        d.Scope = specs{i,2};
        d.DataType = specs{i,3};
        if ~isempty(specs{i,4})
            d.Props.InitialValue = num2str(specs{i,4});
        end
    end
    nIn = sum(strcmp(specs(:,2), 'Input'));
    nOut = sum(strcmp(specs(:,2), 'Output'));
    fprintf('  [ok] %d donnees : %d entrees, %d sorties, %d parametres et variables\n', ...
        size(specs, 1), nIn, nOut, size(specs, 1) - nIn - nOut);
end

%% =====================================================================
%  2. États (chaque état est créé directement dans son parent)
%  =====================================================================

function h = creerEtats(chart)
    h.chart = chart;
    h.fn  = etat(chart, 'FONCTIONNEMENT_NORMAL', [20 20 1180 1000]);
    h.urg = etat(chart, 'URGENCE_ATEX',          [20 1060 420 170]);
    h.att = etat(h.fn,  'ATTENTE_DEMARRAGE',     [60 70 220 70]);
    h.ter = etat(h.fn,  'SECHAGE_TERMINE',       [940 70 220 70]);
    h.en  = etat(h.fn,  'EN_CYCLE',              [40 180 1140 820]);
    h.en.Decomposition = 'PARALLEL_AND';
    h.src = etat(h.en,  'SOURCE',                [60 220 720 760]);
    h.pha = etat(h.en,  'PHASE',                 [800 220 360 760]);
    ordreExecution(h.src, 1);   % comme le firmware : source, puis fin de cycle
    ordreExecution(h.pha, 2);

    h.sol = etat(h.src, 'MODE_SOLAIRE',          [90 270 220 110]);
    h.err = etat(h.src, 'ERREUR_COMBUSTION',     [340 270 420 110]);
    h.h2  = etat(h.src, 'MODE_H2',               [90 410 670 260]);
    h.gpl = etat(h.src, 'MODE_GPL',              [90 690 670 260]);
    h.sm_h2  = sousMachine(h.h2,  460);
    h.sm_gpl = sousMachine(h.gpl, 740);

    h.nor = etat(h.pha, 'NORMAL',                [830 270 300 90]);
    h.dem = etat(h.pha, 'DEMANDE_PROLONGATION',  [830 410 300 260]);
    h.pro = etat(h.pha, 'PROLONGATION',          [830 690 300 260]);

    fprintf('  [ok] %d etats crees (EN_CYCLE : %s)\n', ...
        numel(chart.find('-isa', 'Stateflow.State')), h.en.Decomposition);
end

function s = etat(parent, nom, position)
    s = Stateflow.State(parent);
    s.LabelString = nom;
    s.Position = position;
end

function sm = sousMachine(mode, y)
    sm.purge      = etat(mode, 'PURGE',      [110 y 190 190]);
    sm.allumage   = etat(mode, 'ALLUMAGE',   [320 y 190 190]);
    sm.regulation = etat(mode, 'REGULATION', [530 y 210 190]);
end

function ordreExecution(s, n)
    try
        s.ExecutionOrder = n;
    catch
        % ordre par défaut (position graphique) : SOURCE est à gauche, donc
        % exécutée avant PHASE, comme dans le firmware.
    end
end

function t = TAG()
    t = 'construire_modele';
end

%% =====================================================================
%  3. Actions des états et 4. transitions (logique du firmware v3)
%  =====================================================================

function n = nomEtat(s)
    lignes = strsplit(s.LabelString, sprintf('\n'));
    n = strtrim(lignes{1});
end

function c = cheminComplet(obj)
    % Chemin hiérarchique d'un état : Path (chemin du parent) + nom.
    if isa(obj, 'Stateflow.Chart')
        c = obj.Path;
    else
        c = [obj.Path '/' nomEtat(obj)];
    end
end

function s = txtFermerGaz()
    s = 'V_H2=uint8(0); V_But=uint8(0); V_Fl_1=uint8(0); V_Fl_2=uint8(0); V_Fl_3=uint8(0); Spark=uint8(0);';
end

function s = txtAppliquerPalier()
    s = 'V_Fl_1=uint8(palier==0); V_Fl_2=uint8(palier==0 || palier==1); V_Fl_3=uint8(palier~=3);';
end

function s = txtVentilPalier()
    % PWM_DISTRIB_PAR_PALIER / PWM_EXTRACT_PAR_PALIER du firmware.
    s = ['PWM_Purge=uint8(60); if palier==0, PWM_Inj=uint8(255); PWM_Ext=uint8(200); ' ...
         'elseif palier==1, PWM_Inj=uint8(210); PWM_Ext=uint8(165); ' ...
         'else, PWM_Inj=uint8(165); PWM_Ext=uint8(130); end'];
end

function s = txtCalculerSeuils()
    s = ['if T_cible <= T_init + 3, T1_seuil=T_cible; T2_seuil=T_cible; T3_seuil=T_cible; ' ...
         'else, T1_seuil=T_init+(T_cible-T_init)/3; T2_seuil=T_init+2*(T_cible-T_init)/3; ' ...
         'T3_seuil=T_cible; end'];
end

function s = txtPalierInitial()
    % palierInitial() + armerPurgeMiseEnRoute() du firmware.
    s = ['if Regul_Auto==0, palier=Palier_Impose; ' ...
         'elseif regime_chaud==0, palier=uint8(0); ' ...
         'elseif T_sec < T1_seuil, palier=uint8(0); ' ...
         'elseif T_sec < T2_seuil, palier=uint8(1); ' ...
         'elseif T_sec < T3_seuil, palier=uint8(2); ' ...
         'else, palier=uint8(3); end; ' ...
         'if palier==3, raison_purge=uint8(3); else, raison_purge=uint8(0); end'];
end

function s = txtGererPalier()
    % majPalier() du firmware : hystérésis 4 niveaux mémorisée par `palier`.
    s = sprintf([ ...
        'if palier==0\n' ...
        '  if T_sec >= T1_seuil + Hhyst/2, palier=uint8(1); end\n' ...
        'elseif palier==1\n' ...
        '  if T_sec >= T2_seuil + Hhyst/2, palier=uint8(2); elseif T_sec < T1_seuil - Hhyst/2, palier=uint8(0); end\n' ...
        'elseif palier==2\n' ...
        '  if T_sec >= T3_seuil + Hhyst/2, palier=uint8(3); elseif T_sec < T2_seuil - Hhyst/2, palier=uint8(1); end\n' ...
        'end']);
end

function s = txtCondHumidite()
    % conditionHumidite() du firmware (t_cycle = temps depuis Btn_Start).
    s = 't_cycle >= Temps_Min_Fin && H_sec_e <= H_fin';
end

function s = txtCondSolaireOn()
    s = 'T_cap_est >= T_cible + Marge_Sol';
end

function s = txtCondSolaireMaintien()
    % seuilMaintienSolaire() : ON en régime FROID, OFF en régime CHAUD.
    s = ['((regime_chaud==1 && T_cap_est >= T_cible + Marge_Sol - Hyst_Sol) || ' ...
         '(regime_chaud==0 && T_cap_est >= T_cible + Marge_Sol))'];
end

function ecrireLabel(s, lignes)
    s.LabelString = strjoin([{nomEtat(s)}, lignes], sprintf('\n'));
end

function ecrireActionsEtats(h)
    fprintf('--- Actions des etats ---\n');
    fg = txtFermerGaz();

    % FONCTIONNEMENT_NORMAL : acquisition (lireEntrees() du firmware) —
    % grandeurs FIXE/AUTO, T_cap estimée, H_fin, repère FROID/CHAUD,
    % resynchronisation du mode auto quand l'opérateur change le sélecteur.
    ecrireLabel(h.fn, {sprintf([ ...
        'entry, during:\n' ...
        'if Fixe_T_amb==1, T_amb_e=Val_T_amb; else, T_amb_e=T_amb; end\n' ...
        'if Fixe_H_amb==1, H_amb_e=Val_H_amb; else, H_amb_e=H_amb; end\n' ...
        'if Fixe_H_sec==1, H_sec_e=Val_H_sec; else, H_sec_e=H_sec; end\n' ...
        'if Fixe_Press==1, Press_e=Val_Press; else, Press_e=Press_H2; end\n' ...
        'T_cap_est = T_amb_e + DeltaT_Sol;\n' ...
        'H_fin = min(H_produit + H_amb_e, 95);\n' ...
        'if regime_chaud==0 && T_sec >= Seuil_Chaud + Hhyst/2, regime_chaud=uint8(1);\n' ...
        'elseif regime_chaud==1 && T_sec < Seuil_Chaud - Hhyst/2, regime_chaud=uint8(0); end\n' ...
        'if Mode_Auto ~= Mode_Auto_prev, Mode_Auto_Eff=Mode_Auto; Mode_Auto_prev=Mode_Auto; end'])});

    ecrireLabel(h.att, {['entry: ' fg ' PWM_Purge=uint8(0); PWM_Inj=uint8(0); PWM_Ext=uint8(0); Buzzer=uint8(0); Etat_LCD=uint8(0);']});

    ecrireLabel(h.ter, {['entry: ' fg ' PWM_Purge=uint8(0); PWM_Inj=uint8(0); PWM_Ext=uint8(90); Buzzer=uint8(0); Etat_LCD=uint8(7);']});

    ecrireLabel(h.urg, { ...
        ['entry: ' fg ' PWM_Purge=uint8(255); PWM_Inj=uint8(0); PWM_Ext=uint8(255); Buzzer=uint8(1); Etat_LCD=uint8(9);'], ...
        ['if cause_urgence==0, if AU_Manuel==1, cause_urgence=uint8(3); ' ...
         'elseif MQ8_H2 >= Seuil_MQ8, cause_urgence=uint8(1); else, cause_urgence=uint8(2); end; end'], ...
        ['during: ' fg]});

    % EN_CYCLE : demarrerCycle() du firmware (une seule fois par cycle).
    ecrireLabel(h.en, {sprintf([ ...
        'entry:\n' ...
        'if Mode_Auto_Eff==1, T_init=T_sec; else, T_init=T_init_manuel; end\n' ...
        '%s\n' ...
        'regime_chaud = uint8(T_sec >= Seuil_Chaud);\n' ...
        'nb_echecs_allumage=uint16(0); nb_pertes_flamme=uint8(0);\n' ...
        'humidite_deja_atteinte=uint8(0); post_purge=uint8(0); t_cycle=0;\n' ...
        '%s\n' ...
        'during:\n' ...
        't_cycle = temporalCount(sec);\n' ...
        '%s\n' ...
        'if in(SOURCE.ERREUR_COMBUSTION), Etat_LCD=uint8(8);\n' ...
        'elseif in(PHASE.DEMANDE_PROLONGATION), Etat_LCD=uint8(5);\n' ...
        'elseif in(PHASE.PROLONGATION), Etat_LCD=uint8(6);\n' ...
        'elseif in(SOURCE.MODE_H2), Etat_LCD=uint8(3);\n' ...
        'elseif in(SOURCE.MODE_GPL), Etat_LCD=uint8(4);\n' ...
        'else, Etat_LCD=uint8(2); end'], ...
        txtCalculerSeuils(), txtPalierInitial(), txtCalculerSeuils())});

    ecrireLabel(h.src, {});
    ecrireLabel(h.pha, {});
    ecrireLabel(h.nor, {});

    % MODE_SOLAIRE : etapeSolaire() — post-purge après extinction du brûleur.
    ecrireLabel(h.sol, {sprintf([ ...
        'entry, during:\n' ...
        '%s Source_Active=uint8(0);\n' ...
        'if post_purge==1 && temporalCount(sec) >= Temps_Purge, post_purge=uint8(0); end\n' ...
        'if post_purge==1, PWM_Purge=uint8(255); PWM_Inj=uint8(60); PWM_Ext=uint8(200);\n' ...
        'else, PWM_Purge=uint8(0); PWM_Inj=uint8(220); PWM_Ext=uint8(150); end'], fg)});

    ecrireLabel(h.h2,  {'entry: Source_Active=uint8(1);'});
    ecrireLabel(h.gpl, {'entry: Source_Active=uint8(2);'});

    ecrireLabel(h.err, {sprintf([ ...
        'entry: %s Buzzer=uint8(1); PWM_Purge=uint8(255); PWM_Inj=uint8(0); PWM_Ext=uint8(200);\n' ...
        'Choix_Erreur=uint8(0); Btn_SELECT_prev=Btn_SELECT;\n' ...
        'during:\n' ...
        'if Btn_SELECT==1 && Btn_SELECT_prev==0, Choix_Erreur=uint8(mod(double(Choix_Erreur)+1, 3)); end\n' ...
        'Btn_SELECT_prev=Btn_SELECT;'], fg)});

    ecrireLabel(h.dem, {sprintf([ ...
        'entry: Btn_UP_prev=Btn_UP; Btn_DOWN_prev=Btn_DOWN;\n' ...
        'during:\n' ...
        'if humidite_deja_atteinte==0 && %s, humidite_deja_atteinte=uint8(1); motif_demande=uint8(0); end\n' ...
        'Btn_UP_prev=Btn_UP; Btn_DOWN_prev=Btn_DOWN;'], txtCondHumidite())});

    ecrireLabel(h.pro, {});

    ecrireSousMachine(h.sm_h2,  'V_H2=uint8(1); V_But=uint8(0);');
    ecrireSousMachine(h.sm_gpl, 'V_H2=uint8(0); V_But=uint8(1);');
    fprintf('  [ok] libelles ecrits\n');
end

function ecrireSousMachine(sm, actionVanne)
    fg = txtFermerGaz();
    ecrireLabel(sm.purge, {['entry: ' fg ' PWM_Purge=uint8(255); PWM_Inj=uint8(60); PWM_Ext=uint8(200);']});
    ecrireLabel(sm.allumage, {['entry: ' actionVanne ' ' txtAppliquerPalier() ' Spark=uint8(1); ' txtVentilPalier()]});
    % REGULATION : palier imposé, ou comparaison T_sec / seuils toutes les
    % Periode_Regul secondes. Le gaz n'est JAMAIS fermé ici au palier 0 % :
    % c'est la transition [palier==3] qui le fait, APRÈS le contrôle de
    % flamme (sinon l'extinction volontaire serait prise pour une panne).
    ecrireLabel(sm.regulation, {sprintf([ ...
        'entry: %s t_derniere_regul=0;\n' ...
        'during:\n' ...
        'if Regul_Auto==0\n' ...
        '  palier=Palier_Impose;\n' ...
        'elseif temporalCount(sec) - t_derniere_regul >= Periode_Regul\n' ...
        '  t_derniere_regul = temporalCount(sec);\n' ...
        '%s\n' ...
        'end\n' ...
        'if palier ~= 3\n  %s\n  %s\nend'], actionVanne, txtGererPalier(), txtAppliquerPalier(), txtVentilPalier())});
end

function t = transition(chart, src, dst, label)
    % Crée une transition dans le plus petit ancêtre commun de src et dst
    % (src vide = transition par défaut, créée dans le parent de dst).
    if isempty(src)
        parent = objetParChemin(chart, dst.Path);
    else
        parent = objetParChemin(chart, ancetreCommun(src, dst));
    end
    t = Stateflow.Transition(parent);
    if ~isempty(src)
        t.Source = src;
    end
    t.Destination = dst;
    try   % cosmétique uniquement : tracé lisible des défauts et des boucles
        if isempty(src)
            t.DestinationOClock = 0;
            t.SourceEndpoint = t.DestinationEndpoint - [0 25];
            t.Midpoint = t.DestinationEndpoint - [0 12];
        elseif src == dst
            t.SourceOClock = 2;
            t.DestinationOClock = 4;
        end
    catch
    end
    t.LabelString = label;
    t.Description = TAG();
end

function c = ancetreCommun(a, b)
    % Plus long préfixe commun (par segments) des chemins des parents.
    pa = strsplit(a.Path, '/');
    pb = strsplit(b.Path, '/');
    n = min(numel(pa), numel(pb));
    k = 0;
    while k < n && strcmp(pa{k+1}, pb{k+1})
        k = k + 1;
    end
    c = strjoin(pa(1:k), '/');
    if a == b   % auto-transition
        c = a.Path;
    end
end

function o = objetParChemin(chart, chemin)
    if strcmp(chemin, chart.Path)
        o = chart;
        return;
    end
    tous = chart.find('-isa', 'Stateflow.State');
    for i = 1:numel(tous)
        if strcmp(cheminComplet(tous(i)), chemin)
            o = tous(i);
            return;
        end
    end
    o = chart;   % repli : Stateflow reclasse la transition
end

function ajouterTransitions(chart, h)
    fprintf('--- Transitions ---\n');
    fg = txtFermerGaz();
    ERR = 'in(EN_CYCLE.SOURCE.ERREUR_COMBUSTION)';
    n0 = numel(chart.find('-isa', 'Stateflow.Transition'));

    % --- Transitions par défaut (une par état OR ayant des sous-états).
    transition(chart, [], h.fn, '');                        % chart
    transition(chart, [], h.att, '');                       % FONCTIONNEMENT_NORMAL
    transition(chart, [], h.sol, '');                       % SOURCE (sécurité : pas de gaz)
    transition(chart, [], h.nor, '');                       % PHASE
    transition(chart, [], h.sm_h2.purge, '');
    transition(chart, [], h.sm_gpl.purge, '');

    % --- 1a. URGENCE : fuite H2 / GPL ou arrêt d'urgence (priorité maximale :
    %     transition la plus externe, créée en premier).
    transition(chart, h.fn, h.urg, '[MQ8_H2 >= Seuil_MQ8 || MQ6_But >= Seuil_MQ6 || AU_Manuel == 1]');
    % --- 1b. URGENCE : flamme vue alors que le gaz est commandé fermé.
    transition(chart, h.fn, h.urg, sprintf( ...
        '[duration(Flame==1 && V_H2==0 && V_But==0) >= DELAI_FLAMME_PARASITE]{cause_urgence=uint8(5);}'));

    % --- Réarmement : seulement si aucune cause présente (flamme comprise).
    %     Retour à ATTENTE_DEMARRAGE : le cycle n'est jamais repris.
    transition(chart, h.urg, h.att, ['[(Btn_OK==1 || Btn_Rearm==1) && MQ8_H2 < Seuil_MQ8 && ' ...
        'MQ6_But < Seuil_MQ6 && AU_Manuel==0 && Flame==0]' ...
        '{Buzzer=uint8(0); palier=uint8(0); raison_purge=uint8(0); cause_urgence=uint8(0);}']);

    % --- 2. Btn_Stop et 3. surchauffe (bord de EN_CYCLE : tout le cycle).
    transition(chart, h.en, h.ter, ['[Btn_Stop==1]{' fg '}']);
    transition(chart, h.en, h.ter, ['[T_sec >= T_SEC_MAX_SECURITE && ~' ERR ']{' fg '}']);

    % --- Fin du refroidissement / relance depuis SECHAGE_TERMINE.
    transition(chart, h.ter, h.att, '[after(300, sec) || Btn_Start==1]');

    % --- Démarrage automatique (demarrerCycle) : FROID -> seuil ON seul,
    %     CHAUD (T_sec >= Seuil_Chaud) -> solaire accepté dès OFF.
    condSol = ['((T_sec >= Seuil_Chaud && T_cap_est >= T_cible + Marge_Sol - Hyst_Sol) || ' ...
               txtCondSolaireOn() ')'];
    transition(chart, h.att, h.sol, ['[Btn_Start==1 && Mode_Auto_Eff==1 && ' condSol ']']);
    transition(chart, h.att, h.h2,  ['[Btn_Start==1 && Mode_Auto_Eff==1 && ~' condSol ' && Press_e >= Press_H2_Min]']);
    transition(chart, h.att, h.gpl, ['[Btn_Start==1 && Mode_Auto_Eff==1 && ~' condSol ' && Press_e < Press_H2_Min]']);
    % Démarrage manuel : Choix_Manuel 1 = solaire, 2 = H2, 3 = GPL.
    transition(chart, h.att, h.sol, '[Btn_Start==1 && Mode_Auto_Eff==0 && Choix_Manuel==1]');
    transition(chart, h.att, h.h2,  '[Btn_Start==1 && Mode_Auto_Eff==0 && Choix_Manuel==2]');
    transition(chart, h.att, h.gpl, '[Btn_Start==1 && Mode_Auto_Eff==0 && Choix_Manuel==3]');

    % --- Arbitrage de source (arbitrageSource), priorité 1 = retour solaire.
    retourSol = ['[Mode_Auto_Eff==1 && ' txtCondSolaireOn() ']{' fg ' post_purge=uint8(1);}'];
    transition(chart, h.h2,  h.sol, retourSol);
    transition(chart, h.gpl, h.sol, retourSol);
    versComb = ['Mode_Auto_Eff==1 && ~' txtCondSolaireMaintien()];
    actComb  = ['{post_purge=uint8(0); nb_echecs_allumage=uint16(0); ' txtPalierInitial() '}'];
    transition(chart, h.sol, h.h2,  ['[' versComb ' && Press_e >= Press_H2_Min]' actComb]);
    transition(chart, h.sol, h.gpl, ['[' versComb ' && Press_e < Press_H2_Min]' actComb]);
    transition(chart, h.h2,  h.gpl, '[Mode_Auto_Eff==1 && Press_e < Press_H2_Min]{raison_purge=uint8(2); nb_echecs_allumage=uint16(0);}');
    transition(chart, h.gpl, h.h2,  '[Mode_Auto_Eff==1 && Press_e >= Press_H2_Min + Marge_Retour_H2]{raison_purge=uint8(2); nb_echecs_allumage=uint16(0);}');

    % --- Sous-machines de combustion.
    ajouterTransitionsCombustion(chart, h, h.sm_h2);
    ajouterTransitionsCombustion(chart, h, h.sm_gpl);

    % --- ERREUR_COMBUSTION : reprise au palier mémorisé après purge.
    reprise = '{if Choix_Erreur==2, Mode_Auto_Eff=uint8(1); end; nb_echecs_allumage=uint16(0); raison_purge=uint8(1); Buzzer=uint8(0);}';
    transition(chart, h.err, h.h2,  ['[Btn_OK==1 && (Choix_Erreur==0 || Choix_Erreur==2) && Source_Active==1]' reprise]);
    transition(chart, h.err, h.gpl, ['[Btn_OK==1 && (Choix_Erreur==0 || Choix_Erreur==2) && Source_Active==2]' reprise]);
    transition(chart, h.err, h.att, '[Btn_OK==1 && Choix_Erreur==1]{Mode_Auto_Eff=uint8(0); Buzzer=uint8(0);}');
    transition(chart, h.err, h.ter, '[Btn_Stop==1]{Buzzer=uint8(0);}');

    % --- Région PHASE (fin de cycle, V3).
    hum = txtCondHumidite();
    % Une erreur de combustion ramène la phase à NORMAL (dans le firmware,
    % REESSAYER/AUTOMATIQUE repassent par MODE_x) : créées EN PREMIER sur
    % DEMANDE et PROLONGATION pour être prioritaires.
    transition(chart, h.dem, h.nor, ['[' ERR ']']);
    transition(chart, h.pro, h.nor, ['[' ERR ']']);
    transition(chart, h.nor, h.dem, ['[~' ERR ' && ' hum ']{motif_demande=uint8(0); humidite_deja_atteinte=uint8(1); duree_prolongation=Tps_Prolongation;}']);
    transition(chart, h.nor, h.dem, ['[~' ERR ' && t_cycle >= Duree_Max_Cycle]{motif_demande=uint8(1); duree_prolongation=Tps_Prolongation;}']);
    transition(chart, h.dem, h.pro, '[Btn_OK==1]');
    transition(chart, h.dem, h.ter, ['[Btn_Stop==1]{' fg '}']);
    transition(chart, h.dem, h.ter, ['[after(Temps_Reponse, sec)]{' fg '}']);
    transition(chart, h.dem, h.dem, '[Btn_UP==1 && Btn_UP_prev==0]{duree_prolongation=min(duree_prolongation+300, 43200);}');
    transition(chart, h.dem, h.dem, '[Btn_DOWN==1 && Btn_DOWN_prev==0]{duree_prolongation=max(duree_prolongation-300, 300);}');
    transition(chart, h.pro, h.dem, ['[humidite_deja_atteinte==0 && ' hum ']{motif_demande=uint8(0); humidite_deja_atteinte=uint8(1); duree_prolongation=Tps_Prolongation;}']);
    transition(chart, h.pro, h.dem, '[after(duree_prolongation, sec)]{motif_demande=uint8(2); duree_prolongation=Tps_Prolongation;}');

    n1 = numel(chart.find('-isa', 'Stateflow.Transition'));
    fprintf('  [ok] %d transitions creees\n', n1 - n0);
end

function ajouterTransitionsCombustion(chart, h, sm)
    % gererCombustion() du firmware. Ordre de création = priorité : le
    % contrôle de flamme passe avant la coupure volontaire au palier 0 %.
    % Veille à 0 % (raison 3 OU palier 3, p. ex. bascule pendant la veille) :
    % rallumage seulement quand la demande revient, au palier 33 %.
    transition(chart, sm.purge, sm.allumage, ...
        ['[after(Temps_Purge, sec) && ((raison_purge ~= 3 && palier ~= 3) || T_sec < T3_seuil - Hhyst/2)]' ...
         '{if raison_purge==3 || palier==3, palier=uint8(2); end}']);
    transition(chart, sm.allumage, sm.regulation, '[Flame==1]{Spark=uint8(0); nb_echecs_allumage=uint16(0);}');
    transition(chart, sm.allumage, sm.purge, ...
        '[after(Temps_Allumage, sec) && Flame==0 && nb_echecs_allumage + 1 < MAX_ECHECS]{nb_echecs_allumage=nb_echecs_allumage+1; raison_purge=uint8(1);}');
    transition(chart, sm.allumage, h.err, ...
        '[after(Temps_Allumage, sec) && Flame==0 && nb_echecs_allumage + 1 >= MAX_ECHECS]{nb_echecs_allumage=nb_echecs_allumage+1;}');
    transition(chart, sm.regulation, sm.purge, ...
        ['[Flame==0 && nb_pertes_flamme < MAX_PERTES_FLAMME]{' txtFermerGaz() ' nb_pertes_flamme=nb_pertes_flamme+1; raison_purge=uint8(1);}']);
    transition(chart, sm.regulation, h.urg, ...
        '[Flame==0 && nb_pertes_flamme >= MAX_PERTES_FLAMME]{cause_urgence=uint8(4);}');
    transition(chart, sm.regulation, sm.purge, '[palier==3]{raison_purge=uint8(3);}');
end

%% =====================================================================
%  5. Blocs Simulink : câblage du chart et modèle physique
%  =====================================================================

function ordreSorties = cablerChart(fsm, chart, chartPath, sc, TE)
    % Entrées 1 à 4 : ports du sous-système FSM (modèle physique) ; les
    % autres : From Workspace (variables sc_<Nom> du scénario).
    externes = {'T_sec', 'H_sec', 'Flame', 'T_amb'};
    hChart = get_param(chartPath, 'Handle');
    entrees = chart.find('-isa', 'Stateflow.Data', 'Scope', 'Input');
    sorties = chart.find('-isa', 'Stateflow.Data', 'Scope', 'Output');
    nMax = max(numel(entrees), numel(sorties));
    set_param(chartPath, 'Position', [420 20 620 40 + 40 * nMax]);

    for i = 1:numel(entrees)
        d = entrees(i);
        y = 20 + 40 * (d.Port - 1);
        if any(strcmp(d.Name, externes))
            hSrc = add_block('simulink/Sources/In1', [fsm '/' d.Name], ...
                'Position', [40 y 70 y + 14]);
        else
            if ~isfield(sc.signaux, d.Name)
                fprintf('  [ATTENTION] entree %s absente de scenario_sechoir : ajoutez-la\n', d.Name);
            end
            hSrc = add_block('simulink/Sources/From Workspace', [fsm '/sc_' d.Name], ...
                'Position', [20 y - 5 150 y + 20], ...
                'VariableName', ['sc_' d.Name], 'SampleTime', num2str(TE), ...
                'Interpolate', 'off', 'OutputAfterFinalValue', 'Holding final value');
        end
        if strcmp(d.DataType, 'uint8')
            hConv = add_block('simulink/Signal Attributes/Data Type Conversion', ...
                [fsm '/uint8_' d.Name], 'Position', [230 y 290 y + 20], 'OutDataTypeStr', 'uint8');
            lier(fsm, hSrc, 1, hConv, 1);
            hSrc = hConv;
        end
        lier(fsm, hSrc, 1, hChart, d.Port);
    end
    for k = 1:numel(externes)   % numéros de ports fixés une fois tous créés
        set_param([fsm '/' externes{k}], 'Port', num2str(k));
    end

    ordreSorties = cell(1, numel(sorties));
    for i = 1:numel(sorties)
        d = sorties(i);
        y = 20 + 40 * (d.Port - 1);
        hOut = add_block('simulink/Sinks/Out1', [fsm '/' d.Name], 'Position', [760 y 790 y + 14]);
        lier(fsm, hChart, d.Port, hOut, 1);
        ordreSorties{d.Port} = d.Name;
    end
    for k = 1:numel(ordreSorties)
        set_param([fsm '/' ordreSorties{k}], 'Port', num2str(k));
    end
    fprintf('  [ok] chart cable : %d entrees (%s via ports, le reste via From Workspace), %d sorties\n', ...
        numel(entrees), strjoin(externes, ', '), numel(sorties));
end

function construireModelePhysique(nom, fsm, ordreSorties, TE)
    hFSM = get_param(fsm, 'Handle');
    port = @(n) find(strcmp(ordreSorties, n));   % numéro de sortie FSM d'un signal

    % Puissance de gaz : électrovannes de rampe -> watts
    hConvP = add_block('simulink/User-Defined Functions/MATLAB Function', ...
        [nom '/CONVERSION_PUISSANCE'], 'Position', [700 60 820 160]);
    ecrireFonction(getfullname(hConvP), codeConversion());
    lier(nom, hFSM, port('V_Fl_1'), hConvP, 1);
    lier(nom, hFSM, port('V_Fl_2'), hConvP, 2);
    lier(nom, hFSM, port('V_Fl_3'), hConvP, 3);

    % Apport solaire et puissance totale
    hPsol = fromWs(nom, 'P_sol', [700 200 820 225], TE);
    hSomme = add_block('simulink/Math Operations/Sum', [nom '/Puissance_totale'], ...
        'Inputs', '++', 'Position', [880 100 910 130]);
    lier(nom, hConvP, 1, hSomme, 1);
    lier(nom, hPsol, 1, hSomme, 2);

    % Chambre de séchage (1er ordre)
    hTamb = fromWs(nom, 'T_amb', [40 600 170 625], TE);
    hTherm = construireThermique(nom);
    lier(nom, hSomme, 1, hTherm, 1);
    lier(nom, hTamb, 1, hTherm, 2);
    lier(nom, hTherm, 1, hFSM, 1);
    lier(nom, hTamb, 1, hFSM, 4);

    % Séchage : dH/dt = f(T_sec, H), intégré -> H_sec
    hSech = add_block('simulink/User-Defined Functions/MATLAB Function', ...
        [nom '/MODELE_SECHAGE'], 'Position', [980 300 1090 350]);
    ecrireFonction(getfullname(hSech), codeSechage());
    hIntH = add_block('simulink/Continuous/Integrator', [nom '/H_sec'], ...
        'InitialCondition', 'sc_H0', 'Position', [1140 305 1170 345]);
    lier(nom, hTherm, 1, hSech, 1);
    lier(nom, hSech, 1, hIntH, 1);
    lier(nom, hIntH, 1, hSech, 2);
    lier(nom, hIntH, 1, hFSM, 2);

    % Brûleur : flamme si le gaz est ouvert (au pas précédent), sauf panne
    hBru = add_block('simulink/User-Defined Functions/MATLAB Function', ...
        [nom '/BRULEUR'], 'Position', [240 620 340 720]);
    ecrireFonction(getfullname(hBru), codeBruleur());
    hRetH2 = add_block('simulink/Discrete/Unit Delay', [nom '/Retard_V_H2'], ...
        'SampleTime', num2str(TE), 'Position', [700 620 730 650]);
    hRetBut = add_block('simulink/Discrete/Unit Delay', [nom '/Retard_V_But'], ...
        'SampleTime', num2str(TE), 'Position', [700 680 730 710]);
    hPanne = fromWs(nom, 'Flamme_panne', [40 760 170 785], TE);
    hParas = fromWs(nom, 'Flamme_parasite', [40 820 170 845], TE);
    lier(nom, hFSM, port('V_H2'), hRetH2, 1);
    lier(nom, hFSM, port('V_But'), hRetBut, 1);
    lier(nom, hRetH2, 1, hBru, 1);
    lier(nom, hRetBut, 1, hBru, 2);
    lier(nom, hPanne, 1, hBru, 3);
    lier(nom, hParas, 1, hBru, 4);
    lier(nom, hBru, 1, hFSM, 3);

    % Enregistrement (sorties de sim) et Scope de suivi
    hMux = add_block('simulink/Signal Routing/Mux', [nom '/Sorties_FSM'], ...
        'Inputs', num2str(numel(ordreSorties)), 'Position', [1100 420 1105 700]);
    for k = 1:numel(ordreSorties)
        lier(nom, hFSM, k, hMux, k);
    end
    journal(nom, hMux, 'log_fsm', [1160 545 1250 575]);
    journal(nom, hTherm, 'log_T_sec', [1160 40 1250 70]);
    journal(nom, hIntH, 'log_H_sec', [1260 300 1350 330]);
    journal(nom, hConvP, 'log_P_gaz', [1160 150 1250 180]);
    journal(nom, hBru, 'log_Flame', [1160 620 1250 650]);
    hScope = add_block('simulink/Sinks/Scope', [nom '/SUIVI'], ...
        'NumInputPorts', '3', 'Position', [1300 200 1340 280]);
    lier(nom, hTherm, 1, hScope, 1);
    lier(nom, hIntH, 1, hScope, 2);
    lier(nom, hFSM, port('Etat_LCD'), hScope, 3);
    fprintf('  [ok] modele physique : puissance, thermique, sechage, bruleur, Scope, enregistrement\n');
end

function h = construireThermique(nom)
    % tau dT_sec/dt = Kth * P + T_amb - T_sec   (Kth = 0,098 K/W ; tau = 3530 s)
    sys = [nom '/MODELE_THERMIQUE'];
    h = add_block('built-in/Subsystem', sys, 'Position', [960 90 1080 150]);
    hP = add_block('simulink/Sources/In1', [sys '/P'], 'Position', [30 40 60 54]);
    hTa = add_block('simulink/Sources/In1', [sys '/T_amb'], 'Position', [30 110 60 124]);
    hK = add_block('simulink/Math Operations/Gain', [sys '/Kth'], 'Gain', '0.098', ...
        'Position', [110 32 170 62]);
    hS = add_block('simulink/Math Operations/Sum', [sys '/Somme'], 'Inputs', '++-', ...
        'Position', [230 60 260 110]);
    hTau = add_block('simulink/Math Operations/Gain', [sys '/1_sur_tau'], 'Gain', '1/3530', ...
        'Position', [300 70 360 100]);
    hI = add_block('simulink/Continuous/Integrator', [sys '/Integrateur'], ...
        'InitialCondition', 'Tsec0', 'Position', [400 70 430 100]);
    hO = add_block('simulink/Sinks/Out1', [sys '/T_sec'], 'Position', [480 78 510 92]);
    lier(sys, hP, 1, hK, 1);
    lier(sys, hK, 1, hS, 1);
    lier(sys, hTa, 1, hS, 2);
    lier(sys, hI, 1, hS, 3);
    lier(sys, hS, 1, hTau, 1);
    lier(sys, hTau, 1, hI, 1);
    lier(sys, hI, 1, hO, 1);
end

function lier(sys, hSrc, pSrc, hDst, pDst)
    phS = get_param(hSrc, 'PortHandles');
    phD = get_param(hDst, 'PortHandles');
    add_line(sys, phS.Outport(pSrc), phD.Inport(pDst), 'autorouting', 'on');
end

function h = fromWs(sys, nom, position, TE)
    h = add_block('simulink/Sources/From Workspace', [sys '/sc_' nom], ...
        'Position', position, 'VariableName', ['sc_' nom], 'SampleTime', num2str(TE), ...
        'Interpolate', 'off', 'OutputAfterFinalValue', 'Holding final value');
end

function journal(sys, hSrc, variable, position)
    h = add_block('simulink/Sinks/To Workspace', [sys '/' variable], ...
        'VariableName', variable, 'SaveFormat', 'Timeseries', 'Position', position);
    lier(sys, hSrc, 1, h, 1);
end

function ecrireFonction(chemin, code)
    emc = find(sfroot, '-isa', 'Stateflow.EMChart', 'Path', chemin);
    emc.Script = code;
end

function afficherCauses(e, retrait)
    % Détail d'une erreur Simulink à causes multiples (contenu du Diagnostic Viewer).
    for k = 1:numel(e.cause)
        c = e.cause{k};
        fprintf('%s- %s\n', retrait, c.message);
        afficherCauses(c, [retrait '  ']);
    end
end

function c = codeConversion()
    c = sprintf([ ...
        'function P = conversion_puissance(V_Fl_1, V_Fl_2, V_Fl_3)\n' ...
        '%% Puissance de gaz selon les electrovannes de rampe (paliers du firmware).\n' ...
        'Pnom = 5000;   %% W\n' ...
        'if V_Fl_1 == 1 && V_Fl_2 == 1 && V_Fl_3 == 1\n' ...
        '    P = Pnom;\n' ...
        'elseif V_Fl_2 == 1 && V_Fl_3 == 1\n' ...
        '    P = 0.67 * Pnom;\n' ...
        'elseif V_Fl_3 == 1\n' ...
        '    P = 0.33 * Pnom;\n' ...
        'else\n' ...
        '    P = 0;\n' ...
        'end\n']);
end

function c = codeSechage()
    c = sprintf([ ...
        'function dH = sechage(T_sec, H)\n' ...
        '%% Modele de sechage ILLUSTRATIF (a remplacer par un modele identifie) :\n' ...
        '%% l''humidite de l''air extrait tend vers H_EQ, d''autant plus vite que\n' ...
        '%% la chambre est chaude au-dessus de T_DEBUT.\n' ...
        'T_DEBUT = 35;      %% degC\n' ...
        'K = 1.14e-5;       %% 1/(s.degC)\n' ...
        'H_EQ = 20;         %% %%HR\n' ...
        'dH = -K * max(T_sec - T_DEBUT, 0) * (H - H_EQ);\n']);
end

function c = codeBruleur()
    c = sprintf([ ...
        'function Flame = bruleur(V_H2, V_But, panne, parasite)\n' ...
        '%% Capteur de flamme : flamme si une vanne de gaz est ouverte (au pas\n' ...
        '%% precedent), sauf panne simulee ; ou flamme parasite simulee.\n' ...
        'gaz = (V_H2 ~= 0) || (V_But ~= 0);\n' ...
        'Flame = double((gaz && panne == 0) || parasite ~= 0);\n']);
end
