%% nombre_fr.m  —  nombre écrit avec une virgule décimale (tableaux du mémoire)
%
%   s = nombre_fr(x, nDecimales)

function s = nombre_fr(x, nDecimales)

    if nargin < 2
        nDecimales = 2;
    end
    if isnan(x)
        s = '--';
        return;
    end
    s = strrep(sprintf(sprintf('%%.%df', nDecimales), x), '.', ',');
    if strcmp(s(1), '-') && all(s(2:end) == '0' | s(2:end) == ',')
        s = s(2:end);   % pas de « -0,00 »
    end
end
