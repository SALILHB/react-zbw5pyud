%% heure_jour.m  —  instant du cycle (s) écrit en heure de la journée
%
%   s = heure_jour(t)            « 12 h 40 » (t = 0 à 8 h 00)
%   s = heure_jour(t, h0)        autre heure de départ

function s = heure_jour(t, h0)

    if nargin < 2
        h0 = 8;
    end
    if isnan(t)
        s = '--';
        return;
    end
    m = round(h0 * 60 + t / 60);
    s = sprintf('%d h %02d', floor(m / 60), mod(m, 60));
end
