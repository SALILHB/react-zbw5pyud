%% journal_scenario.m  —  chronologie d'un scénario simulé (fenêtre de commande)
%
%   journal_scenario(r)
%
% Affiche chaque changement d'état de la FSM et de puissance de gaz, avec
% l'instant (s et min). C'est ce texte qu'il faut comparer au résultat
% attendu du scénario (et me copier en cas d'écart).

function journal_scenario(r)

    noms = containers.Map({0, 2, 3, 4, 5, 6, 7, 8, 9}, ...
        {'ATTENTE_DEMARRAGE', 'MODE_SOLAIRE', 'MODE_H2', 'MODE_GPL', ...
         'DEMANDE_PROLONGATION', 'PROLONGATION', 'SECHAGE_TERMINE', ...
         'ERREUR_COMBUSTION', 'URGENCE_ATEX'});
    etat = round(r.fsm.Etat_LCD);
    pct = round(100 * r.P_gaz / 5000);
    MAX_LIGNES = 60;

    fprintf('--- Chronologie (etat FSM / puissance de gaz) ---\n');
    fprintf('  t = %8.1f s (%6.1f min) : %s, gaz %d %%\n', r.t(1), r.t(1) / 60, ...
        nomEtat(noms, etat(1)), pct(1));
    n = 0;
    for k = 2:numel(r.t)
        changeEtat = etat(k) ~= etat(k - 1);
        changeGaz = pct(k) ~= pct(k - 1);
        if ~(changeEtat || changeGaz)
            continue;
        end
        n = n + 1;
        if n > MAX_LIGNES
            fprintf('  ... (chronologie tronquee a %d lignes)\n', MAX_LIGNES);
            break;
        end
        if changeEtat
            fprintf('  t = %8.1f s (%6.1f min) : %s, gaz %d %%\n', r.t(k), r.t(k) / 60, ...
                nomEtat(noms, etat(k)), pct(k));
        else
            fprintf('  t = %8.1f s (%6.1f min) :   gaz %d %%   (T_sec = %.1f C)\n', ...
                r.t(k), r.t(k) / 60, pct(k), r.T_sec(k));
        end
    end
    fprintf('  Fin : T_sec = %.1f C, H_sec = %.1f %%, T_sec max = %.1f C\n', ...
        r.T_sec(end), r.H_sec(end), max(r.T_sec));
end

function s = nomEtat(noms, code)
    if isKey(noms, code)
        s = noms(code);
    else
        s = sprintf('code %d', code);
    end
end
