%% exporter_etude.m  —  écrit un cas d'étude au format texte du banc programme
%
%   exporter_etude(sc, fichier)
%
% Format lu par tests/simulation_etudes.cpp : celui de exporter_scenario,
% plus les lignes « param et_<nom> <valeur> » des réglages de sc.etude.

function exporter_etude(sc, fichier)

    fid = fopen(fichier, 'w');
    if fid < 0
        error('exporter_etude:fichier', 'Impossible d''ecrire %s', fichier);
    end
    fprintf(fid, 'StopTime %.10g\nTsec0 %.10g\nH0 %.10g\n', sc.StopTime, sc.Tsec0, sc.H0);
    p = fieldnames(sc.param);
    for i = 1:numel(p)
        fprintf(fid, 'param %s %.10g\n', p{i}, sc.param.(p{i}));
    end
    e = fieldnames(sc.etude);
    for i = 1:numel(e)
        fprintf(fid, 'param et_%s %.10g\n', e{i}, sc.etude.(e{i}));
    end
    s = fieldnames(sc.signaux);
    for i = 1:numel(s)
        m = sc.signaux.(s{i});
        c = [repmat(s(i), 1, size(m, 1)); num2cell(m')];
        fprintf(fid, 'signal %s %.10g %.10g\n', c{:});
    end
    fclose(fid);
end
