%% construire_modele_etudes.m  —  copie du modèle validé, complétée pour les études E1 à E4
%
%   construire_modele_etudes
%   construire_modele_etudes(source)     (défaut : 'Simulation_Sechoir_Hybride')
%
% Enregistre Etudes_Sechoir.slx (dossier matlab/) à partir d'une COPIE de
% Simulation_Sechoir_Hybride.slx, qui n'est pas modifié. Le chart n'est pas
% touché. Blocs ajoutés ou modifiés dans la copie :
%
%   MODELE_THERMIQUE    C_eq dT/dt = P + UA f(t) (T_amb - T)
%                       gains et_UA et 1/et_Ceq ; f(t) = sc_UA_facteur (porte)
%   MESURE_T_SEC        T_mes = T(t - et_retard) + sc_Bruit_T, quantifiée à
%                       0,0625 °C si et_quantif = 1 ; c'est T_mes que lit le chart
%   DISPONIBILITE_GAZ   puissance nulle si seule V_But est ouverte et que le GPL
%                       manque (sc_GPL_dispo = 0) ; BRULEUR : pas de flamme non plus
%   STOCK_H2            m_H2 = intégrale de V_H2 P / (et_eta et_PCI_H2) ;
%                       Press_H2 = sc_Press_H2 x max(0, 1 - m_H2 / et_m0)
%                       (et_m0 = 0 : pression du scénario, stock illimité)
%   log_T_mes, log_P_sol, log_T_amb, log_Press_H2, log_m_H2 (To Workspace)
%
% Avec les réglages par défaut (completer_etude), la copie se comporte
% exactement comme le modèle validé : le script le vérifie à la fin sur les
% scénarios 1, 3 et 8 (instants des changements de palier comparés).

function construire_modele_etudes(source)

    if nargin < 1
        source = 'Simulation_Sechoir_Hybride';
    end
    cible = 'Etudes_Sechoir';
    % Dossier matlab/ : celui qui contient le modèle validé, que ce script
    % soit rangé dans matlab/etudes/ (prévu) ou directement dans matlab/.
    ici = fileparts(mfilename('fullpath'));
    dossier = '';
    for candidat = {fileparts(ici), ici, pwd}
        if exist(fullfile(candidat{1}, [source '.slx']), 'file')
            dossier = candidat{1};
            break;
        end
    end
    if isempty(dossier)
        error('construire_modele_etudes:modele', ...
              '%s.slx introuvable (cherche dans %s, %s et %s).', source, fileparts(ici), ici, pwd);
    end
    addpath(dossier);
    if exist(fullfile(dossier, 'etudes'), 'dir')
        addpath(fullfile(dossier, 'etudes'));
    end
    TE = 0.1;

    fprintf('=== Construction de %s (copie de %s) ===\n', cible, source);
    if bdIsLoaded(cible)
        close_system(cible, 0);
    end
    if bdIsLoaded(source)
        close_system(source, 0);   % la copie part du fichier enregistré
    end
    load_system(fullfile(dossier, [source '.slx']));
    fichier = fullfile(dossier, [cible '.slx']);
    save_system(source, fichier);   % le modèle chargé s'appelle désormais Etudes_Sechoir
    nom = cible;
    fsm = [nom '/FSM'];
    appliquer_etude(nom, cas_etude('E4'));   % variables nécessaires à la compilation

    etape('MODELE_THERMIQUE (UA, C_eq, porte)', @() thermique(nom, TE));
    etape('MESURE_T_SEC (retard, bruit, quantification)', @() mesure(nom, fsm, TE));
    etape('DISPONIBILITE_GAZ et BRULEUR (GPL)', @() disponibiliteGaz(nom, fsm, TE));
    etape('STOCK_H2 (pression du reservoir)', @() stockH2(nom, fsm, TE));
    etape('enregistrements supplementaires', @() journaux(nom));

    save_system(nom, fichier);
    try
        set_param(nom, 'SimulationCommand', 'update');
        fprintf('  [ok] %s compile sans erreur\n', cible);
    catch e
        fprintf('  [ERREUR] compilation : %s\n', e.message);
        for k = 1:numel(e.cause)
            fprintf('    - %s\n', e.cause{k}.message);
        end
        fprintf('  (copiez-moi toute cette sortie)\n');
        return;
    end
    save_system(nom, fichier);
    verifier(dossier);
    fprintf('=== %s.slx enregistre ===\n', cible);
end

%% =====================================================================
function etape(nomEtape, action)
    try
        action();
        fprintf('  [ok] %s\n', nomEtape);
    catch e
        fprintf('  [ERREUR] %s : %s\n', nomEtape, e.message);
        rethrow(e);
    end
end

function thermique(nom, TE)
    sys = [nom '/MODELE_THERMIQUE'];
    % Une ligne ramifiée est supprimée avec ses branches : on redemande la
    % liste après chaque suppression (les anciens identifiants deviennent invalides).
    lignes = find_system(sys, 'SearchDepth', 1, 'FindAll', 'on', 'Type', 'line');
    while ~isempty(lignes)
        delete_line(lignes(1));
        lignes = find_system(sys, 'SearchDepth', 1, 'FindAll', 'on', 'Type', 'line');
    end
    delete_block([sys '/Kth']);
    delete_block([sys '/Somme']);
    delete_block([sys '/1_sur_tau']);
    hP = get_param([sys '/P'], 'Handle');
    hTa = get_param([sys '/T_amb'], 'Handle');
    hI = get_param([sys '/Integrateur'], 'Handle');
    hO = get_param([sys '/T_sec'], 'Handle');
    hF = add_block('simulink/Sources/From Workspace', [sys '/sc_UA_facteur'], ...
        'VariableName', 'sc_UA_facteur', 'SampleTime', num2str(TE), 'Interpolate', 'off', ...
        'OutputAfterFinalValue', 'Holding final value', 'Position', [30 170 150 195]);
    hEcart = add_block('simulink/Math Operations/Sum', [sys '/Ecart'], 'Inputs', '+-', ...
        'Position', [110 105 140 135]);
    hProd = add_block('simulink/Math Operations/Product', [sys '/Porte'], 'Inputs', '2', ...
        'Position', [190 110 220 160]);
    hUA = add_block('simulink/Math Operations/Gain', [sys '/UA'], 'Gain', 'et_UA', ...
        'Position', [260 115 310 145]);
    hS = add_block('simulink/Math Operations/Sum', [sys '/Bilan'], 'Inputs', '++', ...
        'Position', [350 60 380 110]);
    hC = add_block('simulink/Math Operations/Gain', [sys '/1_sur_Ceq'], 'Gain', '1/et_Ceq', ...
        'Position', [420 70 480 100]);
    set_param(hI, 'Position', [520 70 550 100]);
    set_param(hO, 'Position', [600 78 630 92]);
    lier(sys, hTa, 1, hEcart, 1);
    lier(sys, hI, 1, hEcart, 2);
    lier(sys, hEcart, 1, hProd, 1);
    lier(sys, hF, 1, hProd, 2);
    lier(sys, hProd, 1, hUA, 1);
    lier(sys, hP, 1, hS, 1);
    lier(sys, hUA, 1, hS, 2);
    lier(sys, hS, 1, hC, 1);
    lier(sys, hC, 1, hI, 1);
    lier(sys, hI, 1, hO, 1);
end

function mesure(nom, fsm, TE)
    delete_line(nom, 'MODELE_THERMIQUE/1', 'FSM/1');
    hTherm = get_param([nom '/MODELE_THERMIQUE'], 'Handle');
    hFSM = get_param(fsm, 'Handle');
    hRet = add_block('simulink/Continuous/Transport Delay', [nom '/RETARD_MESURE'], ...
        'DelayTime', 'et_retard', 'InitialOutput', 'Tsec0', 'BufferSize', '8192', ...
        'Position', [1120 90 1170 120]);
    hBruit = fromWs(nom, 'Bruit_T', [1120 140 1240 165], TE);
    hS = add_block('simulink/Math Operations/Sum', [nom '/T_bruitee'], 'Inputs', '++', ...
        'Position', [1200 90 1220 130]);
    hQ = add_block('simulink/User-Defined Functions/MATLAB Function', [nom '/QUANTIFICATION'], ...
        'Position', [1260 90 1340 140]);
    ecrireFonction([nom '/QUANTIFICATION'], sprintf([ ...
        'function T_mes = quantification(T, q)\n' ...
        '%% Resolution du DS18B20 (0,0625 degC) si q = 1.\n' ...
        'if q ~= 0\n' ...
        '    T_mes = round(T / 0.0625) * 0.0625;\n' ...
        'else\n' ...
        '    T_mes = T;\n' ...
        'end\n']));
    hQc = add_block('simulink/Sources/Constant', [nom '/et_quantif'], 'Value', 'et_quantif', ...
        'Position', [1200 150 1240 170]);
    lier(nom, hTherm, 1, hRet, 1);
    lier(nom, hRet, 1, hS, 1);
    lier(nom, hBruit, 1, hS, 2);
    lier(nom, hS, 1, hQ, 1);
    lier(nom, hQc, 1, hQ, 2);
    lier(nom, hQ, 1, hFSM, 1);
    journal(nom, hQ, 'log_T_mes', [1380 100 1470 130]);
end

function disponibiliteGaz(nom, fsm, TE)
    port = portsSortie(fsm);
    hFSM = get_param(fsm, 'Handle');
    hConv = get_param([nom '/CONVERSION_PUISSANCE'], 'Handle');
    hSomme = get_param([nom '/Puissance_totale'], 'Handle');
    hLog = get_param([nom '/log_P_gaz'], 'Handle');
    delete_line(nom, 'CONVERSION_PUISSANCE/1', 'Puissance_totale/1');
    delete_line(nom, 'CONVERSION_PUISSANCE/1', 'log_P_gaz/1');

    hGpl = fromWs(nom, 'GPL_dispo', [40 880 170 905], TE);
    hD = add_block('simulink/User-Defined Functions/MATLAB Function', [nom '/DISPONIBILITE_GAZ'], ...
        'Position', [830 40 900 120]);
    ecrireFonction([nom '/DISPONIBILITE_GAZ'], sprintf([ ...
        'function P_eff = disponibilite_gaz(P, V_H2, V_But, gpl)\n' ...
        '%% Bouteille de GPL vide : aucune puissance quand seule V_But est ouverte.\n' ...
        'P_eff = double(P);\n' ...
        'if V_But ~= 0 && V_H2 == 0 && gpl == 0\n' ...
        '    P_eff = 0;\n' ...
        'end\n']));
    lier(nom, hConv, 1, hD, 1);
    lier(nom, hFSM, port('V_H2'), hD, 2);
    lier(nom, hFSM, port('V_But'), hD, 3);
    lier(nom, hGpl, 1, hD, 4);
    lier(nom, hD, 1, hSomme, 1);
    lier(nom, hD, 1, hLog, 1);

    % BRULEUR : 5e entrée, pas de flamme sur V_But sans GPL
    ecrireFonction([nom '/BRULEUR'], sprintf([ ...
        'function Flame = bruleur(V_H2, V_But, panne, parasite, gpl)\n' ...
        '%% Capteur de flamme : flamme si une vanne de gaz est ouverte (au pas\n' ...
        '%% precedent) et que le gaz arrive, sauf panne simulee ; ou flamme parasite.\n' ...
        'gaz = (V_H2 ~= 0) || (V_But ~= 0 && gpl ~= 0);\n' ...
        'Flame = double((gaz && panne == 0) || parasite ~= 0);\n']));
    lier(nom, hGpl, 1, get_param([nom '/BRULEUR'], 'Handle'), 5);
end

function stockH2(nom, fsm, TE)
    % Entrée Press_H2 du chart : From Workspace remplacé par un port du sous-système.
    blocPress = [fsm '/sc_Press_H2'];
    ph = get_param(blocPress, 'PortHandles');
    delete_line(get_param(ph.Outport(1), 'Line'));
    pos = get_param(blocPress, 'Position');
    delete_block(blocPress);
    chart = find(sfroot, '-isa', 'Stateflow.Chart', 'Path', [fsm '/Chart']);
    d = chart.find('-isa', 'Stateflow.Data', 'Name', 'Press_H2');
    hIn = add_block('simulink/Sources/In1', [fsm '/Press_H2_stock'], 'Position', pos);
    lier(fsm, hIn, 1, get_param([fsm '/Chart'], 'Handle'), d(1).Port);
    numeroPort = str2double(get_param(hIn, 'Port'));

    port = portsSortie(fsm);
    hFSM = get_param(fsm, 'Handle');
    hPsc = fromWs(nom, 'Press_H2', [40 960 170 985], TE);
    hConv = add_block('simulink/Signal Attributes/Data Type Conversion', [nom '/V_H2_double'], ...
        'OutDataTypeStr', 'double', 'Position', [240 1030 290 1050]);
    hProd = add_block('simulink/Math Operations/Product', [nom '/Debit_H2'], 'Inputs', '2', ...
        'Position', [330 1020 360 1070]);
    hG = add_block('simulink/Math Operations/Gain', [nom '/1_sur_eta_PCI'], ...
        'Gain', '1/(et_eta*et_PCI_H2*3600)', 'Position', [400 1030 470 1060]);   % W -> g/s
    hM = add_block('simulink/Continuous/Integrator', [nom '/m_H2'], 'InitialCondition', '0', ...
        'Position', [510 1030 540 1060]);
    hPr = add_block('simulink/User-Defined Functions/MATLAB Function', [nom '/PRESSION_H2'], ...
        'Position', [580 960 660 1030]);
    ecrireFonction([nom '/PRESSION_H2'], sprintf([ ...
        'function p = pression_h2(p_sc, m, m0)\n' ...
        '%% Stock limite : la pression baisse avec la masse consommee (m0 = 0 : illimite).\n' ...
        'if m0 > 0\n' ...
        '    p = p_sc * max(0, 1 - m / m0);\n' ...
        'else\n' ...
        '    p = p_sc;\n' ...
        'end\n']));
    hM0 = add_block('simulink/Sources/Constant', [nom '/et_m0'], 'Value', 'et_m0', ...
        'Position', [500 1080 540 1100]);
    lier(nom, hFSM, port('V_H2'), hConv, 1);
    lier(nom, hConv, 1, hProd, 1);
    lier(nom, get_param([nom '/DISPONIBILITE_GAZ'], 'Handle'), 1, hProd, 2);
    lier(nom, hProd, 1, hG, 1);
    lier(nom, hG, 1, hM, 1);
    lier(nom, hPsc, 1, hPr, 1);
    lier(nom, hM, 1, hPr, 2);
    lier(nom, hM0, 1, hPr, 3);
    lier(nom, hPr, 1, hFSM, numeroPort);
    journal(nom, hPr, 'log_Press_H2', [720 960 810 990]);
    journal(nom, hM, 'log_m_H2', [720 1030 810 1060]);
end

function journaux(nom)
    journal(nom, get_param([nom '/sc_P_sol'], 'Handle'), 'log_P_sol', [880 240 970 270]);
    journal(nom, get_param([nom '/sc_T_amb'], 'Handle'), 'log_T_amb', [240 560 330 590]);
end

function verifier(dossier)
    % Réglages par défaut : mêmes changements de palier que la référence.
    fprintf('  Verification de la copie (reglages par defaut) :\n');
    for n = [1 3 8]
        sc = completer_etude(scenario_sechoir(n));
        r = simuler_etude(sc, 'simulink');
        ref = charger_reference(n);
        [ta, pa] = changementsPalier(r.t, r.P_gaz);
        [tb, pb] = changementsPalier(ref.t, ref.P_gaz);
        if numel(ta) == numel(tb) && all(pa == pb)
            fprintf('    scenario %d : %d changements, ecart max %.1f s [ok]\n', n, numel(ta), max(abs(ta - tb)));
        else
            fprintf('    scenario %d : %d changements au lieu de %d [A VERIFIER]\n', n, numel(ta), numel(tb));
        end
    end
    fprintf('  (dossier %s)\n', dossier);
end

function [t, p] = changementsPalier(t, P)
    P = round(P(:));
    k = find(diff(P) ~= 0) + 1;
    t = t(k);
    p = P(k);
end

function port = portsSortie(fsm)
    sorties = find_system(fsm, 'SearchDepth', 1, 'BlockType', 'Outport');
    noms = get_param(sorties, 'Name');
    numeros = str2double(get_param(sorties, 'Port'));
    port = @(n) numeros(strcmp(noms, n));
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
