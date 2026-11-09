%% diagnostic_graphique.m  —  teste l'affichage et l'export des figures
%
%   diagnostic_graphique
%
% Si les figures s'affichent toutes blanches (même un simple tracé), le
% problème vient du rendu graphique de MATLAB sur ce PC (carte graphique ou
% pilote), pas des données de simulation. Ce test affiche le moteur de rendu
% utilisé et essaie trois manières d'enregistrer une figure.

function diagnostic_graphique()

    dossier = fullfile(fileparts(mfilename('fullpath')), 'captures');
    if ~exist(dossier, 'dir')
        mkdir(dossier);
    end
    fprintf('MATLAB %s\n', version);

    f = figure('Name', 'Test graphique', 'Color', 'w', 'WindowStyle', 'normal');
    plot(1:10, (1:10).^2, 'o-', 'LineWidth', 2);
    title('Test : une courbe doit apparaitre');
    grid on;
    drawnow;
    try
        info = rendererinfo(gca);
        fprintf('Rendu : %s | %s | %s\n', info.GraphicsRenderer, info.Vendor, info.RendererDevice);
    catch e
        fprintf('rendererinfo indisponible : %s\n', e.message);
    end

    essayer(@() exportgraphics(f, fullfile(dossier, 'test_1_exportgraphics.png'), 'Resolution', 150), ...
        'test_1_exportgraphics.png');
    essayer(@() print(f, fullfile(dossier, 'test_2_print.png'), '-dpng', '-r150'), 'test_2_print.png');
    essayer(@() saveas(f, fullfile(dossier, 'test_3_saveas.png')), 'test_3_saveas.png');

    fprintf(['\nRegardez : 1) la fenetre "Test graphique" montre-t-elle la courbe ?\n' ...
             '           2) lesquels des 3 fichiers test_*.png dans captures/ montrent la courbe ?\n' ...
             'Copiez-moi ces reponses et les lignes ci-dessus.\n']);
end

function essayer(action, nom)
    try
        action();
        fprintf('  [ok] %s enregistre\n', nom);
    catch e
        fprintf('  [ERREUR] %s : %s\n', nom, e.message);
    end
end
