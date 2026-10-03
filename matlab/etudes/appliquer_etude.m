%% appliquer_etude.m  —  charge un cas d'étude dans le modèle Etudes_Sechoir
%
%   appliquer_etude(modele, sc)
%
% appliquer_scenario (signaux sc_*, paramètres du chart), puis les réglages
% des études lus par les blocs ajoutés par construire_modele_etudes :
% et_UA, et_Ceq, et_retard, et_quantif, et_m0, et_eta, et_PCI_H2.

function appliquer_etude(modele, sc)

    sc = completer_etude(sc);
    appliquer_scenario(modele, sc);
    noms = {'UA', 'Ceq', 'retard', 'quantif', 'm0', 'eta', 'PCI_H2'};
    for i = 1:numel(noms)
        assignin('base', ['et_' noms{i}], sc.etude.(noms{i}));
    end
end
