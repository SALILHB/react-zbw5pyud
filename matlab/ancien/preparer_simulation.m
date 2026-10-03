%% preparer_simulation.m  —  construit le modèle de simulation en boucle fermée
%
%   preparer_simulation()
%   preparer_simulation(source, cible)
%
% Part du modèle fourni (par défaut Commande_Sechoir_Hybride_corrige.slx),
% l'enregistre sous un NOUVEAU nom (par défaut Simulation_Sechoir_Hybride.slx :
% le modèle d'origine n'est jamais modifié), puis :
%   1. complète le chart FSM avec completer_fsm_sechoir (logique du firmware v3) ;
%   2. câble TOUTES les entrées du chart. Dans le modèle fourni, les ports du
%      sous-système FSM ne sont reliés ni au chart ni aux sorties : sans ce
%      câblage, le chart ne reçoit rien et aucune vanne ne s'ouvre ;
%   3. ajoute les blocs de scénario (From Workspace, variables sc_<Nom>), le
%      modèle de brûleur (flamme), le modèle de séchage (H_sec), l'apport
%      solaire, un Scope de suivi et l'enregistrement des signaux ;
%   4. règle le solveur (pas fixe 0,1 s) et compile le modèle pour vérifier.
%
% Si l'étape 1 signale [HIERARCHIE INCORRECTE] : corriger à la main dans
% Simulation_Sechoir_Hybride (voir docs/FSM_SIMULINK.md §1), enregistrer, puis
%   preparer_simulation('Simulation_Sechoir_Hybride', 'Simulation_Sechoir_Hybride')
% (source = cible : le modèle est repris tel quel, sans recopie).
%
% NON EXÉCUTÉ DANS UNE VRAIE SESSION MATLAB (pas de MATLAB dans
% l'environnement de développement) : chaque étape affiche [ok] ou l'erreur
% exacte. Copiez-moi la sortie complète en cas de problème.

function preparer_simulation(source, cible)

    if nargin < 1
        source = 'Commande_Sechoir_Hybride_corrige';
    end
    if nargin < 2
        cible = 'Simulation_Sechoir_Hybride';
    end
    dossier = fileparts(mfilename('fullpath'));
    TE = 0.1;   % pas de calcul (s) : réactivité de la sécurité flamme

    fprintf('=== Preparation du modele de simulation : %s ===\n', cible);

    % --- 0. Copie du modèle source sous le nouveau nom
    if strcmp(source, cible)
        if ~bdIsLoaded(cible)
            load_system(fullfile(dossier, [cible '.slx']));
        end
    else
        if bdIsLoaded(cible)
            close_system(cible, 0);
        end
        if bdIsLoaded(source)
            close_system(source, 0);   % repartir du fichier, pas d'une version modifiée en mémoire
        end
        load_system(fullfile(dossier, [source '.slx']));
        save_system(source, fullfile(dossier, [cible '.slx']));
        fprintf('  [ok] %s.slx copie en %s.slx\n', source, cible);
    end

    % --- 1. Logique du chart
    if ~completer_fsm_sechoir(cible)
        save_system(cible);
        fprintf(['\n*** Chart incomplet : corrigez la hierarchie dans %s, enregistrez,\n' ...
                 '*** puis lancez preparer_simulation(''%s'', ''%s'').\n'], cible, cible, cible);
        return;
    end

    fsm = [cible '/FSM'];
    chartPath = [fsm '/Chart'];
    chart = find(sfroot, '-isa', 'Stateflow.Chart', 'Path', chartPath);
    try
        chart.ChartUpdate = 'DISCRETE';
        chart.SampleTime = num2str(TE);
        fprintf('  [ok] chart execute toutes les %g s\n', TE);
    catch e
        fprintf('  [avertissement] periode du chart non reglee : %s\n', e.message);
    end

    % --- 2. Nettoyage : lignes et constantes de la racine, ports du sous-système FSM
    supprimerLignes(cible);
    supprimerBlocs(cible, 'Constant');
    supprimerLignes(fsm);
    supprimerBlocs(fsm, 'Inport');
    supprimerBlocs(fsm, 'Outport');
    fprintf('  [ok] anciens ports et constantes supprimes\n');

    % --- 3. Câblage du chart dans le sous-système FSM
    sc = scenario_sechoir(1);
    appliquer_scenario(cible, sc);   % variables sc_* nécessaires à la compilation

    externes = {'T_sec', 'H_sec', 'Flame', 'T_amb'};   % viennent du modèle physique
    hChart = get_param(chartPath, 'Handle');
    entrees = chart.find('-isa', 'Stateflow.Data', 'Scope', 'Input');
    sorties = chart.find('-isa', 'Stateflow.Data', 'Scope', 'Output');
    nMax = max(numel(entrees), numel(sorties));
    set_param(chartPath, 'Position', [420 20 620 40 + 40 * nMax]);

    for i = 1:numel(entrees)
        d = entrees(i);
        y = 20 + 40 * (d.Port - 1);
        k = find(strcmp(d.Name, externes));
        if ~isempty(k)
            hSrc = add_block('simulink/Sources/In1', [fsm '/' d.Name], ...
                'Position', [40 y 70 y + 14]);
        else
            if ~isfield(sc.signaux, d.Name)
                fprintf('  [ATTENTION] entree %s absente de scenario_sechoir : ajoutez-la\n', d.Name);
            end
            hSrc = add_block('simulink/Sources/From Workspace', [fsm '/sc_' d.Name], ...
                'MakeNameUnique', 'on', 'Position', [20 y - 5 150 y + 20], ...
                'VariableName', ['sc_' d.Name], 'SampleTime', num2str(TE), ...
                'Interpolate', 'off', 'OutputAfterFinalValue', 'Holding final value');
        end
        if strcmp(d.DataType, 'uint8')
            hConv = add_block('simulink/Signal Attributes/Data Type Conversion', ...
                [fsm '/uint8_' d.Name], 'MakeNameUnique', 'on', ...
                'Position', [230 y 290 y + 20], 'OutDataTypeStr', 'uint8');
            lier(fsm, hSrc, 1, hConv, 1);
            hSrc = hConv;
        end
        lier(fsm, hSrc, 1, hChart, d.Port);
    end
    for k = 1:numel(externes)   % numéros de ports fixés une fois tous créés
        set_param([fsm '/' externes{k}], 'Port', num2str(k));
    end
    fprintf('  [ok] %d entrees du chart cablees (%s via ports, le reste via From Workspace)\n', ...
        numel(entrees), strjoin(externes, ', '));

    ordreSorties = {'V_Fl_1', 'V_Fl_2', 'V_Fl_3', 'V_H2', 'V_But', 'Spark', ...
                    'PWM_Inj', 'PWM_Ext', 'PWM_Purge', 'Etat_LCD', 'Buzzer'};
    for i = 1:numel(sorties)
        d = sorties(i);
        if ~any(strcmp(d.Name, ordreSorties))
            ordreSorties{end + 1} = d.Name; %#ok<AGROW>
        end
        y = 20 + 40 * (d.Port - 1);
        hOut = add_block('simulink/Sinks/Out1', [fsm '/' d.Name], ...
            'Position', [760 y 790 y + 14]);
        lier(fsm, hChart, d.Port, hOut, 1);
    end
    for k = 1:numel(ordreSorties)
        set_param([fsm '/' ordreSorties{k}], 'Port', num2str(k));
    end
    fprintf('  [ok] %d sorties du chart reliees aux ports du sous-systeme FSM\n', numel(sorties));

    % --- 4. Racine : modèle physique en boucle fermée
    hFSM = get_param(fsm, 'Handle');
    hConvP = get_param([cible '/CONVERSION_PUISSANCE'], 'Handle');
    hTherm = get_param([cible '/MODELE_THERMIQUE'], 'Handle');
    hAff = get_param([cible '/AFFICHAGE'], 'Handle');
    hDisp = get_param([cible '/Display'], 'Handle');
    set_param(hFSM, 'Position', [420 60 620 560]);

    hTamb = fromWs(cible, 'T_amb', [40 580 170 605], TE);
    hPsol = fromWs(cible, 'P_sol', [40 660 170 685], TE);
    hPanne = fromWs(cible, 'Flamme_panne', [40 740 170 765], TE);
    hParas = fromWs(cible, 'Flamme_parasite', [40 800 170 825], TE);

    % Puissance : gaz (CONVERSION_PUISSANCE) + solaire
    for k = 1:3
        lier(cible, hFSM, k, hConvP, k);
    end
    hSomme = add_block('simulink/Math Operations/Sum', [cible '/Puissance_totale'], ...
        'MakeNameUnique', 'on', 'Inputs', '++', 'Position', [900 90 930 120]);
    lier(cible, hConvP, 1, hSomme, 1);
    lier(cible, hPsol, 1, hSomme, 2);
    lier(cible, hConvP, 1, hDisp, 1);
    lier(cible, hSomme, 1, hTherm, 1);
    lier(cible, hTamb, 1, hTherm, 2);

    % T_sec rebouclée
    lier(cible, hTherm, 1, hFSM, 1);
    lier(cible, hTherm, 1, hAff, 1);

    % Séchage : dH/dt = f(T_sec, H), intégré -> H_sec
    hSech = add_block('simulink/User-Defined Functions/MATLAB Function', ...
        [cible '/MODELE_SECHAGE'], 'MakeNameUnique', 'on', 'Position', [900 300 1010 350]);
    ecrireFonction(getfullname(hSech), codeSechage());
    hIntH = add_block('simulink/Continuous/Integrator', [cible '/H_sec'], ...
        'MakeNameUnique', 'on', 'InitialCondition', 'sc_H0', 'Position', [1060 305 1090 345]);
    lier(cible, hTherm, 1, hSech, 1);
    lier(cible, hSech, 1, hIntH, 1);
    lier(cible, hIntH, 1, hSech, 2);
    lier(cible, hIntH, 1, hFSM, 2);

    % Brûleur : flamme présente si le gaz est ouvert (retard d'un pas), sauf panne
    hBru = add_block('simulink/User-Defined Functions/MATLAB Function', ...
        [cible '/BRULEUR'], 'MakeNameUnique', 'on', 'Position', [240 620 340 720]);
    ecrireFonction(getfullname(hBru), codeBruleur());
    hRetH2 = add_block('simulink/Discrete/Unit Delay', [cible '/Retard_V_H2'], ...
        'MakeNameUnique', 'on', 'SampleTime', num2str(TE), 'Position', [700 620 730 650]);
    hRetBut = add_block('simulink/Discrete/Unit Delay', [cible '/Retard_V_But'], ...
        'MakeNameUnique', 'on', 'SampleTime', num2str(TE), 'Position', [700 680 730 710]);
    lier(cible, hFSM, 4, hRetH2, 1);
    lier(cible, hFSM, 5, hRetBut, 1);
    lier(cible, hRetH2, 1, hBru, 1);
    lier(cible, hRetBut, 1, hBru, 2);
    lier(cible, hPanne, 1, hBru, 3);
    lier(cible, hParas, 1, hBru, 4);
    lier(cible, hBru, 1, hFSM, 3);
    lier(cible, hTamb, 1, hFSM, 4);

    % Enregistrement (variables de sortie de sim) et Scope de suivi
    hMux = add_block('simulink/Signal Routing/Mux', [cible '/Sorties_FSM'], ...
        'MakeNameUnique', 'on', 'Inputs', num2str(numel(ordreSorties)), 'Position', [1100 420 1105 700]);
    for k = 1:numel(ordreSorties)
        lier(cible, hFSM, k, hMux, k);
    end
    journal(cible, hMux, 'log_fsm', [1160 545 1250 575]);
    journal(cible, hTherm, 'log_T_sec', [1160 40 1250 70]);
    journal(cible, hIntH, 'log_H_sec', [1160 300 1250 330]);
    journal(cible, hConvP, 'log_P_gaz', [1160 150 1250 180]);
    journal(cible, hBru, 'log_Flame', [1160 620 1250 650]);

    hScope = add_block('simulink/Sinks/Scope', [cible '/SUIVI'], 'MakeNameUnique', 'on', ...
        'NumInputPorts', '3', 'Position', [1300 200 1340 280]);
    lier(cible, hTherm, 1, hScope, 1);
    lier(cible, hIntH, 1, hScope, 2);
    lier(cible, hFSM, find(strcmp(ordreSorties, 'Etat_LCD')), hScope, 3);
    fprintf('  [ok] modele physique, bruleur, sechage, enregistrement et Scope ajoutes\n');

    % --- 5. Solveur et mise en page
    set_param(cible, 'SolverType', 'Fixed-step', 'Solver', 'ode4', ...
        'FixedStep', num2str(TE), 'StopTime', num2str(sc.StopTime), ...
        'ReturnWorkspaceOutputs', 'on');
    fprintf('  [ok] solveur ode4, pas fixe %g s\n', TE);
    try
        Simulink.BlockDiagram.arrangeSystem(fsm);
        Simulink.BlockDiagram.arrangeSystem(cible);
    catch
        % mise en page automatique indisponible : sans conséquence
    end

    % --- 6. Compilation de contrôle
    try
        set_param(cible, 'SimulationCommand', 'update');
        fprintf('  [ok] le modele compile sans erreur\n');
    catch e
        fprintf('  [ERREUR] compilation : %s\n', e.message);
        afficherCauses(e, '    ');
        fprintf('  (copiez-moi toute cette liste)\n');
    end
    save_system(cible);
    fprintf('=== %s.slx enregistre. Etape suivante : lancer_simulation(1) ===\n', cible);
end

%% ---------------------------------------------------------------------
function afficherCauses(e, retrait)
    % Détail d'une erreur Simulink à causes multiples (contenu du Diagnostic Viewer).
    for k = 1:numel(e.cause)
        c = e.cause{k};
        fprintf('%s- %s\n', retrait, c.message);
        afficherCauses(c, [retrait '  ']);
    end
end

function supprimerLignes(sys)
    lignes = find_system(sys, 'SearchDepth', 1, 'FindAll', 'on', 'Type', 'line');
    for i = 1:numel(lignes)
        try
            delete_line(lignes(i));
        catch
            % ligne déjà supprimée avec sa ligne mère (branche)
        end
    end
end

function supprimerBlocs(sys, type)
    blocs = find_system(sys, 'SearchDepth', 1, 'BlockType', type);
    for i = 1:numel(blocs)
        delete_block(blocs{i});
    end
end

function lier(sys, hSrc, pSrc, hDst, pDst)
    phS = get_param(hSrc, 'PortHandles');
    phD = get_param(hDst, 'PortHandles');
    add_line(sys, phS.Outport(pSrc), phD.Inport(pDst), 'autorouting', 'on');
end

function h = fromWs(sys, nom, position, TE)
    h = add_block('simulink/Sources/From Workspace', [sys '/sc_' nom], 'MakeNameUnique', 'on', ...
        'Position', position, 'VariableName', ['sc_' nom], 'SampleTime', num2str(TE), ...
        'Interpolate', 'off', 'OutputAfterFinalValue', 'Holding final value');
end

function journal(sys, hSrc, variable, position)
    h = add_block('simulink/Sinks/To Workspace', [sys '/' variable], 'MakeNameUnique', 'on', ...
        'VariableName', variable, 'SaveFormat', 'Timeseries', 'Position', position);
    lier(sys, hSrc, 1, h, 1);
end

function ecrireFonction(chemin, code)
    emc = find(sfroot, '-isa', 'Stateflow.EMChart', 'Path', chemin);
    emc.Script = code;
end

function c = codeSechage()
    c = sprintf([ ...
        'function dH = sechage(T_sec, H)\n' ...
        '%% Modele de sechage ILLUSTRATIF (a remplacer par un modele identifie) :\n' ...
        '%% l''humidite de l''air extrait tend vers H_EQ, d''autant plus vite que\n' ...
        '%% la chambre est chaude au-dessus de T_DEBUT.\n' ...
        'T_DEBUT = 35;      %% degC\n' ...
        'K = 1.14e-5;       %% 1/(s.degC)\n' ...
        'H_EQ = 20;         %%  %%HR\n' ...
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
