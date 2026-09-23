%% completer_fsm_sechoir.m  —  version 3 (alignée sur le firmware v3)
%
% Complète le chart Stateflow "FSM/Chart" du modèle Commande_Sechoir_Hybride
% pour qu'il reproduise EXACTEMENT la logique du firmware Arduino
% sechoir_hybride/sechoir_hybride.ino (source de vérité). Référence complète :
% docs/FSM_SIMULINK.md.
%
% PARTIR DU MODÈLE D'ORIGINE : matlab/Commande_Sechoir_Hybride_corrige.slx
% (chart FSM d'origine + corrections physiques Pnom/Kth/tauth). Les versions
% précédentes de ce script contenaient des erreurs (voir docs/FSM_SIMULINK.md
% §0) : ne réutilisez pas un .slx déjà modifié par elles.
%
% Ce que fait le script (relançable : il supprime puis recrée ses propres
% transitions, repérées par Description = 'completer_fsm_sechoir', et
% réécrit les libellés des états qu'il gère) :
%   1. déclare les données manquantes (paramètres en Local : pas de nouveau
%      port à câbler, sauf Btn_Stop et Btn_Rearm) ;
%   2. construit la hiérarchie :
%        FONCTIONNEMENT_NORMAL (OR)
%          ATTENTE_DEMARRAGE
%          EN_CYCLE (AND, deux régions parallèles)
%            SOURCE : MODE_SOLAIRE / MODE_H2 / MODE_GPL / ERREUR_COMBUSTION
%                     (MODE_H2 et MODE_GPL : PURGE -> ALLUMAGE -> REGULATION)
%            PHASE  : NORMAL / DEMANDE_PROLONGATION / PROLONGATION
%          SECHAGE_TERMINE
%        URGENCE_ATEX
%      (FIN_TEMPORISATION est renommé DEMANDE_PROLONGATION) ;
%   3. écrit les actions de chaque état et toutes les transitions.
%
% Langage d'action du chart : MATLAB (actionLanguage = 2 dans le .slx). Toute
% la syntaxe ci-dessous est donc MATLAB (if ... end, ~=, ~in(...)).
%
% NON EXÉCUTÉ DANS UNE VRAIE SESSION MATLAB (pas de MATLAB dans
% l'environnement de développement). Points d'API non vérifiés, signalés à
% l'exécution s'ils échouent :
%   - Decomposition = 'PARALLEL_AND' / 'EXCLUSIVE_OR' ;
%   - le changement de parent d'un état existant (par sa position, puis
%     .Parent, puis sf('set',...)) — repli manuel décrit si tout échoue ;
%   - les opérateurs temporels temporalCount(sec) et duration(...).

function completer_fsm_sechoir(modelName)

    if nargin < 1
        modelName = 'Commande_Sechoir_Hybride';
    end
    if ~bdIsLoaded(modelName)
        load_system(modelName);
    end

    chart = trouverChart(modelName, 'FSM/Chart');
    if isempty(chart)
        error('completer_fsm_sechoir:chartIntrouvable', ...
            'Chart "%s/FSM/Chart" introuvable.', modelName);
    end
    fprintf('=== Completion du chart : %s ===\n', chart.Path);

    signalerTransitionsInconnues(chart);
    supprimerTransitionsDuScript(chart);
    supprimerDonneesObsoletes(chart);
    ajouterDonnees(chart);

    h = construireHierarchie(chart);
    if ~h.ok
        fprintf(['\n*** STRUCTURE INCOMPLETE — aucune transition n''a ete creee. ***\n' ...
                 'Dans l''editeur Stateflow, glissez-deposez les etats signales ci-dessus\n' ...
                 'dans leur boite parente (voir docs/FSM_SIMULINK.md §1), reglez EN_CYCLE\n' ...
                 'en decomposition "AND (parallel)" (clic droit > Decomposition), puis\n' ...
                 'relancez completer_fsm_sechoir.\n']);
        return;
    end

    ecrireActionsEtats(h);
    ajouterTransitions(chart, h);
    alignerTransitionsManuellesExistantes(chart);

    fprintf('\n=== Termine. Simulink : clic droit sur le chart -> "Update Chart", ===\n');
    fprintf('=== puis Diagnostic Viewer pour toute erreur residuelle.            ===\n');
end

%% =====================================================================
%  Constantes du script
%  =====================================================================

function t = TAG()
    t = 'completer_fsm_sechoir';
end

function ssids = SSID_TRANSITIONS_ORIGINE()
    % Transitions présentes dans le .slx fourni par l'opérateur : défaut
    % FN->ATTENTE (31), FN->URGENCE (118), manuel solaire (119), manuel H2 (120).
    ssids = [31 118 119 120];
end

%% =====================================================================
%  Recherche
%  =====================================================================

function chart = trouverChart(modelName, chartPath)
    chart = [];
    tous = sfroot.find('-isa', 'Stateflow.Chart');
    for i = 1:numel(tous)
        if strcmp(tous(i).Path, [modelName '/' chartPath])
            chart = tous(i);
            return;
        end
    end
end

function n = nomEtat(s)
    lignes = strsplit(s.LabelString, sprintf('\n'));
    n = strtrim(lignes{1});
end

function s = trouverEtat(chart, nom)
    % Recherche globale : les noms des états du FSM sont uniques, sauf
    % PURGE / ALLUMAGE / REGULATION (voir trouverEnfant).
    s = [];
    tous = chart.find('-isa', 'Stateflow.State');
    for i = 1:numel(tous)
        if strcmp(nomEtat(tous(i)), nom)
            s = tous(i);
            return;
        end
    end
end

function s = trouverEnfant(parent, nom)
    s = [];
    enfants = parent.find('-isa', 'Stateflow.State', '-depth', 1);
    for i = 1:numel(enfants)
        if enfants(i) ~= parent && strcmp(nomEtat(enfants(i)), nom)
            s = enfants(i);
            return;
        end
    end
end

function c = cheminComplet(obj)
    % Chemin hiérarchique d'un état : Path (chemin du parent) + nom.
    if isa(obj, 'Stateflow.Chart')
        c = obj.Path;
    else
        c = [obj.Path '/' nomEtat(obj)];
    end
end

function b = estEnfantDirect(s, parent)
    b = strcmp(s.Path, cheminComplet(parent));
end

function d = trouverDonnee(chart, nom)
    d = [];
    donnees = chart.find('-isa', 'Stateflow.Data');
    for i = 1:numel(donnees)
        if strcmp(donnees(i).Name, nom)
            d = donnees(i);
            return;
        end
    end
end

%% =====================================================================
%  Nettoyage (relance du script)
%  =====================================================================

function signalerTransitionsInconnues(chart)
    trs = chart.find('-isa', 'Stateflow.Transition');
    inconnues = {};
    for i = 1:numel(trs)
        t = trs(i);
        if ~ismember(t.SSID, SSID_TRANSITIONS_ORIGINE()) && ~strcmp(t.Description, TAG())
            inconnues{end+1} = sprintf('SSID %d : %s', t.SSID, strtrim(t.LabelString)); %#ok<AGROW>
        end
    end
    if ~isempty(inconnues)
        fprintf('--- ATTENTION : transitions non reconnues (version precedente du script ?) ---\n');
        fprintf('    %s\n', inconnues{:});
        fprintf(['    Recommande : repartir de matlab/Commande_Sechoir_Hybride_corrige.slx.\n' ...
                 '    Elles sont conservees telles quelles.\n']);
    end
end

function supprimerTransitionsDuScript(chart)
    trs = chart.find('-isa', 'Stateflow.Transition', 'Description', TAG());
    for i = 1:numel(trs)
        trs(i).delete;
    end
    if ~isempty(trs)
        fprintf('--- %d transition(s) d''une execution precedente supprimee(s) (recreees ci-dessous) ---\n', numel(trs));
    end
end

function supprimerDonneesObsoletes(chart)
    % Données créées par les versions 1-2 de ce script et abandonnées en v3.
    obsoletes = {'Seuil_Tcap_ON', 'Seuil_Tcap_OFF', 'palier0_stable', ...
                 'raison_erreur_combustion', 'arret_force_duree', ...
                 'T_init_saisie', 'AU_Urgence', 'Temps_Arret_Auto', 'Temps_Prolongation'};
    for i = 1:numel(obsoletes)
        d = trouverDonnee(chart, obsoletes{i});
        if ~isempty(d)
            d.delete;
            fprintf('  [donnee obsolete supprimee] %s\n', obsoletes{i});
        end
    end
end

%% =====================================================================
%  1. Données
%  =====================================================================

function ajouterDonnees(chart)
    fprintf('--- Donnees ---\n');
    % Nom, portée, type, valeur initiale. Toutes les DURÉES du chart sont
    % en SECONDES (le menu du firmware affiche des minutes : x60).
    specs = {
        % Nouvelles entrées (boutons du firmware absents du modèle d'origine)
        'Btn_Stop',            'Input', 'uint8',  []
        'Btn_Rearm',           'Input', 'uint8',  []
        % Paramètres opérateur (menu du firmware)
        'T_init_manuel',       'Local', 'double', 25      % T_init en mode manuel
        'Seuil_MQ8',           'Local', 'double', 350
        'Seuil_MQ6',           'Local', 'double', 350
        'Press_H2_Min',        'Local', 'double', 2
        'Marge_Retour_H2',     'Local', 'double', 0.5
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
        'Fixe_T_amb',          'Local', 'uint8',  0
        'Val_T_amb',           'Local', 'double', 25
        'Fixe_H_amb',          'Local', 'uint8',  0
        'Val_H_amb',           'Local', 'double', 40
        'Fixe_H_sec',          'Local', 'uint8',  0
        'Val_H_sec',           'Local', 'double', 60
        'Fixe_Press',          'Local', 'uint8',  0
        'Val_Press',           'Local', 'double', 5
        % Constantes techniques (non réglables au menu dans le firmware)
        'MAX_ECHECS',          'Local', 'uint16', 3
        'MAX_PERTES_FLAMME',   'Local', 'uint8',  1
        'T_SEC_MAX_SECURITE',  'Local', 'double', 90
        'DELAI_FLAMME_PARASITE','Local','double', 5       % s
        % Variables internes
        'T3_seuil',            'Local', 'double', 0
        'T_init',              'Local', 'double', 25
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
        nom = specs{i,1};
        if ~isempty(trouverDonnee(chart, nom))
            continue;
        end
        d = Stateflow.Data(chart);
        d.Name = nom;
        d.Scope = specs{i,2};
        d.DataType = specs{i,3};
        if ~isempty(specs{i,4})
            d.Props.InitialValue = num2str(specs{i,4});
        end
        d.Description = TAG();
        fprintf('  [ajoute] %s (%s, %s)\n', nom, specs{i,2}, specs{i,3});
    end
    fprintf(['  Donnees d''origine reutilisees : Tps_Prolongation (duree de\n' ...
             '  prolongation proposee, en SECONDES : 1800 = 30 min), Temps_Min_Fin\n' ...
             '  (7200 s), Hhyst, H_fin, T1_seuil, T2_seuil, AU_Manuel (= AU_Urgence du\n' ...
             '  firmware). Inutilisees : T_cap (remplacee par T_cap_est), Btn_Prolongation,\n' ...
             '  H_initial, chrono_confirmation.\n']);
end

%% =====================================================================
%  2. Hiérarchie
%  =====================================================================

function h = construireHierarchie(chart)
    fprintf('--- Hierarchie ---\n');
    h = struct('ok', false);

    fn  = trouverEtat(chart, 'FONCTIONNEMENT_NORMAL');
    urg = trouverEtat(chart, 'URGENCE_ATEX');
    att = trouverEtat(chart, 'ATTENTE_DEMARRAGE');
    ter = trouverEtat(chart, 'SECHAGE_TERMINE');
    if any(cellfun(@isempty, {fn, urg, att, ter}))
        fprintf('  [ERREUR] FONCTIONNEMENT_NORMAL / URGENCE_ATEX / ATTENTE_DEMARRAGE / SECHAGE_TERMINE introuvable.\n');
        return;
    end

    % Une version précédente du script avait rendu FONCTIONNEMENT_NORMAL
    % parallèle : il doit rester OR (il contient ATTENTE et SECHAGE_TERMINE).
    reglerDecomposition(fn, 'EXCLUSIVE_OR');

    % Agrandissement du cadre et placement des états de premier niveau.
    fn.Position  = [20 20 1040 920];
    urg.Position = [20 980 320 150];
    att.Position = [60 60 180 80];
    ter.Position = [860 60 180 80];

    ok = true;
    en  = placer(chart, 'EN_CYCLE', fn, [40 170 1000 750]);
    reglerDecomposition(en, 'PARALLEL_AND');
    src = placer(chart, 'SOURCE', en, [60 200 640 700]);
    pha = placer(chart, 'PHASE',  en, [720 200 300 700]);
    ordreExecution(src, 1);
    ordreExecution(pha, 2);

    % FIN_TEMPORISATION (v2) devient DEMANDE_PROLONGATION (v3).
    if isempty(trouverEtat(chart, 'DEMANDE_PROLONGATION'))
        ft = trouverEtat(chart, 'FIN_TEMPORISATION');
        if ~isempty(ft)
            ft.LabelString = 'DEMANDE_PROLONGATION';
            fprintf('  [renomme] FIN_TEMPORISATION -> DEMANDE_PROLONGATION\n');
        end
    end

    sol = placer(chart, 'MODE_SOLAIRE',         src, [80 240 200 90]);
    err = placer(chart, 'ERREUR_COMBUSTION',    src, [320 240 360 90]);
    h2  = placer(chart, 'MODE_H2',              src, [80 360 600 250]);
    gpl = placer(chart, 'MODE_GPL',             src, [80 630 600 250]);
    nor = placer(chart, 'NORMAL',               pha, [740 240 260 80]);
    dem = placer(chart, 'DEMANDE_PROLONGATION', pha, [740 360 260 250]);
    pro = placer(chart, 'PROLONGATION',         pha, [740 630 260 250]);

    sm_h2  = sousMachine(h2,  [100 400 170 190], [290 400 170 190], [480 400 190 190]);
    sm_gpl = sousMachine(gpl, [100 670 170 190], [290 670 170 190], [480 670 190 190]);

    % Vérification finale de la hiérarchie (le point le plus incertain).
    attendus = {en, fn; src, en; pha, en; sol, src; err, src; h2, src; gpl, src; ...
                nor, pha; dem, pha; pro, pha; att, fn; ter, fn};
    for i = 1:size(attendus, 1)
        s = attendus{i,1}; p = attendus{i,2};
        if ~estEnfantDirect(s, p)
            fprintf('  [HIERARCHIE INCORRECTE] %s devrait etre dans %s (actuellement : %s)\n', ...
                nomEtat(s), nomEtat(p), s.Path);
            ok = false;
        end
    end
    if ~estParallele(en)
        fprintf('  [HIERARCHIE INCORRECTE] EN_CYCLE doit etre en decomposition AND (parallel)\n');
        ok = false;
    end
    if ok
        fprintf('  [ok] hierarchie conforme\n');
    end

    h = struct('ok', ok, 'chart', chart, 'fn', fn, 'urg', urg, 'att', att, 'ter', ter, ...
               'en', en, 'src', src, 'pha', pha, 'sol', sol, 'err', err, 'h2', h2, ...
               'gpl', gpl, 'nor', nor, 'dem', dem, 'pro', pro, ...
               'sm_h2', sm_h2, 'sm_gpl', sm_gpl);
end

function s = placer(chart, nom, parent, position)
    % Trouve (ou crée) l'état `nom`, le place à `position` (coordonnées du
    % chart, à l'intérieur de la boîte du parent) et s'assure qu'il est bien
    % enfant de `parent`. Stateflow dérive la hiérarchie de l'inclusion
    % géométrique ; si ce n'est pas le cas dans votre version, on tente de
    % changer le parent explicitement.
    s = trouverEtat(chart, nom);
    if isempty(s)
        s = Stateflow.State(parent);
        s.LabelString = nom;
        s.Description = TAG();
        s.Position = position;
        fprintf('  [cree] %s\n', nom);
        return;
    end
    s.Position = position;
    if ~estEnfantDirect(s, parent)
        changerParent(s, parent);
    end
end

function changerParent(s, parent)
    try
        s.Parent = parent; %#ok<NASGU>
    catch
        try
            sf('set', s.Id, '.parent', parent.Id);
        catch
        end
    end
    if estEnfantDirect(s, parent)
        fprintf('  [deplace] %s -> %s\n', nomEtat(s), nomEtat(parent));
    end
end

function sm = sousMachine(mode, posPurge, posAllumage, posRegul)
    sm.purge      = enfantPlace(mode, 'PURGE', posPurge);
    sm.allumage   = enfantPlace(mode, 'ALLUMAGE', posAllumage);
    sm.regulation = enfantPlace(mode, 'REGULATION', posRegul);
end

function s = enfantPlace(parent, nom, position)
    s = trouverEnfant(parent, nom);
    if isempty(s)
        s = Stateflow.State(parent);
        s.LabelString = nom;
        s.Description = TAG();
        fprintf('  [cree] %s/%s\n', nomEtat(parent), nom);
    end
    s.Position = position;
end

function reglerDecomposition(s, valeur)
    try
        if ~strcmp(s.Decomposition, valeur)
            s.Decomposition = valeur;
            fprintf('  [decomposition] %s = %s\n', nomEtat(s), valeur);
        end
    catch e
        fprintf('  [ATTENTION] Decomposition de %s non reglable par API (%s)\n', nomEtat(s), e.message);
    end
end

function b = estParallele(s)
    try
        b = strcmp(s.Decomposition, 'PARALLEL_AND');
    catch
        b = false;
    end
end

function ordreExecution(s, n)
    try
        s.ExecutionOrder = n;
    catch
        % ordre par défaut (position graphique) : SOURCE est à gauche, donc
        % exécutée avant PHASE, comme dans le firmware.
    end
end

%% =====================================================================
%  3. Actions des états (langage d'action MATLAB)
%  =====================================================================

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

%% =====================================================================
%  4. Transitions
%  =====================================================================

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

    % --- Transitions par défaut des nouveaux états OR ---
    transition(chart, [], h.sol, '');                       % SOURCE (sécurité : pas de gaz)
    transition(chart, [], h.nor, '');                       % PHASE
    transition(chart, [], h.sm_h2.purge, '');
    transition(chart, [], h.sm_gpl.purge, '');

    % --- 1b. URGENCE : flamme vue alors que le gaz est commandé fermé.
    %     (1a, fuite/AU, est la transition d'origine SSID 118.)
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
    % Démarrage manuel GPL (solaire et H2 : transitions d'origine 119/120).
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
    transition(chart, sm.purge, sm.allumage, ...
        '[after(Temps_Purge, sec) && (raison_purge ~= 3 || T_sec < T3_seuil - Hhyst/2)]{if raison_purge==3, palier=uint8(2); end}');
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

function alignerTransitionsManuellesExistantes(chart)
    % Les transitions d'origine 119/120 (démarrage manuel solaire / H2)
    % testent Mode_Auto (entrée brute). Le reste du chart utilise
    % Mode_Auto_Eff (qui tient compte du choix MANUEL/AUTOMATIQUE fait en
    % ERREUR_COMBUSTION) : on aligne leur garde, sans rien changer d'autre.
    trs = chart.find('-isa', 'Stateflow.Transition');
    for i = 1:numel(trs)
        t = trs(i);
        if ismember(t.SSID, [119 120]) && isempty(strfind(t.LabelString, 'Mode_Auto_Eff')) %#ok<STREMP>
            ancien = t.LabelString;
            t.LabelString = regexprep(ancien, '\<Mode_Auto\>', 'Mode_Auto_Eff');
            fprintf('  [aligne] SSID %d : %s  ->  %s\n', t.SSID, ancien, t.LabelString);
        end
    end
end
