%% scenario_sechoir.m  —  scénarios de simulation du séchoir hybride
%
%   sc = scenario_sechoir(n)      n = 1 .. 12 (tableau 14.4 du mémoire)
%
% Les accents des noms sont écrits par leur code (char(233) = é) pour être
% corrects quel que soit l'encodage du fichier.
%
% Un scénario est une structure :
%   sc.num, sc.nom, sc.attendu   identification et résultat attendu
%   sc.StopTime                  durée simulée (s)
%   sc.Tsec0, sc.H0              T_sec et H_sec initiales
%   sc.signaux.<X>               matrice [t valeur] (valeur maintenue jusqu'au
%                                point suivant) pour chaque entrée pilotée :
%                                boutons, consignes, capteurs, défauts
%   sc.param.<P>                 valeur initiale des paramètres Local du chart
%                                (mêmes noms que dans le chart, durées en s)
%
% Tous les champs ont une valeur par défaut (cycle nominal H2 à froid) ; un
% scénario ne modifie que ce qui le distingue. Pour créer un scénario, copier
% un "case" et changer les signaux ou les paramètres.

function sc = scenario_sechoir(n)

    if nargin < 1
        n = 1;
    end
    sc = scenarioParDefaut();
    sc.num = n;
    s = sc.signaux;
    p = sc.param;

    switch n
        case 1
            sc.nom = ['D' char(233) 'marrage ' char(224) ' froid H2'];
            sc.attendu = 'Purge 120 s, allumage a 100 %, puis 67, 33 et 0 %, rallumages a 33 %';
            sc.StopTime = 3600;

        case 2
            sc.nom = ['D' char(233) 'marrage solaire'];
            sc.attendu = 'MODE_SOLAIRE (T_cap_est = 35 + 20 = 55 >= 55), aucune vanne de gaz ouverte';
            s.T_amb = cst(35);
            s.P_sol = cst(PUISSANCE_SOLAIRE_NOMINALE());
            sc.Tsec0 = 35;
            sc.StopTime = 3600;

        case 3
            sc.nom = 'Bascule H2 -> GPL';
            sc.attendu = 'A 1250 s (bruleur a 33 %), Press_H2 < 2 bar : purge 120 s puis rallumage GPL a 33 % (palier conserve)';
            s.Press_H2 = marches([0 8; 1250 0.5]);
            sc.StopTime = 3600;

        case 4
            sc.nom = 'Retour au solaire';
            sc.attendu = 'Quand T_amb atteint 35 C (T_cap_est >= 55) : extinction, post-purge 120 s, MODE_SOLAIRE';
            t = (1200:60:2400)';
            s.T_amb = [0 25; t, 25 + 13 * (t - 1200) / 1200];
            s.P_sol = [0 0; t, PUISSANCE_SOLAIRE_NOMINALE() * (t - 1200) / 1200];
            sc.StopTime = 3600;

        case 5
            sc.nom = [char(201) 'checs d''allumage'];
            sc.attendu = '3 essais separes par des purges -> ERREUR_COMBUSTION ; OK a 700 s -> reprise';
            s.Flamme_panne = marches([0 1; 650 0]);
            s.Btn_OK = impulsions(700);
            sc.StopTime = 1500;

        case 6
            sc.nom = 'Pertes de flamme';
            sc.attendu = '1re perte (350 s) : relance ; 2e perte (700 s) : URGENCE cause 4 ; rearmement a 900 s';
            s.T_cible = cst(70);               % régulation continue plus longue
            s.Flamme_panne = marches([0 0; 350 1; 355 0; 700 1; 705 0]);
            s.Btn_Rearm = impulsions(900);
            sc.StopTime = 1200;

        case 7
            sc.nom = 'Fuite H2';
            sc.attendu = 'URGENCE (cause 1) a 1500 s ; rearmement refuse a 1700 s, accepte a 1900 s ; relance a 2000 s';
            s.MQ8_H2 = marches([0 50; 1500 600; 1800 50]);
            s.Btn_Rearm = impulsions([1700 1900]);
            s.Btn_Start = impulsions([10 2000]);
            sc.StopTime = 2600;

        case 8
            sc.nom = ['Fin par humidit' char(233)];
            sc.attendu = 'H_sec <= H_fin et t >= Temps_Min_Fin : DEMANDE_PROLONGATION, sans reponse -> TERMINE apres 300 s';
            sc.StopTime = 8400;

        case 9
            sc.nom = ['Prolongation accept' char(233) 'e'];
            sc.attendu = 'OK pendant la demande : PROLONGATION 1800 s, nouvelle demande, puis TERMINE';
            s.Btn_OK = impulsions(7300);
            sc.StopTime = 9900;

        case 10
            sc.nom = ['Mode d' char(233) 'grad' char(233) ' : humidit' char(233) ' FIXE'];
            sc.attendu = 'Humidite fixe (60 %) : fin uniquement par Duree_Max_Cycle (90 min ici)';
            p.Fixe_H_sec = 1;
            p.Val_H_sec = 60;
            p.Duree_Max_Cycle = 5400;
            sc.StopTime = 6300;

        case 11
            sc.nom = ['Palier impos' char(233) ' 33 %'];
            sc.attendu = 'Palier 33 % quelle que soit T_sec ; sans regulation, T_sec finit par atteindre 90 C -> TERMINE';
            p.Regul_Auto = 0;
            p.Palier_Impose = 2;
            sc.StopTime = 2700;

        case 12
            sc.nom = 'Surchauffe';
            sc.attendu = 'Palier impose 100 % : T_sec >= 90 C -> SECHAGE_TERMINE (securite)';
            p.Regul_Auto = 0;
            p.Palier_Impose = 0;
            sc.StopTime = 1500;

        otherwise
            error('scenario_sechoir:numero', 'Scenario %d inconnu (1 a 12).', n);
    end
    sc.signaux = s;
    sc.param = p;
end

%% ---------------------------------------------------------------------
function sc = scenarioParDefaut()
    sc.num = 0;
    sc.nom = '';
    sc.attendu = '';
    sc.StopTime = 3600;
    sc.Tsec0 = 25;          % °C
    sc.H0 = 90;             % % HR de l'air extrait au départ

    % --- Boutons (impulsions de 1 s)
    s.Btn_Start = impulsions(10);
    s.Btn_Stop = cst(0);
    s.Btn_OK = cst(0);
    s.Btn_UP = cst(0);
    s.Btn_DOWN = cst(0);
    s.Btn_SELECT = cst(0);
    s.Btn_Rearm = cst(0);
    s.Btn_Prolongation = cst(0);     % entrée d'origine, inutilisée en v3
    % --- Consignes opérateur
    s.Mode_Auto = cst(1);
    s.Choix_Manuel = cst(2);         % utilisé seulement si Mode_Auto = 0
    s.T_cible = cst(55);             % °C
    s.H_produit = cst(10);           % H_fin = min(H_produit + H_amb, 95)
    s.Tps_Prolongation = cst(1800);  % s
    s.H_initial = cst(90);           % entrée d'origine, inutilisée en v3
    s.T_cap = cst(0);                % entrée d'origine, remplacée par T_cap_est
    % --- Capteurs et environnement
    s.T_amb = cst(25);               % °C
    s.H_amb = cst(35);               % % HR
    s.Press_H2 = cst(8);             % bar
    s.MQ8_H2 = cst(50);
    s.MQ6_But = cst(50);
    s.AU_Manuel = cst(0);
    % --- Modèle physique (hors chart)
    s.P_sol = cst(0);                % W apportés par le capteur solaire
    s.Flamme_panne = cst(0);         % 1 = la flamme ne s'établit pas / s'éteint
    s.Flamme_parasite = cst(0);      % 1 = flamme vue alors que le gaz est fermé
    sc.signaux = s;

    % --- Paramètres Local du chart (valeurs par défaut du firmware)
    p.Hhyst = 5;
    p.Temps_Min_Fin = 7200;
    p.Duree_Max_Cycle = 36000;
    p.Temps_Reponse = 300;
    p.Temps_Purge = 120;
    p.Temps_Allumage = 4;
    p.Periode_Regul = 1;
    p.Regul_Auto = 1;
    p.Palier_Impose = 1;
    p.DeltaT_Sol = 20;
    p.Marge_Sol = 0;
    p.Hyst_Sol = 5;
    p.Seuil_Chaud = 40;
    p.Fixe_H_sec = 0;
    p.Val_H_sec = 60;
    p.T_SEC_MAX_SECURITE = 90;
    sc.param = p;
end

function P = PUISSANCE_SOLAIRE_NOMINALE()
    % Puissance qui amène la chambre à T_amb + 20 °C en régime établi :
    % DeltaT = Kth * P  =>  P = 20 / 0.098 (cohérent avec DeltaT_Sol = 20).
    P = 20 / 0.098;
end

function m = cst(v)
    m = [0 v; 1 v];
end

function m = marches(points)
    % points = [t1 v1; t2 v2; ...] ; la valeur est maintenue jusqu'au point suivant.
    m = points;
    if m(1, 1) > 0
        m = [0 0; m];
    end
end

function m = impulsions(instants)
    % Impulsions de 1 s (appui bouton) aux instants donnés (s).
    m = [0 0];
    for k = 1:numel(instants)
        m = [m; instants(k) 1; instants(k) + 1 0]; %#ok<AGROW>
    end
end
