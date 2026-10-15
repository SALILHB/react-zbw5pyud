%% retracer_scenarios.m  —  retrace les figures de scénario depuis les .mat, sans Simulink
%
%   retracer_scenarios            tous les captures/scenario_NN.mat présents
%   retracer_scenarios(1:12)
%
% Recharge captures/scenario_NN.mat et réécrit captures/scenario_NN.png avec
% la mise en forme actuelle de tracer_scenario (par exemple la légende
% « flamme (échelle ×15) »), sans relancer la simulation.

function retracer_scenarios(numeros)

    dossier = fileparts(mfilename('fullpath'));
    if nargin < 1
        numeros = 1:16;
    end
    for n = numeros
        fichier = fullfile(dossier, 'captures', sprintf('scenario_%02d.mat', n));
        if ~exist(fichier, 'file')
            continue;
        end
        d = load(fichier);
        fig = tracer_scenario(d.r);
        png = fullfile(dossier, 'captures', sprintf('scenario_%02d.png', n));
        exporter_figure(fig, png);
        close(fig);
        fprintf('  [ok] %s\n', png);
    end
end
