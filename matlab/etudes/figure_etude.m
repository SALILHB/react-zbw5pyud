%% figure_etude.m  —  figure au format du mémoire (taille réelle en cm)
%
%   fig = figure_etude(largeurCm, hauteurCm)
%
% Police de 9 pt : lisible une fois la figure insérée à 15 cm de large.

function fig = figure_etude(largeurCm, hauteurCm)

    fig = figure('Color', 'w', 'WindowStyle', 'normal', 'Units', 'centimeters', ...
                 'Position', [2 2 largeurCm hauteurCm]);
    set(fig, 'DefaultAxesFontSize', 9, 'DefaultTextFontSize', 9);
    try
        set(fig, 'DefaultLegendFontSize', 9);
    catch
        % propriété absente sous Octave : la légende suit la police des axes
    end
end
