%% bilan_etude.m  —  grandeurs de synthèse d'une simulation d'étude
%
%   b = bilan_etude(r)
%   b = bilan_etude(r, bande)       bande de régulation (défaut [52.5 57.5] °C)
%
% Puissances et grandeurs logiques maintenues d'un point au suivant.
%   b.E_H2, b.E_GPL     énergie utile de gaz par source (kWh), selon la vanne ouverte
%   b.E_sol             énergie solaire pendant MODE_SOLAIRE (kWh)
%   b.E_sol_total       énergie solaire reçue sur tout le cycle (kWh)
%   b.f_sol             E_sol / (E_sol + E_H2 + E_GPL)
%   b.m_H2, b.m_GPL     masses de combustible (g) = E / (eta PCI)
%   b.CO2               CO2 de combustion du butane (kg) = 3,03 x masse de butane
%   b.bande             % du temps dans la bande, compté depuis la 1re entrée
%   b.hors              minutes hors 45-70 °C, comptées depuis la 1re atteinte de 45 °C
%   b.rallumages        nombre d'entrées dans ALLUMAGE (fronts montants de Spark)
%   b.T_max, b.P_moy    T_sec maximale (°C), puissance de gaz moyenne (W)
%   b.t_*               premiers instants (s) : SOLAIRE, H2, GPL, ERREUR, URGENCE
%                       (NaN si jamais atteint)

function b = bilan_etude(r, bande)

    if nargin < 2
        bande = [52.5 57.5];
    end
    t = r.t(:);
    w = [diff(t); 0];                 % durée pendant laquelle chaque point est maintenu
    etat = round(r.fsm.Etat_LCD(:));
    vH2 = r.fsm.V_H2(:) ~= 0;
    vBut = r.fsm.V_But(:) ~= 0;
    P = r.P_gaz(:);
    J2kWh = 1 / 3.6e6;

    b.E_H2 = sum(P .* vH2 .* w) * J2kWh;
    b.E_GPL = sum(P .* vBut .* w) * J2kWh;
    b.E_gaz = b.E_H2 + b.E_GPL;
    b.E_sol = sum(r.P_sol(:) .* (etat == 2) .* w) * J2kWh;
    b.E_sol_total = sum(r.P_sol(:) .* w) * J2kWh;
    b.f_sol = b.E_sol / max(b.E_sol + b.E_gaz, eps);
    e = r.sc.etude;
    b.m_H2 = 1000 * b.E_H2 / (e.eta * e.PCI_H2);
    b.m_GPL = 1000 * b.E_GPL / (e.eta * e.PCI_GPL);
    b.CO2 = 3.03 * b.m_GPL / 1000;
    b.P_moy = sum(P .* w) / max(t(end) - t(1), eps);

    T = r.T_sec(:);
    b.T_max = max(T);
    k0 = find(T >= bande(1), 1);
    if isempty(k0)
        b.bande = 0;
        b.t_bande = NaN;
    else
        dedans = T >= bande(1) & T <= bande(2);
        b.bande = 100 * sum(w(k0:end) .* dedans(k0:end)) / max(t(end) - t(k0), eps);
        b.t_bande = t(k0);
    end
    k45 = find(T >= 45, 1);
    if isempty(k45)
        b.hors = (t(end) - t(1)) / 60;
    else
        hors = T < 45 | T > 70;
        b.hors = sum(w(k45:end) .* hors(k45:end)) / 60;
    end
    spark = r.fsm.Spark(:) ~= 0;
    b.rallumages = sum(diff([0; spark]) > 0);

    b.t_solaire = premier(t, etat == 2);
    b.t_H2 = premier(t, etat == 3);
    b.t_GPL = premier(t, etat == 4);
    b.t_erreur = premier(t, etat == 8);
    b.t_urgence = premier(t, etat == 9);
    b.n_erreur = sum(diff([0; etat == 8]) > 0);
    b.duree = t(end) - t(1);
end

function t0 = premier(t, masque)
    k = find(masque, 1);
    if isempty(k)
        t0 = NaN;
    else
        t0 = t(k);
    end
end
