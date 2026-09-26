%% exporter_figure.m  —  enregistre une figure en PNG 300 dpi pour le mémoire
%
%   exporter_figure(fig, fichier)
%
% Masque la barre d'outils des axes (sinon visible sur l'image exportée sous
% MATLAB R2025), termine le rendu, puis exporte.

function exporter_figure(fig, fichier)
    axes_fig = findall(fig, 'Type', 'axes');
    for i = 1:numel(axes_fig)
        try
            axes_fig(i).Toolbar.Visible = 'off';
        catch
            % propriété absente (Octave, anciennes versions) : sans conséquence
        end
    end
    drawnow;
    if ~isempty(which('exportgraphics'))
        try   % marge autour de la figure : titre non rogné
            exportgraphics(fig, fichier, 'Resolution', 300, 'Padding', 20);
        catch
            exportgraphics(fig, fichier, 'Resolution', 300);
        end
    else
        print(fig, fichier, '-dpng', '-r300');
    end
end
