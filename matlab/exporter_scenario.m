%% exporter_scenario.m  —  écrit un scénario au format texte du banc firmware
%
%   exporter_scenario(n, fichier)
%
% Format lu par tests/simulation_scenarios.cpp (firmware réel + même modèle
% physique que Simulink) pour produire les courbes de référence :
%   StopTime <s> | Tsec0 <°C> | H0 <%> | param <Nom> <valeur> | signal <Nom> <t> <valeur>

function exporter_scenario(n, fichier)

    sc = scenario_sechoir(n);
    fid = fopen(fichier, 'w');
    if fid < 0
        error('exporter_scenario:fichier', 'Impossible d''ecrire %s', fichier);
    end
    fprintf(fid, 'StopTime %.10g\nTsec0 %.10g\nH0 %.10g\n', sc.StopTime, sc.Tsec0, sc.H0);
    p = fieldnames(sc.param);
    for i = 1:numel(p)
        fprintf(fid, 'param %s %.10g\n', p{i}, sc.param.(p{i}));
    end
    s = fieldnames(sc.signaux);
    for i = 1:numel(s)
        m = sc.signaux.(s{i});
        for k = 1:size(m, 1)
            fprintf(fid, 'signal %s %.10g %.10g\n', s{i}, m(k, 1), m(k, 2));
        end
    end
    fclose(fid);
end
