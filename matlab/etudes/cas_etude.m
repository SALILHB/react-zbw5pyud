%% cas_etude.m  —  cas simulés pour les études E1 à E4 du mémoire
%
%   sc = cas_etude('C1')          journée type, cas C1 à C8 (étude E1)
%   sc = cas_etude('C4', m0)      stock d'hydrogène m0 (g) pour C4 et C5
%   sc = cas_etude('M1')          scénario 8 sur 2 h (référence de l'étude E2)
%   sc = cas_etude('E4')          scénario 1 (cas de base de l'étude E4)
%
% Journée type (E1) : cycle de 10 h de 8 h à 18 h (t = 0 à 8 h 00), START à
% 10 s, T_cible = 55 °C, T_sec initiale = T_amb(8 h). Profils échantillonnés
% toutes les 60 s (valeur maintenue entre deux points) :
%   T_amb(h) = 28 + 10 sin(2 pi (h - 9) / 24)
%   P_sol(h) = P_sol,max k_ciel max(0, sin(pi (h - 6) / 12)),  P_sol,max = 20 / Kth
% L'humidité est FIXÉE à 90 % (Fixe_H_sec) : le cycle n'est arrêté ni par
% l'humidité ni par Temps_Min_Fin et couvre toute la journée (hypothèse).

function sc = cas_etude(id, m0)

    if nargin < 2
        m0 = 0;
    end
    id = upper(id);
    if strcmp(id, 'M1')
        sc = completer_etude(scenario_sechoir(8));
        sc.StopTime = 7200;
        sc.id = id;
        return;
    end
    if strcmp(id, 'E4')
        sc = completer_etude(scenario_sechoir(1));
        sc.id = id;
        sc.etude.log = 2;
        return;
    end

    sc = completer_etude(scenario_sechoir(1));
    sc.id = id;
    sc.num = 0;
    sc.StopTime = 36000;
    sc.param.Fixe_H_sec = 1;
    sc.param.Val_H_sec = 90;
    sc.param.Duree_Max_Cycle = 36000;

    kCiel = 1;
    Tmoy = 28;
    Tamp = 10;
    s = sc.signaux;
    switch id
        case 'C1'
            sc.nom = 'Journée claire, installation complète';
        case 'C2'
            sc.nom = 'Journée couverte';
            kCiel = 0.25;
            Tmoy = 22;
            Tamp = 4;
        case 'C3'
            sc.nom = 'Réservoir d''hydrogène vide';
            s.Press_H2 = [0 0; 1 0];
        case 'C4'
            sc.nom = 'Stock d''hydrogène limité';
            sc.etude.m0 = m0;
        case 'C5'
            sc.nom = 'Stock d''hydrogène limité, GPL indisponible';
            sc.etude.m0 = m0;
            s.GPL_dispo = [0 0; 1 0];
        case 'C6'
            sc.nom = 'Solaire seul (mode manuel)';
            s.Mode_Auto = [0 0; 1 0];
            s.Choix_Manuel = [0 1; 1 1];
        case 'C7'
            sc.nom = 'Sans soleil, hydrogène (mode manuel)';
            kCiel = 0;
            s.Mode_Auto = [0 0; 1 0];
            s.Choix_Manuel = [0 2; 1 2];
        case 'C8'
            sc.nom = 'Journée chaude mais couverte';
            kCiel = 0.25;
        otherwise
            error('cas_etude:id', 'Cas %s inconnu (C1 a C8, M1, E4).', id);
    end
    t = (0:60:sc.StopTime)';
    h = 8 + t / 3600;
    s.T_amb = [t, Tmoy + Tamp * sin(2 * pi * (h - 9) / 24)];
    s.P_sol = [t, (20 / 0.098) * kCiel * max(0, sin(pi * (h - 6) / 12))];
    sc.signaux = s;
    sc.Tsec0 = s.T_amb(1, 2);
    sc.attendu = sprintf('Journee type %s', id);
end
