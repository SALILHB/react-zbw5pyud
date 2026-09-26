%% tracer_references.m  —  figures des résultats attendus (référence firmware)
%
%   tracer_references()        les 12 scénarios
%   tracer_references(n)
%
% Trace reference/scenario_NN.csv avec la même mise en forme que les figures
% Simulink et enregistre reference/scenario_NN_firmware.png. Fonctionne dans
% MATLAB et dans GNU Octave.

function tracer_references(numeros)

    if nargin < 1
        numeros = 1:12;
    end
    dossier = fullfile(fileparts(mfilename('fullpath')), 'reference');
    for n = numeros
        r = charger_reference(n);
        fig = tracer_scenario(r);
        fichier = fullfile(dossier, sprintf('scenario_%02d_firmware.png', n));
        print(fig, fichier, '-dpng', '-r150');
        close(fig);
        fprintf('  [ok] %s\n', fichier);
    end
end
