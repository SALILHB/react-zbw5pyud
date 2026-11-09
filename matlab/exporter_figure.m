%% exporter_figure.m  —  enregistre une figure en PNG 300 dpi pour le mémoire
%
%   exporter_figure(fig, fichier)
%
% Masque la barre d'outils des axes (sinon visible sur l'image exportée sous
% MATLAB R2025), termine le rendu, puis exporte. Si l'image obtenue est
% noire (raté du rendu graphique, observé sous R2025b), elle est exportée de
% nouveau avec le moteur vectoriel « painters ».

function exporter_figure(fig, fichier)
    axes_fig = findall(fig, 'Type', 'axes');
    for i = 1:numel(axes_fig)
        try
            axes_fig(i).Toolbar.Visible = 'off';
        catch
            % propriété absente (Octave, anciennes versions) : sans conséquence
        end
    end
    figure(fig);   % fenêtre au premier plan : rendu complet avant l'export
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
    if imageNoire(fichier)
        fprintf('  [attention] image noire : nouvel export (painters) de %s\n', fichier);
        print(fig, fichier, '-dpng', '-r300', '-painters');
        if imageNoire(fichier)
            fprintf('  [ERREUR] %s reste noire : relancez retracer_scenarios ou envoyez-moi ce message\n', fichier);
        end
    end
end

function noire = imageNoire(fichier)
    % Image (presque) entièrement noire : moyenne très faible et peu de variation.
    try
        I = double(imread(fichier));
        noire = mean(I(:)) < 20 && std(I(:)) < 10;
    catch
        noire = false;
    end
end
