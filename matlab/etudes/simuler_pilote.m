%% simuler_pilote.m  —  conduite sans le chart (étude E2), même modèle thermique
%
%   r = simuler_pilote(mode, sc)
%
% mode :
%   'M2a'  opérateur : relevé de T_sec toutes les 15 min ; < 50 °C -> 100 % ;
%          > 60 °C -> coupé ; entre les deux -> inchangé
%   'M2b'  idem, relevé toutes les 30 min
%   'M3'   thermostat tout-ou-rien 100 % / 0 %, 55 +/- 0,5 °C
%   'M4'   33 % en continu à partir de 130 s, sans arrêt automatique
% Chaque (r)allumage est précédé d'une purge de 120 s (gaz fermé). Démarrage
% (START) à 10 s, comme le scénario 8 : premier allumage à 130 s.
% sc : conditions (cas_etude('M1') : T_amb = 25 °C, T_sec initiale 25 °C).
% Pas de calcul 0,1 s, Euler explicite : C_eq dT/dt = P + UA (T_amb - T).
% r a la forme des résultats de simuler_etude (Etat_LCD = 3, V_H2 = gaz ouvert,
% Spark = 1 pendant les 4 s qui suivent chaque allumage).

function r = simuler_pilote(mode, sc)

    PNOM = 5000;
    dt = 0.1;
    tStart = 10;
    tPurge = 120;
    t = (0:dt:sc.StopTime)';
    n = numel(t);
    T = zeros(n, 1);
    P = zeros(n, 1);
    spark = zeros(n, 1);
    T(1) = sc.Tsec0;
    Tamb = sc.signaux.T_amb(1, 2);
    UA = sc.etude.UA;
    Ceq = sc.etude.Ceq;

    etat = 'arret';       % arret | purge | marche
    finPurge = Inf;
    palier = 1;
    prochainReleve = tStart;
    switch mode
        case 'M2a'
            periode = 900;
        case 'M2b'
            periode = 1800;
        case {'M3', 'M4'}
            periode = 0;
        otherwise
            error('simuler_pilote:mode', 'Mode %s inconnu (M2a, M2b, M3, M4).', mode);
    end
    if strcmp(mode, 'M4')
        palier = 0.33;
    end
    dernierAllumage = -Inf;

    for k = 1:n - 1
        tk = t(k);
        Tk = T(k);
        % --- décision de conduite
        if tk >= tStart
            switch mode
                case {'M2a', 'M2b'}
                    if tk >= prochainReleve - 1e-9
                        prochainReleve = prochainReleve + periode;
                        if Tk < 50 && strcmp(etat, 'arret')
                            etat = 'purge';
                            finPurge = tk + tPurge;
                        elseif Tk > 60
                            etat = 'arret';
                        end
                    end
                case 'M3'
                    if Tk < 54.5 && strcmp(etat, 'arret')
                        etat = 'purge';
                        finPurge = tk + tPurge;
                    elseif Tk > 55.5 && strcmp(etat, 'marche')
                        etat = 'arret';
                    end
                case 'M4'
                    if strcmp(etat, 'arret') && isinf(dernierAllumage)
                        etat = 'purge';
                        finPurge = tk + tPurge;
                    end
            end
            if strcmp(etat, 'purge') && tk >= finPurge - 1e-9
                etat = 'marche';
                dernierAllumage = tk;
            end
        end
        if strcmp(etat, 'marche')
            P(k) = palier * PNOM;
        end
        spark(k) = tk - dernierAllumage < 4;
        % --- modèle thermique
        T(k + 1) = Tk + dt * (P(k) + UA * (Tamb - Tk)) / Ceq;
    end
    P(n) = P(n - 1);

    r.t = t;
    r.T_sec = T;
    r.T_mes = T;
    r.H_sec = NaN(n, 1);
    r.P_gaz = P;
    r.P_sol = zeros(n, 1);
    r.T_amb = Tamb * ones(n, 1);
    r.Flame = double(P > 0);
    r.fsm.V_H2 = double(P > 0);
    r.fsm.V_But = zeros(n, 1);
    r.fsm.Etat_LCD = 3 * ones(n, 1);
    r.fsm.Spark = spark;
    r.fsm.PWM_Ext = zeros(n, 1);
    r.Press_H2 = 8 * ones(n, 1);
    r.m_H2 = NaN(n, 1);
    r.sc = sc;
    r.moteur = 'pilote';
end
