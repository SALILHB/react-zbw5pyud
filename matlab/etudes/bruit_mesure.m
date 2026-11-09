%% bruit_mesure.m  —  bruit de mesure reproductible (étude E4)
%
%   m = bruit_mesure(amplitude, duree)
%
% Signal [t valeur] : une valeur par seconde, uniforme dans
% [-amplitude, +amplitude]. Suite pseudo-aléatoire FIXE (fonction de hachage
% sur l'indice), identique sous MATLAB et sous Octave : les deux moteurs de
% simulation voient exactement le même bruit.

function m = bruit_mesure(amplitude, duree)

    k = (0:ceil(duree))';
    u = mod(sin(k * 12.9898 + 78.233) * 43758.5453, 1);   % dans [0, 1[
    m = [k, amplitude * (2 * u - 1)];
end
