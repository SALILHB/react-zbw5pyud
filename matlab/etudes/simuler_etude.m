%% simuler_etude.m  —  simule un cas d'étude et renvoie les signaux
%
%   r = simuler_etude(sc)                 moteur par défaut : 'simulink' sous
%                                         MATLAB, 'programme' sous Octave
%   r = simuler_etude(sc, 'simulink')     copie du modèle Etudes_Sechoir.slx
%                                         (construire_modele_etudes)
%   r = simuler_etude(sc, 'programme')    programme Arduino réel compilé sur PC
%                                         + même modèle physique
%                                         (tests/simulation_etudes.cpp, g++)
%
% r a la forme des résultats de lancer_simulation (r.t, r.T_sec, r.P_gaz,
% r.fsm.V_H2...), complétée par r.T_mes, r.P_sol, r.T_amb, r.Press_H2,
% r.m_H2 et r.fsm.Spark, r.fsm.PWM_Ext. Les grandeurs logiques et les
% puissances sont maintenues d'un point au suivant.

function r = simuler_etude(sc, moteur)

    if nargin < 2 || isempty(moteur)
        if exist('OCTAVE_VERSION', 'builtin')
            moteur = 'programme';
        else
            moteur = 'simulink';
        end
    end
    sc = completer_etude(sc);
    switch lower(moteur)
        case 'programme'
            r = parProgramme(sc);
        case 'simulink'
            r = parSimulink(sc);
        otherwise
            error('simuler_etude:moteur', 'Moteur %s inconnu (simulink ou programme).', moteur);
    end
    r.sc = sc;
    r.moteur = lower(moteur);
end

%% ---------------------------------------------------------------------
function r = parProgramme(sc)
    racine = fileparts(fileparts(fileparts(mfilename('fullpath'))));
    dossierTests = fullfile(racine, 'tests');
    exe = fullfile(dossierTests, 'simulation_etudes');
    if ispc
        exe = [exe '.exe'];
    end
    if ~exist(exe, 'file')
        [st, sortie] = system(sprintf('make -C "%s" simulation_etudes', dossierTests));
        if st ~= 0 || ~exist(exe, 'file')
            error('simuler_etude:compilation', ...
                  ['Banc programme introuvable (%s) et compilation impossible :\n%s\n' ...
                   'Utilisez le moteur ''simulink''.'], exe, sortie);
        end
    end
    txt = [tempname '.txt'];
    csv = [tempname '.csv'];
    exporter_etude(sc, txt);
    st = system(sprintf('"%s" "%s" > "%s"', exe, txt, csv));
    if st ~= 0
        error('simuler_etude:programme', 'Echec du banc programme (%s).', txt);
    end
    if exist('OCTAVE_VERSION', 'builtin')
        M = dlmread(csv, ',', 1, 0);
    else
        M = readmatrix(csv, 'NumHeaderLines', 1);
    end
    delete(txt);
    delete(csv);
    % t,T_sec,T_mes,H_sec,P_gaz,P_sol,T_amb,Flame,V_Fl_1,V_Fl_2,V_Fl_3,V_H2,V_But,
    % Etat,Spark,PWM_Ext,Press_H2,m_H2,Source
    r.t = M(:, 1);
    r.T_sec = M(:, 2);
    r.T_mes = M(:, 3);
    r.H_sec = M(:, 4);
    r.P_gaz = M(:, 5);
    r.P_sol = M(:, 6);
    r.T_amb = M(:, 7);
    r.Flame = M(:, 8);
    noms = {'V_Fl_1', 'V_Fl_2', 'V_Fl_3', 'V_H2', 'V_But', 'Etat_LCD', 'Spark', 'PWM_Ext'};
    for k = 1:numel(noms)
        r.fsm.(noms{k}) = M(:, 8 + k);
    end
    r.Press_H2 = M(:, 17);
    r.m_H2 = M(:, 18);
end

%% ---------------------------------------------------------------------
function r = parSimulink(sc)
    modele = 'Etudes_Sechoir';
    dossier = fileparts(fileparts(mfilename('fullpath')));
    if ~bdIsLoaded(modele)
        fichier = fullfile(dossier, [modele '.slx']);
        if ~exist(fichier, 'file')
            error('simuler_etude:modele', ...
                  'Modele %s absent : lancez d''abord construire_modele_etudes.', fichier);
        end
        load_system(fichier);
    end
    appliquer_etude(modele, sc);
    out = sim(modele, 'StopTime', num2str(sc.StopTime), 'ReturnWorkspaceOutputs', 'on');

    ts = out.get('log_T_sec');
    t = ts.Time(:);
    r.t = t;
    r.T_sec = aligner(ts, t);
    r.T_mes = aligner(out.get('log_T_mes'), t);
    r.H_sec = aligner(out.get('log_H_sec'), t);
    r.P_gaz = aligner(out.get('log_P_gaz'), t);
    r.P_sol = aligner(out.get('log_P_sol'), t);
    r.T_amb = aligner(out.get('log_T_amb'), t);
    r.Flame = aligner(out.get('log_Flame'), t);
    r.Press_H2 = aligner(out.get('log_Press_H2'), t);
    r.m_H2 = aligner(out.get('log_m_H2'), t);
    noms = {'V_Fl_1', 'V_Fl_2', 'V_Fl_3', 'V_H2', 'V_But', 'Spark', ...
            'PWM_Inj', 'PWM_Ext', 'PWM_Purge', 'Etat_LCD', 'Buzzer'};
    D = aligner(out.get('log_fsm'), t);
    for k = 1:numel(noms)
        r.fsm.(noms{k}) = D(:, k);
    end
    r = reduire(r, 1.0);
end

function v = aligner(ts, t)
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

function r = reduire(r, pas)
    % Un point par pas (s) et tous les instants où une grandeur maintenue
    % change : les intégrales d'énergie restent exactes.
    n = numel(r.t);
    k = max(1, round(pas / median(diff(r.t))));
    L = [r.P_gaz, r.P_sol, r.T_amb, r.Flame, r.fsm.Etat_LCD, r.fsm.V_H2, r.fsm.V_But, ...
         r.fsm.Spark, r.fsm.PWM_Ext];
    ch = find(any(diff(L) ~= 0, 2)) + 1;
    garder = unique([1:k:n, ch(:)', n]);
    champs = {'t', 'T_sec', 'T_mes', 'H_sec', 'P_gaz', 'P_sol', 'T_amb', 'Flame', 'Press_H2', 'm_H2'};
    for i = 1:numel(champs)
        r.(champs{i}) = r.(champs{i})(garder);
    end
    noms = fieldnames(r.fsm);
    for i = 1:numel(noms)
        r.fsm.(noms{i}) = r.fsm.(noms{i})(garder);
    end
end
