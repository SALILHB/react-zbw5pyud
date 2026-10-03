%% capturer_chart_lisible.m  —  vues lisibles du chart Stateflow pour le mémoire (images A1 à A4)
%
%   capturer_chart_lisible
%   capturer_chart_lisible(modele)      (défaut : 'Simulation_Sechoir_Hybride')
%
% Travaille sur une COPIE TEMPORAIRE du modèle (le fichier d'origine n'est
% jamais modifié) et écrit dans captures/memoire/ :
%
%   chart_hierarchie.png     A1  chart complet : libellés des états réduits à
%                                leur nom, transitions sans texte (structure)
%   chart_mode_h2.png        A2  zoom sur MODE_H2 : PURGE -> ALLUMAGE -> REGULATION,
%                                conditions des transitions (sans leurs actions)
%   chart_region_phase.png   A3  zoom sur la région PHASE
%   chart_urgence.png        A4  zoom sur URGENCE_ATEX et le réarmement
%   modele_global.png        ré-export sans aperçu miniature du chart
%   modele_fsm.png           (qui masquait des noms de ports)
%
% Chaque étape affiche [ok] ou l'erreur exacte ; une étape qui échoue
% n'empêche pas les suivantes. Version cible : MATLAB R2025b.

function capturer_chart_lisible(modele)

    if nargin < 1
        modele = 'Simulation_Sechoir_Hybride';
    end
    dossier = fileparts(mfilename('fullpath'));
    sortie = fullfile(dossier, 'captures', 'memoire');
    if ~exist(sortie, 'dir')
        mkdir(sortie);
    end
    copie = 'Chart_Lisible_Tmp';
    fichierCopie = fullfile(tempdir, [copie '.slx']);

    fprintf('=== Vues lisibles du chart (copie temporaire %s) ===\n', copie);
    if bdIsLoaded(copie)
        close_system(copie, 0);
    end
    if bdIsLoaded(modele)
        close_system(modele, 0);   % la copie part du fichier enregistré
    end
    copyfile(fullfile(dossier, [modele '.slx']), fichierCopie, 'f');
    load_system(fichierCopie);
    nettoyage = onCleanup(@() fermerCopie(copie, fichierCopie)); %#ok<NASGU>

    fsm = [copie '/FSM'];
    chartPath = [fsm '/Chart'];
    chart = find(sfroot, '-isa', 'Stateflow.Chart', 'Path', chartPath);
    if isempty(chart)
        fprintf('  [ERREUR] chart %s introuvable\n', chartPath);
        return;
    end

    % --- Modèle global et sous-système FSM sans aperçu miniature du chart
    etape('apercu miniature desactive', @() desactiverApercu({fsm, chartPath}));
    etape('modele_global.png', @() exporterSysteme(copie, fullfile(sortie, 'modele_global.png')));
    etape('modele_fsm.png', @() exporterSysteme(fsm, fullfile(sortie, 'modele_fsm.png')));

    etats = chart.find('-isa', 'Stateflow.State');
    transitions = chart.find('-isa', 'Stateflow.Transition');

    % --- Libellés des états réduits à leur nom (toutes les vues)
    for i = 1:numel(etats)
        etats(i).LabelString = nomEtat(etats(i));
    end
    % --- Vues détaillées : conditions des transitions sans leurs actions {...}
    for i = 1:numel(transitions)
        transitions(i).LabelString = conditionSeule(transitions(i).LabelString);
    end
    open_system(chartPath);

    etape('chart_mode_h2.png', @() exporterZoom(chart, trouver(etats, 'MODE_H2'), ...
        fullfile(sortie, 'chart_mode_h2.png')));
    etape('chart_region_phase.png', @() exporterZoom(chart, trouver(etats, 'PHASE'), ...
        fullfile(sortie, 'chart_region_phase.png')));
    etape('chart_urgence.png', @() exporterZoom(chart, trouver(etats, 'URGENCE_ATEX'), ...
        fullfile(sortie, 'chart_urgence.png')));

    % --- Vue d'ensemble : transitions sans texte, structure seule
    for i = 1:numel(transitions)
        transitions(i).LabelString = '';
    end
    etape('chart_hierarchie.png', @() sfprint(chart.Path, 'png', ...
        fullfile(sortie, 'chart_hierarchie.png'), 1));
    fprintf('=== Termine : images dans %s ===\n', sortie);
    fprintf('Le modele d''origine n''a pas ete modifie. Rouvrez-le si besoin :\n');
    fprintf('  open_system(''%s'')\n', modele);
end

%% ---------------------------------------------------------------------
function etape(nom, action)
    try
        action();
        fprintf('  [ok] %s\n', nom);
    catch e
        fprintf('  [ERREUR] %s : %s\n', nom, e.message);
    end
end

function desactiverApercu(blocs)
    for i = 1:numel(blocs)
        set_param(blocs{i}, 'ContentPreviewEnabled', 'off');
    end
end

function exporterSysteme(sys, fichier)
    open_system(sys);
    try
        print(['-s' sys], '-dpng', '-r300', fichier);
    catch
        saveas(get_param(sys, 'Handle'), fichier, 'png');
    end
end

function exporterZoom(chart, etat, fichier)
    % Vue courante de l'éditeur, cadrée sur l'état (et ce qu'il contient).
    if isempty(etat)
        error('etat introuvable');
    end
    etat.fitToView;
    drawnow;
    pause(0.5);
    sfprint(chart.Path, 'png', fichier, 0);
end

function s = trouver(etats, nom)
    s = [];
    for i = 1:numel(etats)
        if strcmp(nomEtat(etats(i)), nom)
            s = etats(i);
            return;
        end
    end
end

function n = nomEtat(s)
    lignes = strsplit(s.LabelString, sprintf('\n'));
    n = strtrim(lignes{1});
end

function c = conditionSeule(label)
    % "[condition]{action}" -> "[condition]" ; libellé sans condition inchangé.
    c = regexprep(label, '\{[^}]*\}\s*$', '');
end

function fermerCopie(copie, fichierCopie)
    if bdIsLoaded(copie)
        close_system(copie, 0);
    end
    if exist(fichierCopie, 'file')
        delete(fichierCopie);
    end
end
