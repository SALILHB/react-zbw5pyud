%% appliquer_scenario.m  —  charge un scénario dans le modèle de simulation
%
%   appliquer_scenario(modele, sc)
%
% - crée dans l'espace de travail de base les variables lues par les blocs :
%   sc_<Nom> pour chaque signal de sc.signaux, sc_H0 et Tsec0 ;
% - écrit sc.param dans la valeur initiale des paramètres Local du chart ;
% - règle la durée de simulation (Stop Time) sur sc.StopTime, pour pouvoir
%   aussi lancer la simulation avec le bouton Run.

function appliquer_scenario(modele, sc)

    noms = fieldnames(sc.signaux);
    for i = 1:numel(noms)
        assignin('base', ['sc_' noms{i}], sc.signaux.(noms{i}));
    end
    assignin('base', 'sc_H0', sc.H0);
    assignin('base', 'Tsec0', sc.Tsec0);
    set_param(modele, 'StopTime', num2str(sc.StopTime));

    chart = find(sfroot, '-isa', 'Stateflow.Chart', 'Path', [modele '/FSM/Chart']);
    if isempty(chart)
        error('appliquer_scenario:chart', 'Chart %s/FSM/Chart introuvable.', modele);
    end
    params = fieldnames(sc.param);
    for i = 1:numel(params)
        d = chart.find('-isa', 'Stateflow.Data', 'Name', params{i});
        if isempty(d)
            fprintf('  [ATTENTION] parametre %s absent du chart\n', params{i});
            continue;
        end
        d(1).Props.InitialValue = num2str(sc.param.(params{i}));
    end
end
