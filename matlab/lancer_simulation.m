%% lancer_simulation.m  —  simule un ou plusieurs scénarios et exporte les courbes
%
%   lancer_simulation(1)          scénario 1
%   lancer_simulation([1 3 8])    plusieurs scénarios
%   lancer_simulation(1:12)       tous
%   r = lancer_simulation(5)      renvoie aussi les signaux
%
% Pré-requis : construire_modele() exécuté une fois (crée
% Simulation_Sechoir_Hybride.slx). Pour chaque scénario :
%   - affiche le journal des états et de la puissance (à me copier en cas de doute) ;
%   - enregistre la figure  captures/scenario_NN.png  (300 dpi, pour le mémoire)
%     et les signaux        captures/scenario_NN.mat.

function res = lancer_simulation(numeros, modele)

    if nargin < 1
        numeros = 1;
    end
    if nargin < 2
        modele = 'Simulation_Sechoir_Hybride';
    end
    dossier = fileparts(mfilename('fullpath'));
    dossierCaptures = fullfile(dossier, 'captures');
    if ~exist(dossierCaptures, 'dir')
        mkdir(dossierCaptures);
    end
    if ~bdIsLoaded(modele)
        load_system(fullfile(dossier, [modele '.slx']));
    end

    res = [];
    for n = numeros
        sc = scenario_sechoir(n);
        fprintf('\n=== Scenario %d : %s ===\n', n, sc.nom);
        fprintf('Attendu : %s\n', sc.attendu);
        appliquer_scenario(modele, sc);

        tic;
        out = sim(modele, 'StopTime', num2str(sc.StopTime), 'ReturnWorkspaceOutputs', 'on');
        fprintf('Simulation de %g s terminee en %.1f s de calcul.\n', sc.StopTime, toc);

        r = extraire(out);
        r.sc = sc;
        journal_scenario(r);
        fig = tracer_scenario(r);
        exporter_figure(fig, fullfile(dossierCaptures, sprintf('scenario_%02d.png', n)));
        save(fullfile(dossierCaptures, sprintf('scenario_%02d.mat', n)), 'r');
        fprintf('Figure : captures/scenario_%02d.png\n', n);
        res = [res, r]; %#ok<AGROW>
    end
end

%% ---------------------------------------------------------------------
function r = extraire(out)
    ts = out.get('log_T_sec');
    r.t = ts.Time(:);
    r.T_sec = aligner(ts, r.t);
    r.H_sec = aligner(out.get('log_H_sec'), r.t);
    r.P_gaz = aligner(out.get('log_P_gaz'), r.t);
    r.Flame = aligner(out.get('log_Flame'), r.t);

    noms = {'V_Fl_1', 'V_Fl_2', 'V_Fl_3', 'V_H2', 'V_But', 'Spark', ...
            'PWM_Inj', 'PWM_Ext', 'PWM_Purge', 'Etat_LCD', 'Buzzer'};
    D = aligner(out.get('log_fsm'), r.t);
    for k = 1:numel(noms)
        r.fsm.(noms{k}) = D(:, k);
    end
end

function v = aligner(ts, t)
    % Données d'un timeseries en colonnes (N x m), ramenées sur la base de temps t.
    D = double(ts.Data);
    if ndims(D) == 3
        D = permute(D, [3 1 2]);
        D = reshape(D, size(D, 1), []);
    elseif size(D, 1) ~= numel(ts.Time)
        D = D.';
    end
    if numel(ts.Time) == numel(t) && all(ts.Time(:) == t)
        v = D;
    else
        v = interp1(ts.Time(:), D, t, 'previous', 'extrap');
    end
end
