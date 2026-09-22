%% completer_fsm_sechoir.m
%
% Complete le chart Stateflow "FSM/Chart" du modele Commande_Sechoir_Hybride
% via l'API Stateflow officielle (Stateflow.State, Stateflow.Transition,
% Stateflow.Data) -- aucune edition manuelle du XML.
%
% NOTE (corrige suite a un test reel) : la version initiale essayait de
% factoriser gerer_palier()/appliquer_palier()/fermer_gaz()/calculer_seuils()
% via Stateflow.EMLFunction, classe indisponible dans certaines versions de
% MATLAB ("Unable to resolve the name 'Stateflow.EMLFunction.empty'"). Cette
% version inline directement la logique dans chaque action d'etat/transition
% (duplication assumee entre MODE_H2 et MODE_GPL, cf. docs/FSM_SIMULINK.md
% §3, note sur la duplication) -- ne repose plus que sur Stateflow.State,
% Stateflow.Transition et Stateflow.Data, deja confirmes fonctionnels.
%
% CE SCRIPT N'A PAS ETE EXECUTE NI VERIFIE DANS UNE VRAIE SESSION MATLAB
% (environnement de developpement sans MATLAB/Simulink). Executez-le avec
% le modele deja ouvert, section par section si vous preferez controler
% chaque etape (chaque sous-fonction est idempotente : relancer le script
% ne duplique pas ce qui existe deja).
%
% Prerequis avant d'executer :
%   1. Ouvrir Commande_Sechoir_Hybride.slx dans Simulink.
%   2. Recabler manuellement les 9 entrees manquantes du sous-systeme
%      "FSM" (voir la checklist en fin de fichier / docs/FSM_SIMULINK.md).
%   3. Executer ce script (F5, ou section par section avec Ctrl+Entree).
%   4. Dans Simulink : clic droit sur le chart -> "Update Chart" (ou
%      Ctrl+D sur le modele) pour resynchroniser le sous-systeme genere.
%
% Reference complete (dictionnaire de donnees, tableau de transitions,
% avis critiques) : docs/FSM_SIMULINK.md

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
            'Chart "%s/FSM/Chart" introuvable. Verifiez que le sous-systeme FSM contient bien un bloc "Chart".', modelName);
    end

    fprintf('=== Completion du chart : %s ===\n', chart.Path);

    ajouterDonnees(chart);
    completerEtatsSimples(chart);
    % ERREUR_COMBUSTION doit exister AVANT les sous-machines de combustion :
    % celles-ci y creent des transitions (echec de basculement, echecs
    % repetes) et echoueraient silencieusement si l'etat n'existait pas
    % encore (creerTransitionVersErreur() se contente alors d'un message et
    % ne cree rien -- l'ordre correct evite d'avoir a relancer le script).
    ajouterEtatErreurCombustion(chart);
    ajouterSousMachineCombustion(chart, 'MODE_H2');
    ajouterSousMachineCombustion(chart, 'MODE_GPL');
    ajouterTransitionsPrioritaires(chart);
    ajouterTransitionsArbitrage(chart);

    fprintf('\n=== Termine. Dans Simulink : clic droit sur le chart -> "Update Chart". ===\n');
    fprintf('=== Verifiez ensuite Diagnostic Viewer pour toute erreur residuelle.   ===\n');
end

%% ------------------------------------------------------------------
%  Utilitaires de recherche (idempotence)
%  ------------------------------------------------------------------

function chart = trouverChart(modelName, chartPath)
    rt = sfroot;
    allCharts = rt.find('-isa', 'Stateflow.Chart');
    chart = [];
    fullPath = [modelName '/' chartPath];
    for i = 1:numel(allCharts)
        if strcmp(allCharts(i).Path, fullPath)
            chart = allCharts(i);
            return;
        end
    end
end

function s = trouverEtat(parent, nom)
    % parent : Stateflow.Chart ou Stateflow.State
    s = [];
    enfants = parent.find('-isa', 'Stateflow.State', '-depth', 1);
    for i = 1:numel(enfants)
        if strcmp(strtrim(nomSansActions(enfants(i).LabelString)), nom)
            s = enfants(i);
            return;
        end
    end
end

function n = nomSansActions(labelString)
    % Le LabelString d'un etat est "Nom\nentry: ...\nduring: ...", on ne
    % garde que la 1ere ligne.
    lignes = strsplit(labelString, sprintf('\n'));
    n = strtrim(lignes{1});
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

% --- Logique partagee, inlinee (voir note en tete de fichier) ---
% Utilisee a plusieurs endroits (etats + transitions) plutot que factorisee
% en fonction Stateflow, pour eviter toute dependance a une classe d'API
% indisponible selon la version de MATLAB.

function s = txtFermerGaz()
    s = 'V_H2=uint8(0); V_But=uint8(0); V_Fl_1=uint8(0); V_Fl_2=uint8(0); V_Fl_3=uint8(0); Spark=uint8(0);';
end

function s = txtAppliquerPalier()
    s = 'V_Fl_1=uint8(palier==0); V_Fl_2=uint8(palier==0||palier==1); V_Fl_3=uint8(palier~=3);';
end

function s = txtCalculerSeuils()
    s = ['ECART_MIN=3; if T_cible<=T_init+ECART_MIN, T1_seuil=T_cible; T2_seuil=T_cible; T3_seuil=T_cible; ' ...
         'else delta=T_cible-T_init; T1_seuil=T_init+delta/3; T2_seuil=T_init+2*delta/3; T3_seuil=T_cible; end'];
end

function s = txtGererPalier()
    s = sprintf([ ...
        'demi=Hhyst/2;\n' ...
        'switch palier\n' ...
        '  case 0\n' ...
        '    if T_sec >= T1_seuil + demi, palier = uint8(1); end\n' ...
        '  case 1\n' ...
        '    if T_sec >= T2_seuil + demi\n' ...
        '      palier = uint8(2);\n' ...
        '    elseif T_sec < T1_seuil - demi\n' ...
        '      palier = uint8(0);\n' ...
        '    end\n' ...
        '  case 2\n' ...
        '    if T_sec >= T3_seuil + demi\n' ...
        '      palier = uint8(3);\n' ...
        '    elseif T_sec < T2_seuil - demi\n' ...
        '      palier = uint8(1);\n' ...
        '    end\n' ...
        'end\n' ...
        'if palier ~= 3\n' ...
        '  %s\n' ...
        'end'], txtAppliquerPalier());
end

function transitionExiste = existeTransition(chart, srcNom, dstNom, condition)
    transitionExiste = false;
    trs = chart.find('-isa', 'Stateflow.Transition');
    for i = 1:numel(trs)
        t = trs(i);
        srcOk = isempty(srcNom);
        if ~isempty(t.Source) && ~isempty(srcNom)
            srcOk = strcmp(nomSansActions(t.Source.LabelString), srcNom);
        end
        dstOk = ~isempty(t.Destination) && strcmp(nomSansActions(t.Destination.LabelString), dstNom);
        condOk = isempty(condition) || strcmp(strtrim(t.LabelString), strtrim(condition));
        if srcOk && dstOk && condOk
            transitionExiste = true;
            return;
        end
    end
end

%% ------------------------------------------------------------------
%  1. Donnees manquantes (voir docs/FSM_SIMULINK.md §2.2)
%  ------------------------------------------------------------------

function ajouterDonnees(chart)
    fprintf('--- Donnees ---\n');
    specs = {
        'Btn_Stop',           'Input',  'uint8',  []
        'Seuil_MQ8',          'Input',  'double', []
        'Seuil_MQ6',          'Input',  'double', []
        'Press_H2_Min',       'Input',  'double', []
        'T3_seuil',           'Local',  'double', 0
        'T_init',             'Local',  'double', 25
        'palier',             'Local',  'uint8',  0
        'Source_Active',      'Local',  'uint8',  0
        'raison_purge',       'Local',  'uint8',  0
        'nb_echecs_allumage', 'Local',  'uint16', 0
        'MAX_ECHECS',         'Local',  'uint16', 3
        'Choix_Erreur',       'Local',  'uint8',  0
        'arret_force_duree',  'Local',  'uint8',  0
        'Seuil_Tcap_ON',      'Local',  'double', 55
        'Seuil_Tcap_OFF',     'Local',  'double', 45
        'Marge_Retour_H2',    'Local',  'double', 0.5
        'Temps_Arret_Auto',   'Local',  'double', 300
        'Duree_Max_Cycle',    'Local',  'double', 36000
        'Temps_Purge',        'Local',  'double', 120
        'Temps_Allumage',     'Local',  'double', 4
    };
    for i = 1:size(specs, 1)
        nom = specs{i, 1}; scope = specs{i, 2}; type = specs{i, 3}; valeur = specs{i, 4};
        if ~isempty(trouverDonnee(chart, nom))
            fprintf('  [deja present] %s\n', nom);
            continue;
        end
        d = Stateflow.Data(chart);
        d.Name = nom;
        d.Scope = scope;
        d.DataType = type;
        if ~isempty(valeur)
            d.Props.InitialValue = num2str(valeur);
        end
        fprintf('  [ajoute] %s (%s, %s)\n', nom, scope, type);
    end
end

%% ------------------------------------------------------------------
%  3. Etats simples : entry: uniquement (voir docs/FSM_SIMULINK.md §3.3)
%  ------------------------------------------------------------------

function completerEtatsSimples(chart)
    fprintf('--- Actions des etats simples ---\n');
    fg = txtFermerGaz();
    specs = {
        'ATTENTE_DEMARRAGE', [fg ' PWM_Purge=uint8(0); PWM_Inj=uint8(0); PWM_Ext=uint8(0); Buzzer=uint8(0); Etat_LCD=uint8(0);']
        'MODE_SOLAIRE',      [fg ' PWM_Purge=uint8(0); PWM_Inj=uint8(220); PWM_Ext=uint8(150); Etat_LCD=uint8(2);']
        'PROLONGATION',      'Etat_LCD=uint8(5);'
        'FIN_TEMPORISATION', 'Etat_LCD=uint8(6);'
        'SECHAGE_TERMINE',   [fg ' PWM_Purge=uint8(0); PWM_Inj=uint8(0); PWM_Ext=uint8(90);']
        'URGENCE_ATEX',      [fg ' PWM_Purge=uint8(255); PWM_Inj=uint8(0); PWM_Ext=uint8(255); Buzzer=uint8(1); Etat_LCD=uint8(9);']
    };
    for i = 1:size(specs, 1)
        nom = specs{i, 1}; entryAction = specs{i, 2};
        s = trouverEtat(chart, nom);
        if isempty(s)
            fprintf('  [INTROUVABLE, non modifie] %s -- verifiez le nom exact dans le modele\n', nom);
            continue;
        end
        if contains(s.LabelString, 'entry:')
            fprintf('  [deja une action entry, non ecrasee] %s\n', nom);
            continue;
        end
        s.LabelString = sprintf('%s\nentry: %s', nom, entryAction);
        fprintf('  [entry ajoutee] %s\n', nom);
    end

    % during: sur FONCTIONNEMENT_NORMAL (H_fin recalcule en continu, §5 du
    % CdC), pas sur le chart lui-meme : Stateflow.Chart n'a pas de garantie
    % documentee de supporter LabelString comme un etat -- FONCTIONNEMENT_
    % NORMAL est un vrai Stateflow.State (deja utilise avec succes ailleurs
    % dans ce script) dont le during: s'execute quel que soit le sous-etat
    % actif, ce qui produit exactement le meme effet.
    fn = trouverEtat(chart, 'FONCTIONNEMENT_NORMAL');
    if isempty(fn)
        fprintf('  [FONCTIONNEMENT_NORMAL introuvable, during H_fin non ajoute]\n');
    elseif contains(fn.LabelString, 'during:')
        fprintf('  [during de FONCTIONNEMENT_NORMAL deja present]\n');
    else
        fn.LabelString = sprintf('%s\nduring: H_fin = H_produit + H_amb;', nomSansActions(fn.LabelString));
        fprintf('  [during ajoute sur FONCTIONNEMENT_NORMAL] H_fin = H_produit + H_amb;\n');
    end
end

%% ------------------------------------------------------------------
%  4. Sous-machine de combustion : PURGE -> ALLUMAGE -> REGULATION
%     Identique dans MODE_H2 et MODE_GPL (voir docs/FSM_SIMULINK.md §3.1)
%  ------------------------------------------------------------------

function ajouterSousMachineCombustion(chart, nomParent)
    fprintf('--- Sous-machine de combustion : %s ---\n', nomParent);
    parent = trouverEtat(chart, nomParent);
    if isempty(parent)
        fprintf('  [INTROUVABLE] %s -- rien ajoute\n', nomParent);
        return;
    end

    utiliseH2 = strcmp(nomParent, 'MODE_H2');
    if utiliseH2
        actionVanne = 'V_H2 = uint8(1); V_But = uint8(0);';
    else
        actionVanne = 'V_H2 = uint8(0); V_But = uint8(1);';
    end

    purge = trouverEtatEnfant(parent, 'PURGE');
    if isempty(purge)
        purge = Stateflow.State(parent);
        purge.Position = [20 40 100 60];
        purge.LabelString = sprintf('PURGE\nentry: %s Spark=uint8(0); PWM_Purge=uint8(255); PWM_Inj=uint8(60); PWM_Ext=uint8(200);', txtFermerGaz());
        fprintf('  [ajoute] PURGE\n');
    else
        fprintf('  [deja present] PURGE\n');
    end

    allumage = trouverEtatEnfant(parent, 'ALLUMAGE');
    if isempty(allumage)
        allumage = Stateflow.State(parent);
        allumage.Position = [140 40 100 60];
        allumage.LabelString = sprintf('ALLUMAGE\nentry: %s %s Spark=uint8(1);', actionVanne, txtAppliquerPalier());
        fprintf('  [ajoute] ALLUMAGE\n');
    else
        fprintf('  [deja present] ALLUMAGE\n');
    end

    regulation = trouverEtatEnfant(parent, 'REGULATION');
    if isempty(regulation)
        regulation = Stateflow.State(parent);
        regulation.Position = [260 40 100 60];
        regulation.LabelString = sprintf('REGULATION\nentry: %s\nduring: %s', actionVanne, txtGererPalier());
        fprintf('  [ajoute] REGULATION\n');
    else
        fprintf('  [deja present] REGULATION\n');
    end

    % Transition par defaut vers PURGE (etat initial de la sous-machine)
    if ~existeTransition(parent, '', 'PURGE', '')
        td = Stateflow.Transition(parent);
        td.Destination = purge;
        fprintf('  [transition par defaut] -> PURGE\n');
    end

    % PURGE -> ALLUMAGE
    condPurgeAllumage = ['[after(Temps_Purge, sec) && (raison_purge ~= 3 || T_sec < T3_seuil - Hhyst/2)]' ...
        '{if (raison_purge == 3) { palier = 2; }}'];
    if ~existeTransition(chart, 'PURGE', 'ALLUMAGE', condPurgeAllumage)
        t = Stateflow.Transition(chart);
        t.Source = purge; t.Destination = allumage;
        t.LabelString = condPurgeAllumage;
        fprintf('  [transition] PURGE -> ALLUMAGE\n');
    end

    % ALLUMAGE -> REGULATION (succes)
    condAllumageOk = '[Flame == 1]{Spark=uint8(0); nb_echecs_allumage=uint16(0);}';
    if ~existeTransition(chart, 'ALLUMAGE', 'REGULATION', condAllumageOk)
        t = Stateflow.Transition(chart);
        t.Source = allumage; t.Destination = regulation;
        t.LabelString = condAllumageOk;
        fprintf('  [transition] ALLUMAGE -> REGULATION\n');
    end

    % ALLUMAGE -> ERREUR_COMBUSTION (echec de basculement, direct)
    condEchecBasculement = '[after(Temps_Allumage, sec) && Flame == 0 && raison_purge == 2]';
    creerTransitionVersErreur(chart, allumage, condEchecBasculement, 'AllumageEchecBasculement');

    % ALLUMAGE -> PURGE (echec ordinaire, sous le seuil d''escalade)
    condEchecOrdinaire = ['[after(Temps_Allumage, sec) && Flame == 0 && raison_purge ~= 2 && nb_echecs_allumage < MAX_ECHECS]' ...
        '{nb_echecs_allumage=nb_echecs_allumage+uint16(1); raison_purge=uint8(1);}'];
    if ~existeTransition(chart, 'ALLUMAGE', 'PURGE', condEchecOrdinaire)
        t = Stateflow.Transition(chart);
        t.Source = allumage; t.Destination = purge;
        t.LabelString = condEchecOrdinaire;
        fprintf('  [transition] ALLUMAGE -> PURGE (echec)\n');
    end

    % ALLUMAGE -> ERREUR_COMBUSTION (echecs repetes)
    condEchecRepete = '[after(Temps_Allumage, sec) && Flame == 0 && nb_echecs_allumage >= MAX_ECHECS]{nb_echecs_allumage=nb_echecs_allumage+uint16(1);}';
    creerTransitionVersErreur(chart, allumage, condEchecRepete, 'AllumageEchecRepete');

    % REGULATION -> PURGE (perte de flamme inattendue)
    condPerteFlamme = '[Flame == 0 && nb_echecs_allumage < MAX_ECHECS]{nb_echecs_allumage=nb_echecs_allumage+uint16(1); raison_purge=uint8(1);}';
    if ~existeTransition(chart, 'REGULATION', 'PURGE', condPerteFlamme)
        t = Stateflow.Transition(chart);
        t.Source = regulation; t.Destination = purge;
        t.LabelString = condPerteFlamme;
        fprintf('  [transition] REGULATION -> PURGE (perte de flamme)\n');
    end

    % REGULATION -> ERREUR_COMBUSTION (perte de flamme + echecs repetes)
    condPerteFlammeRepete = '[Flame == 0 && nb_echecs_allumage >= MAX_ECHECS]{nb_echecs_allumage=nb_echecs_allumage+uint16(1);}';
    creerTransitionVersErreur(chart, regulation, condPerteFlammeRepete, 'RegulationPerteFlammeRepete');

    % REGULATION -> PURGE (consigne atteinte : coupure volontaire)
    condCoupureVolontaire = '[palier == 3]{raison_purge=uint8(3);}';
    if ~existeTransition(chart, 'REGULATION', 'PURGE', condCoupureVolontaire)
        t = Stateflow.Transition(chart);
        t.Source = regulation; t.Destination = purge;
        t.LabelString = condCoupureVolontaire;
        fprintf('  [transition] REGULATION -> PURGE (coupure volontaire, palier 0%%)\n');
    end
end

function s = trouverEtatEnfant(parentState, nom)
    s = [];
    enfants = parentState.find('-isa', 'Stateflow.State', '-depth', 1);
    for i = 1:numel(enfants)
        if strcmp(strtrim(nomSansActions(enfants(i).LabelString)), nom)
            s = enfants(i);
            return;
        end
    end
end

%% ------------------------------------------------------------------
%  5. Etat ERREUR_COMBUSTION (nouveau, sibling de premier niveau)
%  ------------------------------------------------------------------

function ajouterEtatErreurCombustion(chart)
    fprintf('--- ERREUR_COMBUSTION ---\n');
    if ~isempty(trouverEtat(chart, 'ERREUR_COMBUSTION'))
        fprintf('  [deja present]\n');
        return;
    end
    fn = trouverEtat(chart, 'FONCTIONNEMENT_NORMAL');
    if isempty(fn)
        fprintf('  [FONCTIONNEMENT_NORMAL introuvable, ERREUR_COMBUSTION non ajoute]\n');
        return;
    end
    err = Stateflow.State(fn);
    err.Position = [700 200 120 70];
    err.LabelString = sprintf('ERREUR_COMBUSTION\nentry: %s Buzzer=uint8(1); PWM_Purge=uint8(255); Choix_Erreur=uint8(0);\nduring: if Btn_SELECT==1, Choix_Erreur = mod(Choix_Erreur+1, uint8(3)); end', txtFermerGaz());
    fprintf('  [ajoute] ERREUR_COMBUSTION\n');

    % Retour vers MODE_H2 ou MODE_GPL (REESSAYER ou AUTOMATIQUE)
    condRetourH2 = '[Btn_OK==1 && (Choix_Erreur==0 || Choix_Erreur==2) && Source_Active==1]{if Choix_Erreur==2, Mode_Auto=uint8(1); end nb_echecs_allumage=uint16(0); raison_purge=uint8(2);}';
    modeH2 = trouverEtat(chart, 'MODE_H2');
    if ~isempty(modeH2) && ~existeTransition(chart, 'ERREUR_COMBUSTION', 'MODE_H2', condRetourH2)
        t = Stateflow.Transition(chart);
        t.Source = err; t.Destination = modeH2;
        t.LabelString = condRetourH2;
        fprintf('  [transition] ERREUR_COMBUSTION -> MODE_H2 (reessayer/auto)\n');
    end

    condRetourGpl = '[Btn_OK==1 && (Choix_Erreur==0 || Choix_Erreur==2) && Source_Active==2]{if Choix_Erreur==2, Mode_Auto=uint8(1); end nb_echecs_allumage=uint16(0); raison_purge=uint8(2);}';
    modeGpl = trouverEtat(chart, 'MODE_GPL');
    if ~isempty(modeGpl) && ~existeTransition(chart, 'ERREUR_COMBUSTION', 'MODE_GPL', condRetourGpl)
        t = Stateflow.Transition(chart);
        t.Source = err; t.Destination = modeGpl;
        t.LabelString = condRetourGpl;
        fprintf('  [transition] ERREUR_COMBUSTION -> MODE_GPL (reessayer/auto)\n');
    end

    condManuel = '[Btn_OK==1 && Choix_Erreur==1]{Mode_Auto=uint8(0); Buzzer=uint8(0);}';
    attente = trouverEtat(chart, 'ATTENTE_DEMARRAGE');
    if ~isempty(attente) && ~existeTransition(chart, 'ERREUR_COMBUSTION', 'ATTENTE_DEMARRAGE', condManuel)
        t = Stateflow.Transition(chart);
        t.Source = err; t.Destination = attente;
        t.LabelString = condManuel;
        fprintf('  [transition] ERREUR_COMBUSTION -> ATTENTE_DEMARRAGE (manuel)\n');
    end

    condStop = sprintf('[Btn_Stop==1]{Buzzer=uint8(0); %s}', txtFermerGaz());
    termine = trouverEtat(chart, 'SECHAGE_TERMINE');
    if ~isempty(termine) && ~existeTransition(chart, 'ERREUR_COMBUSTION', 'SECHAGE_TERMINE', condStop)
        t = Stateflow.Transition(chart);
        t.Source = err; t.Destination = termine;
        t.LabelString = condStop;
        fprintf('  [transition] ERREUR_COMBUSTION -> SECHAGE_TERMINE (Btn_Stop)\n');
    end
end

function creerTransitionVersErreur(chart, source, condition, tag)
    err = trouverEtat(chart, 'ERREUR_COMBUSTION');
    if isempty(err)
        fprintf('    [ERREUR_COMBUSTION pas encore cree, transition "%s" reportee -- relancez le script apres ajouterEtatErreurCombustion]\n', tag);
        return;
    end
    if ~existeTransition(chart, nomSansActions(source.LabelString), 'ERREUR_COMBUSTION', condition)
        t = Stateflow.Transition(chart);
        t.Source = source; t.Destination = err;
        t.LabelString = condition;
        fprintf('    [transition] %s -> ERREUR_COMBUSTION (%s)\n', nomSansActions(source.LabelString), tag);
    end
end

%% ------------------------------------------------------------------
%  6. Transitions prioritaires (bord de FONCTIONNEMENT_NORMAL)
%  ------------------------------------------------------------------

function ajouterTransitionsPrioritaires(chart)
    fprintf('--- Transitions prioritaires ---\n');
    fn = trouverEtat(chart, 'FONCTIONNEMENT_NORMAL');
    urgence = trouverEtat(chart, 'URGENCE_ATEX');
    termine = trouverEtat(chart, 'SECHAGE_TERMINE');
    prolongation = trouverEtat(chart, 'PROLONGATION');
    finTempo = trouverEtat(chart, 'FIN_TEMPORISATION');

    if isempty(fn) || isempty(termine)
        fprintf('  [etats de base introuvables, transitions non ajoutees]\n');
        return;
    end

    % 2. Btn_Stop -> SECHAGE_TERMINE
    % (parenthese d'ouverture manquante corrigee ici : "(in(...)||...||in(...))")
    condStop = sprintf('[(in(MODE_SOLAIRE)||in(MODE_H2)||in(MODE_GPL)||in(PROLONGATION)||in(FIN_TEMPORISATION)) && Btn_Stop==1]{%s}', txtFermerGaz());
    ajouterTransitionBordSiAbsente(chart, fn, termine, condStop, 'Btn_Stop');

    % 3. Surchauffe -> SECHAGE_TERMINE
    condSurchauffe = sprintf('[(in(MODE_SOLAIRE)||in(MODE_H2)||in(MODE_GPL)||in(PROLONGATION)||in(FIN_TEMPORISATION)) && T_sec >= 90]{%s}', txtFermerGaz());
    ajouterTransitionBordSiAbsente(chart, fn, termine, condSurchauffe, 'Surchauffe');

    % 4. Plafond Temps_Prolongation -> SECHAGE_TERMINE (arret force)
    if ~isempty(prolongation)
        condPlafond = sprintf('[after(Tps_Prolongation, sec)]{arret_force_duree=uint8(1); %s}', txtFermerGaz());
        if ~existeTransition(chart, 'PROLONGATION', 'SECHAGE_TERMINE', condPlafond)
            t = Stateflow.Transition(chart);
            t.Source = prolongation; t.Destination = termine;
            t.LabelString = condPlafond;
            fprintf('  [transition] PROLONGATION -> SECHAGE_TERMINE (plafond Tps_Prolongation)\n');
        end
    end

    % 5. Fin de cycle -> FIN_TEMPORISATION
    if ~isempty(finTempo)
        condFin = ['[(in(MODE_SOLAIRE)||in(MODE_H2)||in(MODE_GPL)||in(PROLONGATION)) && ' ...
            'after(Temps_Min_Fin,sec) && (H_sec<=H_fin || (palier==3 && after(60,sec)))]'];
        ajouterTransitionBordSiAbsente(chart, fn, finTempo, condFin, 'FinDeCycle');

        % Confirmation / report / arret auto depuis FIN_TEMPORISATION
        condConfirme = sprintf('[Btn_OK==1 || Btn_Stop==1]{%s}', txtFermerGaz());
        if ~existeTransition(chart, 'FIN_TEMPORISATION', 'SECHAGE_TERMINE', condConfirme)
            t = Stateflow.Transition(chart);
            t.Source = finTempo; t.Destination = termine;
            t.LabelString = condConfirme;
            fprintf('  [transition] FIN_TEMPORISATION -> SECHAGE_TERMINE (confirmation)\n');
        end
        condAutoStop = sprintf('[after(Temps_Arret_Auto, sec)]{%s}', txtFermerGaz());
        if ~existeTransition(chart, 'FIN_TEMPORISATION', 'SECHAGE_TERMINE', condAutoStop)
            t = Stateflow.Transition(chart);
            t.Source = finTempo; t.Destination = termine;
            t.LabelString = condAutoStop;
            fprintf('  [transition] FIN_TEMPORISATION -> SECHAGE_TERMINE (arret automatique)\n');
        end
    end

    % 6. Duree_Max_Cycle -> PROLONGATION
    if ~isempty(prolongation)
        condDureeMax = '[(in(MODE_SOLAIRE)||in(MODE_H2)||in(MODE_GPL)) && after(Duree_Max_Cycle,sec)]';
        ajouterTransitionBordSiAbsente(chart, fn, prolongation, condDureeMax, 'DureeMaxCycle');
    end

    % 7. Retour URGENCE_ATEX -> FONCTIONNEMENT_NORMAL (reste a faire a la
    %    main dans Simulink : une transition NE PEUT PAS cibler l'interieur
    %    d'un OR-state depuis l'exterieur sans preciser un etat par defaut
    %    d'entree, ce qui est deja gere par la transition par defaut
    %    existante SSID 31 -> ATTENTE_DEMARRAGE. On peut donc cibler
    %    directement FONCTIONNEMENT_NORMAL.
    if ~isempty(urgence)
        condRearm = '[Btn_OK==1 && MQ8_H2<Seuil_MQ8 && MQ6_But<Seuil_MQ6 && AU_Manuel==0]{Buzzer=uint8(0);}';
        if ~existeTransition(chart, 'URGENCE_ATEX', 'FONCTIONNEMENT_NORMAL', condRearm)
            t = Stateflow.Transition(chart);
            t.Source = urgence; t.Destination = fn;
            t.LabelString = condRearm;
            fprintf('  [transition] URGENCE_ATEX -> FONCTIONNEMENT_NORMAL (rearmement)\n');
        end
    end
end

function ajouterTransitionBordSiAbsente(chart, srcState, dstState, condition, tag)
    if ~existeTransition(chart, nomSansActions(srcState.LabelString), nomSansActions(dstState.LabelString), condition)
        t = Stateflow.Transition(chart);
        t.Source = srcState; t.Destination = dstState;
        t.LabelString = condition;
        fprintf('  [transition] %s -> %s (%s)\n', nomSansActions(srcState.LabelString), nomSansActions(dstState.LabelString), tag);
    else
        fprintf('  [deja presente] %s\n', tag);
    end
end

%% ------------------------------------------------------------------
%  7. Transitions d'arbitrage de source (voir docs/FSM_SIMULINK.md §4.1)
%  ------------------------------------------------------------------

function ajouterTransitionsArbitrage(chart)
    fprintf('--- Arbitrage de source ---\n');
    attente = trouverEtat(chart, 'ATTENTE_DEMARRAGE');
    solaire = trouverEtat(chart, 'MODE_SOLAIRE');
    h2 = trouverEtat(chart, 'MODE_H2');
    gpl = trouverEtat(chart, 'MODE_GPL');

    if any(cellfun(@isempty, {attente, solaire, h2, gpl}))
        fprintf('  [un ou plusieurs etats introuvables, arbitrage non ajoute]\n');
        return;
    end

    % Depuis ATTENTE_DEMARRAGE : manuel (Choix_Manuel==3, GPL, manquant)
    condManuelGpl = '[Btn_Start==1 && Mode_Auto==0 && Choix_Manuel==3]{palier=uint8(0); raison_purge=uint8(0);}';
    ajouterTransitionSiAbsente(chart, attente, gpl, condManuelGpl, 'ManuelGPL');

    % Depuis ATTENTE_DEMARRAGE : automatique
    cs = txtCalculerSeuils();
    condAutoSolaire = sprintf('[Btn_Start==1 && Mode_Auto==1 && T_cap>=Seuil_Tcap_ON]{T_init=T_sec; %s}', cs);
    ajouterTransitionSiAbsente(chart, attente, solaire, condAutoSolaire, 'AutoSolaire');

    condAutoH2 = sprintf('[Btn_Start==1 && Mode_Auto==1 && T_cap<Seuil_Tcap_OFF && Press_H2>=Press_H2_Min]{T_init=T_sec; %s palier=uint8(0); raison_purge=uint8(0);}', cs);
    ajouterTransitionSiAbsente(chart, attente, h2, condAutoH2, 'AutoH2');

    condAutoGpl = sprintf('[Btn_Start==1 && Mode_Auto==1 && T_cap<Seuil_Tcap_OFF && Press_H2<Press_H2_Min]{T_init=T_sec; %s palier=uint8(0); raison_purge=uint8(0);}', cs);
    ajouterTransitionSiAbsente(chart, attente, gpl, condAutoGpl, 'AutoGPL');

    % Retour au solaire depuis H2/GPL
    condRetourSolaire = '[Mode_Auto==1 && T_cap>=Seuil_Tcap_ON]';
    ajouterTransitionSiAbsente(chart, h2, solaire, condRetourSolaire, 'H2VersSolaire');
    ajouterTransitionSiAbsente(chart, gpl, solaire, condRetourSolaire, 'GPLVersSolaire');

    % Solaire insuffisant -> combustion (demarrage a froid)
    condSolaireVersH2 = '[Mode_Auto==1 && T_cap<Seuil_Tcap_OFF && Press_H2>=Press_H2_Min]{palier=uint8(0); raison_purge=uint8(0);}';
    ajouterTransitionSiAbsente(chart, solaire, h2, condSolaireVersH2, 'SolaireVersH2');
    condSolaireVersGpl = '[Mode_Auto==1 && T_cap<Seuil_Tcap_OFF && Press_H2<Press_H2_Min]{palier=uint8(0); raison_purge=uint8(0);}';
    ajouterTransitionSiAbsente(chart, solaire, gpl, condSolaireVersGpl, 'SolaireVersGPL');

    % Bascule H2 <-> GPL (palier CONSERVE, raison_purge=2)
    condH2VersGpl = '[Mode_Auto==1 && Press_H2<Press_H2_Min]{raison_purge=uint8(2);}';
    ajouterTransitionSiAbsente(chart, h2, gpl, condH2VersGpl, 'H2VersGPL');
    condGplVersH2 = '[Mode_Auto==1 && Press_H2>=Press_H2_Min+Marge_Retour_H2]{raison_purge=uint8(2);}';
    ajouterTransitionSiAbsente(chart, gpl, h2, condGplVersH2, 'GPLVersH2');
end

function ajouterTransitionSiAbsente(chart, srcState, dstState, condition, tag)
    if ~existeTransition(chart, nomSansActions(srcState.LabelString), nomSansActions(dstState.LabelString), condition)
        t = Stateflow.Transition(chart);
        t.Source = srcState; t.Destination = dstState;
        t.LabelString = condition;
        fprintf('  [transition] %s -> %s (%s)\n', nomSansActions(srcState.LabelString), nomSansActions(dstState.LabelString), tag);
    else
        fprintf('  [deja presente] %s\n', tag);
    end
end
