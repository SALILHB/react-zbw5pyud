%% completer_etude.m  —  ajoute à un scénario les réglages des études E1 à E4
%
%   sc = completer_etude(sc)
%
% Champs ajoutés s'ils manquent (valeurs par défaut = modèle validé) :
%   sc.etude.UA        conductance de la chambre (W/K)          1/0,098
%   sc.etude.Ceq       capacité thermique équivalente (J/K)    3530/0,098
%   sc.etude.retard    retard de la mesure de T_sec (s)         0
%   sc.etude.quantif   1 = mesure quantifiée à 0,0625 °C        0
%   sc.etude.m0        stock d'hydrogène (g), 0 = illimité      0
%   sc.etude.eta       rendement du brûleur (hypothèse)         0,80
%   sc.etude.PCI_H2    PCI de l'hydrogène (kWh/kg)              33,3
%   sc.etude.PCI_GPL   PCI du butane (kWh/kg)                   12,7
%   sc.etude.log       pas d'enregistrement du banc programme   10 s
%   sc.signaux.UA_facteur   multiplicateur de UA (porte ouverte : 10)
%   sc.signaux.Bruit_T      bruit ajouté à la mesure de T_sec (°C)
%   sc.signaux.GPL_dispo    0 = bouteille de GPL vide

function sc = completer_etude(sc)

    defauts = struct('UA', 1 / 0.098, 'Ceq', 3530 / 0.098, 'retard', 0, 'quantif', 0, ...
                     'm0', 0, 'eta', 0.80, 'PCI_H2', 33.3, 'PCI_GPL', 12.7, 'log', 10);
    if ~isfield(sc, 'etude')
        sc.etude = struct();
    end
    noms = fieldnames(defauts);
    for i = 1:numel(noms)
        if ~isfield(sc.etude, noms{i})
            sc.etude.(noms{i}) = defauts.(noms{i});
        end
    end
    signaux = {'UA_facteur', 1; 'Bruit_T', 0; 'GPL_dispo', 1};
    for i = 1:size(signaux, 1)
        if ~isfield(sc.signaux, signaux{i, 1})
            sc.signaux.(signaux{i, 1}) = [0 signaux{i, 2}; 1 signaux{i, 2}];
        end
    end
end
