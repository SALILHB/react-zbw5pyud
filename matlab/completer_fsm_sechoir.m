%% completer_fsm_sechoir.m
%
% Complete le chart Stateflow "FSM/Chart" du modele Commande_Sechoir_Hybride
% via l'API Stateflow officielle (Stateflow.State, Stateflow.Transition,
% Stateflow.Data) -- aucune edition manuelle du XML.
%
% ============================================================================
% REVISION MAJEURE (suite a verification point par point face au firmware
% Arduino reel sechoir_hybride.ino, seule source de verite en cas de conflit) :
%
%   1. PROLONGATION et FIN_TEMPORISATION etaient des OR-siblings de
%      MODE_SOLAIRE/MODE_H2/MODE_GPL sous FONCTIONNEMENT_NORMAL : y entrer
%      arretait de facto la regulation de combustion (contradiction avec
%      etapeRegulation()/gererCombustion(), appelees SANS synchronisation
%      d'etat mais SANS interruption depuis ETAT_PROLONGATION et
%      ETAT_FIN_TEMPORISATION dans le firmware reel). Corrige par une
%      RESTRUCTURATION EN DEUX REGIONS PARALLELES (AND-state) sous
%      FONCTIONNEMENT_NORMAL :
%        - SOURCE : MODE_SOLAIRE / MODE_H2 / MODE_GPL / ERREUR_COMBUSTION
%          (qui source est active, avec sa sous-machine PURGE/ALLUMAGE/
%          REGULATION -- inchangee)
%        - PHASE  : NORMAL / PROLONGATION / FIN_TEMPORISATION (ce qui est
%          AFFICHE/confirme, independamment de la source)
%      Ceci reproduit fidelement etapeRegulation(bool synchroniser) : la
%      regulation (region SOURCE) continue integralement pendant que PHASE
%      bascule NORMAL <-> PROLONGATION <-> FIN_TEMPORISATION.
%      RISQUE ACCEPTE ET SIGNALE : Decomposition='PARALLEL_AND' et le
%      reparentage programmatique (.Parent=) de MODE_SOLAIRE/MODE_H2/
%      MODE_GPL/PROLONGATION/FIN_TEMPORISATION n'ont PAS ete testes dans une
%      vraie session MATLAB. Si l'un des deux echoue, le script l'indique
%      clairement et propose un repli manuel (glisser-deposer dans
%      l'editeur Stateflow, cf. ajouterStructureParallele()) -- relancer
%      ensuite le script (idempotent) complete le reste automatiquement.
%      Alternative envisagee et ECARTEE : dupliquer entierement PURGE/
%      ALLUMAGE/REGULATION (+ l'arbitrage solaire/H2/GPL) dans PROLONGATION
%      ET dans FIN_TEMPORISATION. Ecartee car le firmware confirme
%      (arbitrageSource() appele sans condition dans etapeRegulation()) que
%      Source_Active peut valoir SRC_SOLAIRE pendant PROLONGATION/
%      FIN_TEMPORISATION : la duplication aurait du reproduire l'integralite
%      de la machine SOURCE (solaire+H2+GPL) DEUX FOIS DE PLUS, un risque de
%      divergence bien plus grave qu'une primitive Stateflow non verifiee.
%
%   2. URGENCE_ATEX etait uniquement modelise en SORTIE (transition de
%      rearmement) : AUCUNE transition n'entrait dans cet etat. Dans le
%      firmware, causeUrgencePresente() est evaluee a CHAQUE iteration quel
%      que soit etat_courant (priorite absolue, §1 des transitions
%      prioritaires). Ajoute : transitions d'entree depuis ATTENTE_DEMARRAGE,
%      FONCTIONNEMENT_NORMAL (couvre tout le cycle, y compris
%      ERREUR_COMBUSTION en tant que descendant) et SECHAGE_TERMINE. Si un
%      etat CONFIG_MENU existe dans le modele de base (non gere par ce
%      script), ajoutez-y la meme garde a la main.
%
%   3. Le rearmement URGENCE_ATEX ciblait FONCTIONNEMENT_NORMAL ; le firmware
%      (case ETAT_URGENCE_ATEX) revient en realite a ETAT_ATTENTE_DEMARRAGE
%      (l'operateur doit rappuyer sur Start). Corrige. Le nom de variable
%      AU_Manuel etait probablement une erreur de frappe pour AU_Urgence
%      (nom reel utilise par causeUrgencePresente()) -- corrige, mais A
%      VERIFIER contre les ports d'entree reels du sous-systeme FSM.
%
%   4. Unites : Config.Duree_Max_Cycle et Config.Temps_Min_Fin sont en
%      MINUTES dans le firmware (multiplies par 60000UL), comme
%      Config.Temps_Prolongation deja identifie precedemment -- toutes les
%      trois utilisent maintenant after(Xxx*60,sec). Temps_Purge,
%      Temps_Allumage et Temps_Arret_Auto sont deja en secondes (*1000UL) :
%      inchanges.
%
%   5. Corrections ponctuelles deja identifiees lors d'une revue croisee :
%      decompte des echecs d'allumage hors-par-un (le firmware incremente
%      PUIS teste >= MAX -- le script testait < MAX AVANT d'incrementer,
%      d'ou une tentative de trop) ; transition REGULATION -> ERREUR_
%      COMBUSTION pour "perte de flamme repetee" supprimee (code mort dans
%      le firmware LUI-MEME : nb_echecs_allumage est toujours remis a 0 en
%      entrant en regulation, donc ce garde ne peut jamais etre vrai des la
%      premiere perte de flamme -- verifie par trace complete du compteur) ;
%      raison_erreur_combustion desormais suivie (Local) et utilisee par
%      REESSAYER/AUTOMATIQUE pour choisir PURGE_BASCULEMENT (2) vs
%      PURGE_ECHEC (1), au lieu de forcer 2 dans tous les cas ; ecriture
%      dans Mode_Auto (Input, illegal) remplacee par un local Mode_Auto_Eff
%      resynchronise depuis Mode_Auto a chaque entree dans ATTENTE_
%      DEMARRAGE ; detection de front sur Btn_SELECT dans le during: de
%      ERREUR_COMBUSTION (sinon defilement en boucle tant que le bouton est
%      maintenu) ; zone morte de demarrage a froid T_cap in [OFF,ON)
%      supprimee (demarrerCycle() n'utilise QUE le seuil ON, jamais OFF, au
%      demarrage -- l'hysterese ON/OFF ne s'applique qu'en cours de cycle
%      via arbitrageSource()) ; calcul de T_init/seuils centralise dans
%      l'entry: de SOURCE (une seule fois par cycle, quelle que soit la
%      transition de demarrage empruntee -- y compris les 2 transitions
%      manuelles PREEXISTANTES du modele de base, non modifiees par ce
%      script) plutot que reimplemente/oublie sur chaque transition
%      individuelle.
%
%   6. FIN_TEMPORISATION : ajout du "report" (Btn_Menu -> auto-transition sur
%      elle-meme, qui reinitialise le chronometre after(Temps_Arret_Auto,sec)
%      via la sortie/reentree Stateflow standard) et de l'annulation
%      automatique si la condition de fin redevient fausse (retour a
%      NORMAL) -- les deux existent dans le firmware et manquaient
%      entierement du modele.
%
%   7. Bug latent corrige dans existeTransition() : la recherche d'une
%      transition par defaut (srcNom == '') ou sans condition (condition ==
%      '') matchait a tort N'IMPORTE QUELLE transition vers la meme
%      destination, a cause d'un isempty(srcNom) mal place. Sans danger pour
%      les defauts deja crees par ce script (ordre de creation favorable en
%      pratique), mais aurait pu bloquer silencieusement la creation du
%      defaut SOURCE -> MODE_SOLAIRE (obligatoire pour un etat OR) sur un
%      re-run apres que des transitions d'arbitrage existent deja.
%
% CE SCRIPT N'A PAS ETE EXECUTE NI VERIFIE DANS UNE VRAIE SESSION MATLAB
% (environnement de developpement sans MATLAB/Simulink). Executez-le avec
% le modele deja ouvert. Chaque sous-fonction est idempotente (relancer le
% script ne duplique pas ce qui existe deja) -- c'est le mecanisme prevu pour
% completer la structure parallele si le reparentage automatique echoue et
% doit etre fait a la main entre deux executions.
%
% Prerequis avant d'executer :
%   1. Ouvrir Commande_Sechoir_Hybride.slx dans Simulink.
%   2. Recabler manuellement les entrees manquantes du sous-systeme "FSM"
%      (voir docs/FSM_SIMULINK.md, dictionnaire de donnees).
%   3. Executer ce script (F5).
%   4. Si le message "[STRUCTURE INCOMPLETE]" apparait : dans l'editeur
%      Stateflow, glisser-deposer MODE_SOLAIRE / MODE_H2 / MODE_GPL dans
%      SOURCE, et PROLONGATION / FIN_TEMPORISATION dans PHASE, puis relancer
%      le script.
%   5. Dans Simulink : clic droit sur le chart -> "Update Chart".
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

    % La structure parallele doit exister AVANT ERREUR_COMBUSTION (qui se
    % parente desormais sous SOURCE, pas sous FONCTIONNEMENT_NORMAL) et
    % avant les transitions prioritaires 5/6 (qui se sourcent depuis NORMAL/
    % PROLONGATION). Si elle echoue (reparentage automatique impossible),
    % les etapes suivantes qui en dependent s'auto-reportent proprement.
    structInfo = ajouterStructureParallele(chart);

    % ERREUR_COMBUSTION doit exister AVANT les sous-machines de combustion :
    % celles-ci y creent des transitions (echec de basculement, echecs
    % repetes) et echoueraient silencieusement si l'etat n'existait pas
    % encore.
    ajouterEtatErreurCombustion(chart, structInfo.source);
    ajouterSousMachineCombustion(chart, 'MODE_H2');
    ajouterSousMachineCombustion(chart, 'MODE_GPL');
    ajouterTransitionsPrioritaires(chart, structInfo);
    ajouterTransitionsArbitrage(chart);

    fprintf('\n=== Termine. Dans Simulink : clic droit sur le chart -> "Update Chart". ===\n');
    fprintf('=== Verifiez ensuite Diagnostic Viewer pour toute erreur residuelle.   ===\n');
    if ~structInfo.ok
        fprintf(['\n*** ACTION REQUISE *** La structure parallele SOURCE/PHASE est\n' ...
                 'incomplete (voir messages "[STRUCTURE INCOMPLETE]" / "[ECHEC\n' ...
                 'reparentage...]" ci-dessus). Terminez le glisser-deposer indique,\n' ...
                 'puis relancez completer_fsm_sechoir : le script est idempotent et\n' ...
                 'completera ce qui manque encore (ERREUR_COMBUSTION, transitions\n' ...
                 'de fin de cycle / Duree_Max_Cycle).\n']);
    end
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
    % Recherche RECURSIVE (pas de restriction de profondeur) : necessaire
    % depuis la restructuration en regions paralleles, ou MODE_SOLAIRE /
    % MODE_H2 / MODE_GPL / PROLONGATION / FIN_TEMPORISATION peuvent se
    % trouver a des profondeurs differentes selon l'etat d'avancement du
    % script (avant/apres reparentage sous SOURCE / PHASE). Les noms d'etats
    % de premier niveau du FSM etant tous uniques, une recherche globale ne
    % cree pas d'ambiguite (contrairement a trouverEtatEnfant, utilisee pour
    % PURGE/ALLUMAGE/REGULATION qui existent a l'identique sous MODE_H2 ET
    % sous MODE_GPL).
    s = [];
    enfants = parent.find('-isa', 'Stateflow.State');
    for i = 1:numel(enfants)
        if strcmp(strtrim(nomSansActions(enfants(i).LabelString)), nom)
            s = enfants(i);
            return;
        end
    end
end

function s = trouverEtatEnfant(parentState, nom)
    % Recherche restreinte aux enfants DIRECTS (depth 1) : utilisee quand le
    % nom peut exister plusieurs fois dans le chart (PURGE/ALLUMAGE/
    % REGULATION sous MODE_H2 ET sous MODE_GPL ; NORMAL sous PHASE).
    s = [];
    enfants = parentState.find('-isa', 'Stateflow.State', '-depth', 1);
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

function ok = reparenter(state, newParent)
    % Deplace un etat existant sous un nouveau parent. Tente d'abord la
    % propriete MCOS .Parent (documentee pour la construction programmatique
    % de charts) puis, en repli, l'API historique sf('set', id, '.parent',
    % ...) si la premiere echoue sur cette version de MATLAB.
    ok = false;
    try
        state.Parent = newParent; %#ok<NASGU>
        ok = true;
        return;
    catch errMcos
        try
            sf('set', state.Id, '.parent', newParent.Id);
            ok = true;
            return;
        catch errSf
            fprintf('    [ECHEC reparentage automatique de "%s" -- %s | repli sf(): %s]\n', ...
                nomSansActions(state.LabelString), errMcos.message, errSf.message);
        end
    end
end

function b = estDescendantDe(state, ancestor)
    prefixe = [ancestor.Path '/'];
    b = strncmp(state.Path, prefixe, length(prefixe));
end

% --- Logique partagee, inlinee (voir historique dans les commentaires
%     d'en-tete : Stateflow.EMLFunction indisponible sur certaines versions
%     de MATLAB) ---

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
    % Corrige (voir note d'en-tete §7) : la version precedente traitait a
    % tort srcNom=='' / condition=='' comme des jokers "n'importe quoi" au
    % lieu de "explicitement vide" (transition par defaut / sans garde).
    transitionExiste = false;
    trs = chart.find('-isa', 'Stateflow.Transition');
    for i = 1:numel(trs)
        t = trs(i);
        if isempty(srcNom)
            srcOk = isempty(t.Source);
        else
            srcOk = ~isempty(t.Source) && strcmp(nomSansActions(t.Source.LabelString), srcNom);
        end
        dstOk = ~isempty(t.Destination) && strcmp(nomSansActions(t.Destination.LabelString), dstNom);
        if isempty(condition)
            condOk = isempty(strtrim(t.LabelString));
        else
            condOk = strcmp(strtrim(t.LabelString), strtrim(condition));
        end
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
        'Btn_Rearm',          'Input',  'uint8',  []
        'AU_Urgence',         'Input',  'uint8',  []
        'Seuil_MQ8',          'Input',  'double', []
        'Seuil_MQ6',          'Input',  'double', []
        'Press_H2_Min',       'Input',  'double', []
        'T_init_saisie',      'Input',  'double', []
        'T3_seuil',           'Local',  'double', 0
        'T_init',             'Local',  'double', 25
        'palier',             'Local',  'uint8',  0
        'Source_Active',      'Local',  'uint8',  0
        'raison_purge',       'Local',  'uint8',  0
        'nb_echecs_allumage', 'Local',  'uint16', 0
        'MAX_ECHECS',         'Local',  'uint16', 3
        'Choix_Erreur',       'Local',  'uint8',  0
        'raison_erreur_combustion', 'Local', 'uint8', 1
        'Mode_Auto_Eff',      'Local',  'uint8',  0
        'Btn_SELECT_prev',    'Local',  'uint8',  0
        'palier0_stable',     'Local',  'uint8',  0
        'arret_force_duree',  'Local',  'uint8',  0
        'Seuil_Tcap_ON',      'Local',  'double', 55
        'Seuil_Tcap_OFF',     'Local',  'double', 45
        'T_SEC_MAX_SECURITE', 'Local',  'double', 90
        'Marge_Retour_H2',    'Local',  'double', 0.5
        'Temps_Arret_Auto',   'Local',  'double', 300
        'Duree_Max_Cycle',    'Local',  'double', 600
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
    fprintf(['  NOTE : Duree_Max_Cycle (minutes, defaut 600=10h) et Temps_Min_Fin\n' ...
             '  (suppose preexistant dans le modele de base, en minutes comme dans\n' ...
             '  Config.Temps_Min_Fin du firmware) sont utilises via after(X*60,sec)\n' ...
             '  -- verifiez l''unite reelle de Temps_Min_Fin si elle est deja cablee.\n']);
end

%% ------------------------------------------------------------------
%  2. Etats simples : entry: uniquement (voir docs/FSM_SIMULINK.md §3.3)
%  ------------------------------------------------------------------

function completerEtatsSimples(chart)
    fprintf('--- Actions des etats simples ---\n');
    fg = txtFermerGaz();
    specs = {
        'ATTENTE_DEMARRAGE', [fg ' PWM_Purge=uint8(0); PWM_Inj=uint8(0); PWM_Ext=uint8(0); Buzzer=uint8(0); Etat_LCD=uint8(0); Mode_Auto_Eff=Mode_Auto;']
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
    % CdC) : un during: sur un etat AND-superstate s'execute a chaque pas
    % quelle que soit la combinaison de sous-etats actifs dans SES DEUX
    % regions paralleles -- effet inchange par la restructuration.
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
%  3. Structure parallele EN_CYCLE = FONCTIONNEMENT_NORMAL (SOURCE || PHASE)
%  ------------------------------------------------------------------

function info = ajouterStructureParallele(chart)
    fprintf('--- Structure parallele SOURCE / PHASE ---\n');
    info = struct('ok', false, 'source', [], 'phase', [], 'normal', []);

    fn = trouverEtat(chart, 'FONCTIONNEMENT_NORMAL');
    if isempty(fn)
        fprintf('  [FONCTIONNEMENT_NORMAL introuvable, structure non ajoutee]\n');
        return;
    end

    source = trouverEtatEnfant(fn, 'SOURCE');
    if isempty(source)
        source = Stateflow.State(fn);
        source.Position = [20 20 420 220];
        source.LabelString = 'SOURCE';
        fprintf('  [ajoute] SOURCE (region parallele)\n');
    else
        fprintf('  [deja present] SOURCE\n');
    end

    phase = trouverEtatEnfant(fn, 'PHASE');
    if isempty(phase)
        phase = Stateflow.State(fn);
        phase.Position = [460 20 260 220];
        phase.LabelString = 'PHASE';
        fprintf('  [ajoute] PHASE (region parallele)\n');
    else
        fprintf('  [deja present] PHASE\n');
    end

    try
        if ~strcmp(fn.Decomposition, 'PARALLEL_AND')
            fn.Decomposition = 'PARALLEL_AND';
            fprintf('  [FONCTIONNEMENT_NORMAL.Decomposition = PARALLEL_AND]\n');
        else
            fprintf('  [Decomposition deja PARALLEL_AND]\n');
        end
    catch e
        fprintf(['  [ATTENTION] Impossible de regler Decomposition=PARALLEL_AND (%s).\n' ...
                 '  A faire a la main : clic droit sur FONCTIONNEMENT_NORMAL dans\n' ...
                 '  l''editeur Stateflow -> Decomposition -> "AND (parallel)".\n'], e.message);
    end

    % Deplacement des 5 etats existants dans SOURCE / PHASE.
    aDeplacer = {
        'MODE_SOLAIRE',      source
        'MODE_H2',           source
        'MODE_GPL',          source
        'PROLONGATION',      phase
        'FIN_TEMPORISATION', phase
    };
    tousEnPlace = true;
    for i = 1:size(aDeplacer, 1)
        nom = aDeplacer{i, 1}; cible = aDeplacer{i, 2};
        s = trouverEtat(chart, nom);
        if isempty(s)
            fprintf('  [INTROUVABLE] %s\n', nom);
            tousEnPlace = false;
            continue;
        end
        if estDescendantDe(s, cible)
            continue; % deja au bon endroit
        end
        if reparenter(s, cible)
            fprintf('  [deplace] %s -> %s\n', nom, nomSansActions(cible.LabelString));
        else
            tousEnPlace = false;
        end
    end

    if ~tousEnPlace
        fprintf(['  [STRUCTURE INCOMPLETE] Le reparentage automatique a echoue pour au\n' ...
                 '  moins un etat (voir "[ECHEC reparentage...]" ci-dessus), ou un etat\n' ...
                 '  est introuvable. Dans l''editeur Stateflow, glissez-deposez a la main :\n' ...
                 '  MODE_SOLAIRE / MODE_H2 / MODE_GPL -> dans SOURCE ; PROLONGATION /\n' ...
                 '  FIN_TEMPORISATION -> dans PHASE. Relancez ensuite ce script\n' ...
                 '  (idempotent) pour completer le reste.\n']);
        return;
    end

    % NORMAL : etat vide, place "par defaut" de PHASE (etat_courant in
    % {SOLAIRE,H2,GPL} du point de vue du firmware).
    normal = trouverEtatEnfant(phase, 'NORMAL');
    if isempty(normal)
        normal = Stateflow.State(phase);
        normal.Position = [20 20 100 60];
        normal.LabelString = 'NORMAL';
        fprintf('  [ajoute] NORMAL (dans PHASE)\n');
    end

    % Transitions par defaut, obligatoires pour tout etat a decomposition OR.
    modeSolaire = trouverEtat(chart, 'MODE_SOLAIRE');
    if ~isempty(modeSolaire) && ~existeTransition(chart, '', 'MODE_SOLAIRE', '')
        t = Stateflow.Transition(source);
        t.Destination = modeSolaire;
        fprintf('  [transition par defaut] SOURCE -> MODE_SOLAIRE\n');
    end
    if ~existeTransition(chart, '', 'NORMAL', '')
        t = Stateflow.Transition(phase);
        t.Destination = normal;
        fprintf('  [transition par defaut] PHASE -> NORMAL\n');
    end

    % entry: sur SOURCE -- T_init/seuils/palier calcules UNE SEULE FOIS par
    % cycle : SOURCE n'est (re)activee qu'au Btn_Start (transition venant de
    % l'exterieur de FONCTIONNEMENT_NORMAL), jamais lors d'un basculement
    % H2<->GPL ni d'une reprise depuis ERREUR_COMBUSTION, ces transitions
    % restant internes a la region SOURCE. Couvre aussi bien les demarrages
    % automatiques que manuels (y compris les 2 transitions manuelles
    % preexistantes du modele de base, non modifiees par ce script).
    entrySource = sprintf([ ...
        'if (Mode_Auto_Eff==0) { T_init=T_init_saisie; } else { T_init=T_sec; }\n' ...
        '%s\n' ...
        'palier=uint8(0); raison_purge=uint8(0); nb_echecs_allumage=uint16(0);'], txtCalculerSeuils());
    if ~contains(source.LabelString, 'entry:')
        source.LabelString = sprintf('SOURCE\nentry: %s', entrySource);
        fprintf('  [entry ajoutee] SOURCE (T_init/seuils/palier, une fois par cycle)\n');
    else
        fprintf('  [entry de SOURCE deja presente]\n');
    end

    info.ok = true;
    info.source = source;
    info.phase = phase;
    info.normal = normal;
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
        purge.LabelString = sprintf([ ...
            'PURGE\n' ...
            'entry: %s Spark=uint8(0); PWM_Purge=uint8(255); PWM_Inj=uint8(60); PWM_Ext=uint8(200); palier0_stable=uint8(0);\n' ...
            'during: if (raison_purge==3 && after(60,sec)) { palier0_stable=uint8(1); }'], txtFermerGaz());
        fprintf('  [ajoute] PURGE\n');
    else
        fprintf('  [deja present] PURGE\n');
    end

    allumage = trouverEtatEnfant(parent, 'ALLUMAGE');
    if isempty(allumage)
        allumage = Stateflow.State(parent);
        allumage.Position = [140 40 100 60];
        allumage.LabelString = sprintf('ALLUMAGE\nentry: %s %s Spark=uint8(1); palier0_stable=uint8(0);', actionVanne, txtAppliquerPalier());
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
    if ~existeTransition(chart, '', 'PURGE', '')
        td = Stateflow.Transition(parent);
        td.Destination = purge;
        fprintf('  [transition par defaut] -> PURGE\n');
    end

    % PURGE -> ALLUMAGE
    condPurgeAllumage = ['[after(Temps_Purge, sec) && (raison_purge ~= 3 || T_sec < T3_seuil - Hhyst/2)]' ...
        '{if (raison_purge == 3) { palier = uint8(2); } }'];
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

    % ALLUMAGE -> ERREUR_COMBUSTION (echec de basculement, direct, jamais
    % rebouclee silencieusement -- §13).
    condEchecBasculement = '[after(Temps_Allumage, sec) && Flame == 0 && raison_purge == 2]{raison_erreur_combustion=uint8(0);}';
    creerTransitionVersErreur(chart, allumage, condEchecBasculement, 'AllumageEchecBasculement');

    % ALLUMAGE -> PURGE (echec ordinaire, sous le seuil d'escalade).
    % Corrige (hors-par-un) : le firmware incremente PUIS teste >= MAX ; on
    % compare donc ici (compteur+1) au seuil AVANT d'incrementer, pour
    % escalader sur le MEME echec (le 3e avec MAX_ECHECS=3) que le firmware.
    condEchecOrdinaire = ['[after(Temps_Allumage, sec) && Flame == 0 && raison_purge ~= 2 && (nb_echecs_allumage+uint16(1)) < MAX_ECHECS]' ...
        '{nb_echecs_allumage=nb_echecs_allumage+uint16(1); raison_purge=uint8(1);}'];
    if ~existeTransition(chart, 'ALLUMAGE', 'PURGE', condEchecOrdinaire)
        t = Stateflow.Transition(chart);
        t.Source = allumage; t.Destination = purge;
        t.LabelString = condEchecOrdinaire;
        fprintf('  [transition] ALLUMAGE -> PURGE (echec)\n');
    end

    % ALLUMAGE -> ERREUR_COMBUSTION (echecs repetes)
    condEchecRepete = ['[after(Temps_Allumage, sec) && Flame == 0 && (nb_echecs_allumage+uint16(1)) >= MAX_ECHECS]' ...
        '{nb_echecs_allumage=nb_echecs_allumage+uint16(1); raison_erreur_combustion=uint8(1);}'];
    creerTransitionVersErreur(chart, allumage, condEchecRepete, 'AllumageEchecRepete');

    % REGULATION -> PURGE (perte de flamme inattendue, tentative de
    % reprise). nb_echecs_allumage vaut TOUJOURS 0 en entrant fraichement en
    % REGULATION (remis a 0 par condAllumageOk) : ce garde-ci est donc
    % systematiquement vrai des la premiere perte de flamme -- coherent avec
    % le firmware, ou le meme raisonnement rend le CAS SYMETRIQUE
    % "escalade directe depuis la regulation" INATTEIGNABLE (voir ci-dessous).
    condPerteFlamme = '[Flame == 0 && (nb_echecs_allumage+uint16(1)) < MAX_ECHECS]{nb_echecs_allumage=nb_echecs_allumage+uint16(1); raison_purge=uint8(1);}';
    if ~existeTransition(chart, 'REGULATION', 'PURGE', condPerteFlamme)
        t = Stateflow.Transition(chart);
        t.Source = regulation; t.Destination = purge;
        t.LabelString = condPerteFlamme;
        fprintf('  [transition] REGULATION -> PURGE (perte de flamme)\n');
    end

    % REGULATION -> ERREUR_COMBUSTION ("perte de flamme repetee") : SUPPRIME
    % (etait du code mort). Preuve : nb_echecs_allumage est remis a 0 par
    % l'UNIQUE transition qui mene a REGULATION (ALLUMAGE -> REGULATION,
    % condAllumageOk) ; une premiere perte de flamme ne peut donc jamais
    % l'amener directement a MAX_ECHECS (>= 2) des ce premier incident, et
    % toute reprise ulterieure repasse par ALLUMAGE (qui gere lui-meme
    % l'escalade). Cette propriete est verifiee a l'identique dans le
    % firmware (gererCombustion(), case PH_PALIER_*), ce n'est pas une
    % particularite introduite par la traduction Stateflow.

    % REGULATION -> PURGE (consigne atteinte : coupure volontaire)
    condCoupureVolontaire = '[palier == 3]{raison_purge=uint8(3);}';
    if ~existeTransition(chart, 'REGULATION', 'PURGE', condCoupureVolontaire)
        t = Stateflow.Transition(chart);
        t.Source = regulation; t.Destination = purge;
        t.LabelString = condCoupureVolontaire;
        fprintf('  [transition] REGULATION -> PURGE (coupure volontaire, palier 0%%)\n');
    end
end

%% ------------------------------------------------------------------
%  5. Etat ERREUR_COMBUSTION (sibling de MODE_SOLAIRE/H2/GPL, dans SOURCE)
%  ------------------------------------------------------------------

function ajouterEtatErreurCombustion(chart, parentSource)
    fprintf('--- ERREUR_COMBUSTION ---\n');
    if ~isempty(trouverEtat(chart, 'ERREUR_COMBUSTION'))
        fprintf('  [deja present]\n');
        return;
    end
    if isempty(parentSource)
        fprintf(['  [SOURCE indisponible (structure parallele incomplete) -- ERREUR_\n' ...
                 '   COMBUSTION reporte, relancez le script apres correction]\n']);
        return;
    end
    err = Stateflow.State(parentSource);
    err.Position = [280 140 130 70];
    err.LabelString = sprintf([ ...
        'ERREUR_COMBUSTION\n' ...
        'entry: %s Buzzer=uint8(1); PWM_Purge=uint8(255); Choix_Erreur=uint8(0);\n' ...
        'during: if (Btn_SELECT==1 && Btn_SELECT_prev==0) { Choix_Erreur = mod(Choix_Erreur+uint8(1), uint8(3)); }\n' ...
        'Btn_SELECT_prev = Btn_SELECT;'], txtFermerGaz());
    fprintf('  [ajoute] ERREUR_COMBUSTION (sous SOURCE)\n');

    % Retour vers MODE_H2 ou MODE_GPL (REESSAYER ou AUTOMATIQUE). raison_purge
    % reprend PURGE_BASCULEMENT (2) si l'erreur venait d'un echec de
    % basculement, sinon PURGE_ECHEC (1) -- au lieu de forcer 2 dans tous les
    % cas (ce qui privait l'operateur d'un jeu complet de MAX_ECHECS
    % tentatives apres un echec d'allumage ordinaire).
    condRetourH2 = ['[Btn_OK==1 && (Choix_Erreur==0 || Choix_Erreur==2) && Source_Active==1]' ...
        '{if (Choix_Erreur==2) { Mode_Auto_Eff=uint8(1); } ' ...
        'nb_echecs_allumage=uint16(0); ' ...
        'if (raison_erreur_combustion==0) { raison_purge=uint8(2); } else { raison_purge=uint8(1); }}'];
    modeH2 = trouverEtat(chart, 'MODE_H2');
    if ~isempty(modeH2) && ~existeTransition(chart, 'ERREUR_COMBUSTION', 'MODE_H2', condRetourH2)
        t = Stateflow.Transition(chart);
        t.Source = err; t.Destination = modeH2;
        t.LabelString = condRetourH2;
        fprintf('  [transition] ERREUR_COMBUSTION -> MODE_H2 (reessayer/auto)\n');
    end

    condRetourGpl = ['[Btn_OK==1 && (Choix_Erreur==0 || Choix_Erreur==2) && Source_Active==2]' ...
        '{if (Choix_Erreur==2) { Mode_Auto_Eff=uint8(1); } ' ...
        'nb_echecs_allumage=uint16(0); ' ...
        'if (raison_erreur_combustion==0) { raison_purge=uint8(2); } else { raison_purge=uint8(1); }}'];
    modeGpl = trouverEtat(chart, 'MODE_GPL');
    if ~isempty(modeGpl) && ~existeTransition(chart, 'ERREUR_COMBUSTION', 'MODE_GPL', condRetourGpl)
        t = Stateflow.Transition(chart);
        t.Source = err; t.Destination = modeGpl;
        t.LabelString = condRetourGpl;
        fprintf('  [transition] ERREUR_COMBUSTION -> MODE_GPL (reessayer/auto)\n');
    end

    % MANUEL : sortie complete du cycle (fn tout entier), retour a
    % ATTENTE_DEMARRAGE -- ecrit desormais Mode_Auto_Eff (local), jamais
    % Mode_Auto (Input, illegal en ecriture).
    condManuel = '[Btn_OK==1 && Choix_Erreur==1]{Mode_Auto_Eff=uint8(0); Buzzer=uint8(0);}';
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
%  6. Transitions prioritaires (voir §19 v2 du CdC et pasFSM() du firmware)
%  ------------------------------------------------------------------

function ajouterTransitionsPrioritaires(chart, structInfo)
    fprintf('--- Transitions prioritaires ---\n');
    fn = trouverEtat(chart, 'FONCTIONNEMENT_NORMAL');
    urgence = trouverEtat(chart, 'URGENCE_ATEX');
    termine = trouverEtat(chart, 'SECHAGE_TERMINE');
    attente = trouverEtat(chart, 'ATTENTE_DEMARRAGE');
    prolongation = trouverEtat(chart, 'PROLONGATION');
    finTempo = trouverEtat(chart, 'FIN_TEMPORISATION');

    if any(cellfun(@isempty, {fn, urgence, termine, attente}))
        fprintf('  [etats de base introuvables, transitions non ajoutees]\n');
        return;
    end

    % 1. Entree URGENCE_ATEX -- priorite absolue, evaluee a chaque
    %    iteration quel que soit etat_courant dans le firmware
    %    (causeUrgencePresente()). MANQUAIT ENTIEREMENT du modele
    %    precedent (seul le rearmement etait cable, l'etat etait donc
    %    inatteignable). Couvre ATTENTE_DEMARRAGE, tout le cycle via
    %    FONCTIONNEMENT_NORMAL (qui couvre ERREUR_COMBUSTION en tant que
    %    descendant) et SECHAGE_TERMINE. Si un etat CONFIG_MENU existe
    %    dans le modele de base, ajoutez-y la meme garde a la main.
    condEntreeUrgence = '[MQ8_H2>Seuil_MQ8 || MQ6_But>Seuil_MQ6 || AU_Urgence==1]';
    ajouterTransitionBordSiAbsente(chart, attente, urgence, condEntreeUrgence, 'EntreeATEX(ATTENTE_DEMARRAGE)');
    ajouterTransitionBordSiAbsente(chart, fn, urgence, condEntreeUrgence, 'EntreeATEX(cycle)');
    ajouterTransitionBordSiAbsente(chart, termine, urgence, condEntreeUrgence, 'EntreeATEX(SECHAGE_TERMINE)');

    % 2. Btn_Stop -> SECHAGE_TERMINE. Source = fn : couvre tout le cycle.
    %    FIN_TEMPORISATION et ERREUR_COMBUSTION gerent Btn_Stop localement
    %    (transitions internes, plus profondes dans la hierarchie) : par les
    %    regles standard de priorite Stateflow (transition la plus interne
    %    gagne), elles priment automatiquement sur celle-ci sans qu'il soit
    %    necessaire de les exclure explicitement par une garde in(...).
    condStop = sprintf('[Btn_Stop==1]{%s}', txtFermerGaz());
    ajouterTransitionBordSiAbsente(chart, fn, termine, condStop, 'Btn_Stop');

    % 3. Surchauffe -> SECHAGE_TERMINE (meme remarque sur la priorite).
    condSurchauffe = '[T_sec>=T_SEC_MAX_SECURITE]';
    condSurchauffe = sprintf('%s{%s}', condSurchauffe, txtFermerGaz());
    ajouterTransitionBordSiAbsente(chart, fn, termine, condSurchauffe, 'Surchauffe');

    % 4. Plafond Temps_Prolongation -> SECHAGE_TERMINE (arret force). Source
    %    = PROLONGATION lui-meme : inchange par la restructuration parallele
    %    (le chronometre local mesure bien depuis l'entree en PROLONGATION,
    %    comme chrono_prolongation dans le firmware). Unite : minutes dans
    %    Config, d'ou *60.
    if ~isempty(prolongation)
        condPlafond = sprintf('[after(Temps_Prolongation*60, sec)]{arret_force_duree=uint8(1); %s}', txtFermerGaz());
        if ~existeTransition(chart, 'PROLONGATION', 'SECHAGE_TERMINE', condPlafond)
            t = Stateflow.Transition(chart);
            t.Source = prolongation; t.Destination = termine;
            t.LabelString = condPlafond;
            fprintf('  [transition] PROLONGATION -> SECHAGE_TERMINE (plafond Temps_Prolongation)\n');
        end
    end

    if ~structInfo.ok
        fprintf(['  [transitions 5/6 (fin de cycle, Duree_Max_Cycle) et le report/\n' ...
                 '   annulation de FIN_TEMPORISATION en dependent -- REPORTES : completez\n' ...
                 '   d''abord la structure parallele SOURCE/PHASE puis relancez.]\n']);
    else
        normal = structInfo.normal;

        % 5. Fin de cycle -> FIN_TEMPORISATION. Deux transitions internes a
        %    PHASE (source = NORMAL, source = PROLONGATION) : une transition
        %    sourcee sur fn lui-meme aurait force la sortie/reentree des
        %    DEUX regions paralleles (perte de l'etat de SOURCE), ce qui
        %    contredit directement le point 1 de cette revision.
        if ~isempty(finTempo)
            condFinDepuisNormal = '[after(Temps_Min_Fin*60,sec) && (H_sec<=H_fin || palier0_stable==1)]';
            ajouterTransitionBordSiAbsente(chart, normal, finTempo, condFinDepuisNormal, 'FinDeCycle(NORMAL)');

            % Depuis PROLONGATION : Duree_Max_Cycle (deja ecoule pour y
            % arriver) est suppose configure >= Temps_Min_Fin (invariant a
            % faire respecter cote firmware/bornerConfig si ce n'est pas
            % deja le cas) -- la condition Temps_Min_Fin est donc deja
            % necessairement vraie ; la re-tester ici mesurerait a tort
            % depuis l'entree en PROLONGATION et non depuis le vrai debut de
            % cycle (chrono_cycle dans le firmware).
            if ~isempty(prolongation)
                condFinDepuisProlongation = '[H_sec<=H_fin || palier0_stable==1]';
                ajouterTransitionBordSiAbsente(chart, prolongation, finTempo, condFinDepuisProlongation, 'FinDeCycle(PROLONGATION)');
            end

            % Report (Btn_Menu) : auto-transition qui reinitialise le
            % chronometre after(Temps_Arret_Auto,sec) via la sortie/reentree
            % standard de Stateflow -- equivalent exact de
            % "chrono_arret_auto = t_boucle;" dans le firmware.
            condReport = '[Btn_Menu==1]';
            if ~existeTransition(chart, 'FIN_TEMPORISATION', 'FIN_TEMPORISATION', condReport)
                t = Stateflow.Transition(chart);
                t.Source = finTempo; t.Destination = finTempo;
                t.LabelString = condReport;
                fprintf('  [transition] FIN_TEMPORISATION -> FIN_TEMPORISATION (report Btn_Menu)\n');
            end

            % Annulation automatique si la condition de fin redevient
            % fausse (ex. H_sec remonte transitoirement) -- retour a NORMAL,
            % PAS a MODE_SOLAIRE/H2/GPL directement : la region SOURCE suit
            % deja independamment sa propre source active.
            condAnnulation = '[H_sec>H_fin && palier0_stable==0]';
            ajouterTransitionBordSiAbsente(chart, finTempo, normal, condAnnulation, 'AnnulationFinTempo');

            % Confirmation / arret automatique -> SECHAGE_TERMINE (inchange,
            % deja sourcees sur finTempo lui-meme, exit legitime hors de fn).
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

        % 6. Duree_Max_Cycle -> PROLONGATION. Source = NORMAL uniquement
        %    (jamais fn) : ne touche pas la region SOURCE. Unite minutes,
        %    d'ou *60.
        if ~isempty(prolongation)
            condDureeMax = '[after(Duree_Max_Cycle*60,sec)]';
            ajouterTransitionBordSiAbsente(chart, normal, prolongation, condDureeMax, 'DureeMaxCycle');
        end
    end

    % 7. Rearmement URGENCE_ATEX -> ATTENTE_DEMARRAGE (CORRIGE : ciblait a
    %    tort FONCTIONNEMENT_NORMAL ; le firmware repart de zero, l'operateur
    %    doit rappuyer sur Start). Double chemin Btn_OK / Btn_Rearm (cf.
    %    demande_rearm_ihm || frontPris(B_REARM) dans le firmware). Variable
    %    renommee AU_Manuel -> AU_Urgence pour matcher causeUrgencePresente()
    %    (A VERIFIER contre les ports d'entree reels du sous-systeme).
    condRearm = ['[(Btn_OK==1 || Btn_Rearm==1) && MQ8_H2<=Seuil_MQ8 && MQ6_But<=Seuil_MQ6 && AU_Urgence==0]' ...
        '{Buzzer=uint8(0); palier=uint8(0); raison_purge=uint8(0);}'];
    if ~existeTransition(chart, 'URGENCE_ATEX', 'ATTENTE_DEMARRAGE', condRearm)
        t = Stateflow.Transition(chart);
        t.Source = urgence; t.Destination = attente;
        t.LabelString = condRearm;
        fprintf('  [transition] URGENCE_ATEX -> ATTENTE_DEMARRAGE (rearmement, cible corrigee)\n');
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
%  7. Transitions d'arbitrage de source (voir docs/FSM_SIMULINK.md §4.1 et
%     arbitrageSource()/demarrerCycle() du firmware)
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

    % Manuel (Choix_Manuel==3 -> GPL). NOTE : les 2 transitions manuelles
    % preexistantes du modele de base (solaire/H2, non creees par ce
    % script) ne sont PAS modifiees ici -- le calcul de T_init/seuils est
    % desormais centralise dans l'entry: de SOURCE, qui s'applique quelle
    % que soit la transition de demarrage empruntee. Si ces 2 transitions
    % testent encore "Mode_Auto==0" (variable d'entree brute) au lieu de
    % "Mode_Auto_Eff==0", corrigez-les a la main (residu signale, non
    % corrige automatiquement : ce script ne touche jamais une transition
    % qu'il n'a pas lui-meme creee).
    condManuelGpl = '[Btn_Start==1 && Mode_Auto_Eff==0 && Choix_Manuel==3]';
    ajouterTransitionSiAbsente(chart, attente, gpl, condManuelGpl, 'ManuelGPL');

    % Automatique -- demarrage a froid : SEUIL_Tcap_ON uniquement, comme
    % demarrerCycle() qui n'utilise JAMAIS Seuil_Tcap_OFF au demarrage
    % (CORRIGE : la version precedente creait une zone morte [OFF,ON) ou
    % Btn_Start ne produisait aucune transition).
    condAutoSolaire = '[Btn_Start==1 && Mode_Auto_Eff==1 && T_cap>=Seuil_Tcap_ON]';
    ajouterTransitionSiAbsente(chart, attente, solaire, condAutoSolaire, 'AutoSolaire');

    condAutoH2 = '[Btn_Start==1 && Mode_Auto_Eff==1 && T_cap<Seuil_Tcap_ON && Press_H2>=Press_H2_Min]';
    ajouterTransitionSiAbsente(chart, attente, h2, condAutoH2, 'AutoH2');

    condAutoGpl = '[Btn_Start==1 && Mode_Auto_Eff==1 && T_cap<Seuil_Tcap_ON && Press_H2<Press_H2_Min]';
    ajouterTransitionSiAbsente(chart, attente, gpl, condAutoGpl, 'AutoGPL');

    % Retour au solaire depuis H2/GPL -- priorite 1 dans arbitrageSource()
    % (verifiee avant le bascule H2<->GPL) ; l'ordre de creation des
    % transitions ci-dessous (retour-solaire avant bascule H2<->GPL)
    % reproduit cette priorite via l'ordre par defaut de Stateflow. A
    % verifier dans le Diagnostic Viewer si un conflit d'ordre apparait.
    condRetourSolaire = '[Mode_Auto_Eff==1 && T_cap>=Seuil_Tcap_ON]{raison_purge=uint8(0);}';
    ajouterTransitionSiAbsente(chart, h2, solaire, condRetourSolaire, 'H2VersSolaire');
    ajouterTransitionSiAbsente(chart, gpl, solaire, condRetourSolaire, 'GPLVersSolaire');

    % Solaire insuffisant -> combustion (demarrage a froid, palier 100%).
    condSolaireVersH2 = '[Mode_Auto_Eff==1 && T_cap<Seuil_Tcap_OFF && Press_H2>=Press_H2_Min]{palier=uint8(0); raison_purge=uint8(0);}';
    ajouterTransitionSiAbsente(chart, solaire, h2, condSolaireVersH2, 'SolaireVersH2');
    condSolaireVersGpl = '[Mode_Auto_Eff==1 && T_cap<Seuil_Tcap_OFF && Press_H2<Press_H2_Min]{palier=uint8(0); raison_purge=uint8(0);}';
    ajouterTransitionSiAbsente(chart, solaire, gpl, condSolaireVersGpl, 'SolaireVersGPL');

    % Bascule H2 <-> GPL (palier CONSERVE, raison_purge=2=bascule)
    condH2VersGpl = '[Mode_Auto_Eff==1 && Press_H2<Press_H2_Min]{raison_purge=uint8(2);}';
    ajouterTransitionSiAbsente(chart, h2, gpl, condH2VersGpl, 'H2VersGPL');
    condGplVersH2 = '[Mode_Auto_Eff==1 && Press_H2>=Press_H2_Min+Marge_Retour_H2]{raison_purge=uint8(2);}';
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
