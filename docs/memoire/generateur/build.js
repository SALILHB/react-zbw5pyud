const fs = require("fs");
const {
  Document, Packer, Paragraph, TextRun, HeadingLevel, AlignmentType, TableOfContents,
  Header, Footer, PageNumber, LevelFormat, ExternalHyperlink, BorderStyle,
} = require("docx");
const { H1, H2, H3, P, L, N, CODE, NOTE, TAB, FIG, SAUT, runs } = require("./lib");

const c = [];            // contenu
const add = (...x) => x.flat().forEach(e => c.push(e));

/* ======================================================================
   PAGE DE TITRE
   ====================================================================== */
const titre = (t, taille, opts = {}) => new Paragraph({ alignment: AlignmentType.CENTER, spacing: { after: 200 }, ...opts, children: [new TextRun({ text: t, size: taille, ...opts.run })] });
add(
  titre("République Algérienne Démocratique et Populaire", 22, { spacing: { after: 60 } }),
  titre("École Supérieure en Sciences Appliquées de Tlemcen", 24, { run: { bold: true } }),
  titre("Mémoire de Master / Ingénieur — Document de référence technique", 22, { spacing: { after: 1400 } }),
  titre("COMMANDE EN TEMPÉRATURE D'UN SÉCHOIR SOLAIRE HYBRIDE", 36, { run: { bold: true, color: "1F3864" } }),
  titre("Solaire — Hydrogène — GPL", 28, { run: { color: "C05621" }, spacing: { after: 500 } }),
  titre("Machine à états finis, implémentation sur Arduino Mega 2560", 24),
  titre("et modèle Simulink / Stateflow pour la simulation", 24, { spacing: { after: 1400 } }),
  titre("Version 3 du firmware et du modèle — septembre 2026", 20, { run: { italics: true, color: "4A5568" } }),
  titre("Ce document rassemble toutes les décisions de conception, les règles de la commande, des exemples chiffrés, les boucles utilisées, les principes de passage de la FSM au code Arduino, le rôle de chaque élément, le guide de simulation, les logiciels utilisés et la bibliographie.", 19, { run: { color: "4A5568" } }),
);

/* ======================================================================
   SOMMAIRE
   ====================================================================== */
add(SAUT(),
  new Paragraph({ alignment: AlignmentType.LEFT, spacing: { after: 200 }, children: [new TextRun({ text: "Sommaire", bold: true, size: 32, color: "1F3864" })] }),
  new TableOfContents("Sommaire", { hyperlink: true, headingStyleRange: "1-2" }),
  P("_Si le sommaire est vide à l'ouverture : clic droit dessus > « Mettre à jour les champs » (Word), ou accepter la mise à jour proposée à l'ouverture._"),
);

/* ======================================================================
   1. INTRODUCTION
   ====================================================================== */
add(H1("1. Introduction"),
  H2("1.1 Contexte"),
  P("Le séchage solaire est l'une des plus anciennes applications de l'énergie solaire ; il reste très utilisé pour la conservation des produits agricoles et marins [7], [8], [9]. Sa limite principale est l'intermittence : le rayonnement varie au cours de la journée, disparaît la nuit et par temps couvert. Les séchoirs **hybrides** associent donc au capteur solaire une source d'appoint (gaz, biomasse, électricité) pour maintenir des conditions de séchage régulées [9], [10], [11]."),
  P("Le séchoir étudié ici combine trois sources, par ordre de priorité : **solaire** (gratuit), **hydrogène** (source d'appoint principale) et **GPL** (secours). Particularité importante : le brûleur est **intégré au capteur solaire thermique**, ce qui a des conséquences directes sur la mesure de la température du capteur (chapitre 6)."),
  H2("1.2 Objectif de la commande"),
  P("La commande doit maintenir la température de la chambre de séchage `T_sec` autour d'une consigne `T_cible` saisie par l'opérateur, choisir automatiquement la source d'énergie la plus économique disponible, détecter la fin du séchage (humidité de l'air extrait) et, surtout, garantir la **sécurité** d'une installation qui brûle de l'hydrogène et du GPL (atmosphères explosives, perte de flamme)."),
  H2("1.3 Principes directeurs"),
  ...L([
    "**Hiérarchie énergétique** : solaire > hydrogène > GPL.",
    "**La sécurité avant tout** : aucune ouverture de gaz sans purge préalable ; toute anomalie ferme le gaz dans la même itération du programme.",
    "**Machine à états finis (FSM)** : tout le comportement est décrit par des états et des transitions explicites, vérifiables et traçables.",
    "**Le firmware Arduino est la source de vérité** : en cas d'écart, le modèle Simulink et le mémoire s'alignent sur le code, qui est testé.",
    "**Tout est paramétrable par l'opérateur** (menu LCD), dans des bornes sûres ; certaines grandeurs mesurées peuvent être remplacées par une valeur saisie si un capteur tombe en panne.",
  ]),
  H2("1.4 Organisation du document"),
  P("Les chapitres 2 à 10 décrivent le système et la logique de commande, avec des exemples chiffrés. Le chapitre 4 est consacré aux **boucles** utilisées. Les chapitres 11 à 13 expliquent le **passage de la FSM au code Arduino**, les transformations entre représentations et le **rôle de chaque élément**. Le chapitre 14 est un guide pour la **simulation Simulink**. Les chapitres 15 à 17 portent sur la validation, les logiciels utilisés et l'analyse critique. La bibliographie et les annexes terminent le document."),
);

/* ======================================================================
   2. SYSTÈME
   ====================================================================== */
add(H1("2. Description du système commandé"),
  H2("2.1 Sources d'énergie"),
  ...TAB("Sources d'énergie et leur rôle", ["Source", "Rôle", "Actionneurs", "Remarque"], [
    ["Solaire", "Priorité 1, gratuite", "Aucune électrovanne ; ventilateurs seuls", "Passif ; choisi selon l'estimation T_cap"],
    ["Hydrogène (H2)", "Priorité 2, appoint", "V_H2 + EV1/EV2/EV3 + Spark", "Choisi si Press_H2 ≥ Press_H2_Min"],
    ["GPL (butane)", "Priorité 3, secours", "V_But + EV1/EV2/EV3 + Spark", "Même logique que H2 (code partagé)"],
  ], [2, 2, 3, 3]),
  P("La puissance du brûleur est modulée en **quatre paliers** par trois électrovannes de rampe : 100 % (EV1+EV2+EV3), 67 % (EV2+EV3), 33 % (EV3 seule) et 0 % (tout fermé)."),
  H2("2.2 Capteurs"),
  ...TAB("Capteurs et entrées", ["Grandeur", "Capteur", "Broche", "Rôle dans la commande", "Lecture"], [
    ["T_sec", "DS18B20 [23]", "22 (1-Wire)", "Grandeur régulée (paliers), surchauffe, repère FROID/CHAUD", "1 s"],
    ["T_cap_mesure", "DS18B20", "22 (1-Wire)", "Affichage seul (sonde vue par le brûleur)", "1 s"],
    ["T_amb, H_amb", "DHT22 [24]", "24", "T_cap estimée, H_fin", "1 s"],
    ["H_sec", "DHT22", "26", "Critère de fin de séchage", "1 s"],
    ["Press_H2", "Transmetteur 4-20 mA", "A2", "Choix H2 / GPL", "chaque boucle"],
    ["Flamme", "Contrôleur UV/IR (TOR)", "28", "Allumage réussi, perte de flamme, flamme parasite", "chaque boucle"],
    ["MQ8_H2, MQ6_But", "MQ-8, MQ-6", "A0, A1", "Détection de fuite → URGENCE", "chaque boucle"],
    ["AU_Urgence", "Bouton NC", "42", "Arrêt d'urgence (fil coupé = urgence)", "chaque boucle"],
  ], [2, 2, 1.4, 4, 1.4]),
  H2("2.3 Actionneurs"),
  ...TAB("Actionneurs", ["Sortie", "Broche", "Rôle"], [
    ["V_H2, V_But", "23, 25", "Électrovannes de source (jamais ouvertes ensemble : verrou logiciel)"],
    ["EV1, EV2, EV3", "27, 29, 31", "Électrovannes de rampe → paliers 100/67/33 %"],
    ["Spark", "33", "Module d'allumage haute tension"],
    ["PWM_Purge, PWM_Distrib, PWM_Extract", "2, 3, 4", "Ventilateurs de purge, de distribution, d'extraction"],
    ["Buzzer", "35", "Alarme sonore (erreur, urgence)"],
    ["LCD 20×4 I2C", "SDA/SCL", "Affichage et menu opérateur"],
  ], [3, 1.5, 5]),
  H2("2.4 Interface opérateur"),
  P("Sept boutons : MENU, UP, DOWN, OK, START, STOP, RÉARMEMENT, plus l'arrêt d'urgence. Un écran LCD 20×4 affiche le menu des paramètres, deux pages de télémesure pendant le séchage, la demande de prolongation, les erreurs et la cause d'une urgence."),
  ...FIG("fig1_architecture.png", "Architecture de la commande : capteurs, carte Arduino, actionneurs"),
);

/* ======================================================================
   3. PRINCIPES FSM
   ====================================================================== */
add(H1("3. Principes de la commande par machine à états finis"),
  H2("3.1 Pourquoi une machine à états finis"),
  P("Une machine à états finis décrit un système réactif par un ensemble fini d'**états** et de **transitions** déclenchées par des conditions. Les **statecharts** de Harel [1] l'enrichissent de trois notions indispensables ici : la **hiérarchie** (un état peut contenir des sous-états), le **parallélisme** (plusieurs régions actives en même temps) et la **priorité** des transitions. C'est le formalisme de Stateflow [19] et une approche classique des systèmes embarqués [2], [3]."),
  ...L([
    "**Déterminisme** : à chaque instant, le comportement dépend uniquement de l'état actif et des entrées.",
    "**Sécurité démontrable** : chaque ouverture de gaz est précédée d'un état PURGE ; aucun chemin ne permet de l'éviter.",
    "**Lisibilité et traçabilité** : chaque exigence du cahier des charges correspond à un état ou une transition identifiable.",
    "**Testabilité** : chaque transition peut être testée isolément (chapitre 15).",
  ]),
  H2("3.2 Vocabulaire"),
  ...TAB("Vocabulaire de la FSM", ["Terme", "Définition", "Exemple dans le séchoir"], [
    ["État", "Situation stable du système", "MODE_H2, PURGE, URGENCE_ATEX"],
    ["Transition", "Passage d'un état à un autre", "PURGE → ALLUMAGE"],
    ["Garde (condition)", "Condition qui autorise la transition", "`after(Temps_Purge, sec)`"],
    ["Action d'entrée (entry)", "Exécutée une fois en entrant dans l'état", "Fermer le gaz en entrant en PURGE"],
    ["Action continue (during)", "Exécutée à chaque pas tant que l'état est actif", "Calcul du palier en REGULATION"],
    ["Hiérarchie (OR)", "Un seul sous-état actif à la fois", "PURGE ou ALLUMAGE ou REGULATION"],
    ["Parallélisme (AND)", "Plusieurs régions actives en même temps", "SOURCE ‖ PHASE dans EN_CYCLE"],
    ["Priorité", "Ordre d'évaluation des transitions", "Urgence avant Stop avant surchauffe"],
  ], [2.2, 4, 3.8]),
  H2("3.3 Ce qui est en série et ce qui est en parallèle"),
  ...TAB("Éléments en série (un seul actif à la fois)", ["Niveau", "Enchaînement"], [
    ["Machine globale", "ATTENTE → CYCLE → TERMINÉ → ATTENTE"],
    ["Combustion", "PURGE → ALLUMAGE → RÉGULATION"],
    ["Paliers", "100 % ⇄ 67 % ⇄ 33 % ⇄ 0 %"],
    ["Phase du cycle", "NORMAL → DEMANDE_PROLONGATION → PROLONGATION → DEMANDE…"],
  ], [3, 7]),
  ...TAB("Éléments en parallèle (actifs en même temps pendant le cycle)", ["Bloc", "Rôle", "Variables et paramètres utilisés"], [
    ["SOURCE", "Quelle énergie + combustion", "T_cap, Press_H2, Press_H2_Min, seuils solaires, Temps_Purge, Temps_Allumage"],
    ["RÉGULATION (dans SOURCE)", "Choix du palier", "T_sec, T_cible, T_init, T1/T2/T3, Hhyst, Periode_Regul"],
    ["PHASE", "Fin de cycle et prolongation", "H_sec, H_fin, Temps_Min_Fin, Duree_Max_Cycle, N, Temps_Reponse"],
    ["SÉCURITÉ", "ATEX, flamme, surchauffe, Stop", "MQ8, MQ6, seuils, AU, Flamme, T_SEC_MAX"],
    ["ACQUISITION", "Mesures, valeurs fixes, H_fin, T_cap", "H_produit, H_amb, T_amb, DeltaT_Sol"],
  ], [2.4, 3, 4.6]),
  P("Règle essentielle : **un bloc parallèle ne bloque jamais un autre** ; seule la SÉCURITÉ peut tout arrêter. En particulier, pendant que l'écran demande « Prolonger ? », la combustion continue de réguler au même palier."),
  H2("3.4 Hiérarchie des états"),
  ...FIG("fig2_hierarchie.png", "Hiérarchie des états (version 3)"),
  ...TAB("Rôle de chaque état", ["État", "Rôle", "Sorties principales"], [
    ["ATTENTE_DEMARRAGE", "Repos, attente de START", "Tout fermé, ventilateurs à 0"],
    ["EN_CYCLE", "Cycle de séchage en cours (deux régions parallèles)", "—"],
    ["MODE_SOLAIRE", "Séchage passif", "Gaz fermé ; post-purge éventuelle puis ventilation solaire"],
    ["MODE_H2 / MODE_GPL", "Combustion (même logique, vanne source différente)", "Sous-états PURGE, ALLUMAGE, REGULATION"],
    ["PURGE", "Balayage avant toute ouverture de gaz ; veille à 0 %", "Gaz fermé, purge 255"],
    ["ALLUMAGE", "Ouverture au palier courant + étincelle", "Vannes + Spark"],
    ["REGULATION", "Modulation 100/67/33 % par hystérésis", "Vannes selon palier"],
    ["ERREUR_COMBUSTION", "3 échecs d'allumage : décision de l'opérateur", "Gaz fermé, buzzer"],
    ["NORMAL", "Séchage normal (phase)", "—"],
    ["DEMANDE_PROLONGATION", "Fin atteinte : « Prolonger N min ? »", "Régulation poursuivie"],
    ["PROLONGATION", "Prolongation de N min choisie", "Régulation poursuivie"],
    ["SECHAGE_TERMINE", "Fin du cycle, refroidissement", "Gaz fermé (0 %), extraction faible"],
    ["URGENCE_ATEX", "Repli de sécurité, depuis tout état", "Gaz fermé, purge et extraction 255, buzzer"],
  ], [3, 4.5, 3.5]),
);

/* ======================================================================
   4. BOUCLES
   ====================================================================== */
add(H1("4. Les boucles de la commande"),
  P("La commande est organisée en plusieurs boucles imbriquées. Il faut distinguer la **boucle d'exécution** du programme (le « moteur » qui tourne en permanence) et les **boucles de commande** au sens de l'automatique (mesure → décision → action sur le procédé → nouvelle mesure) [4]."),
  H2("4.1 Boucle principale d'exécution : loop()"),
  P("Sur Arduino, la fonction `loop()` est appelée indéfiniment. Chaque tour exécute quatre étapes dans un ordre fixe. Aucun appel à `delay()` n'est utilisé : un tour dure quelques millisecondes, ce qui permet de lire la flamme et les détecteurs de gaz **à chaque tour**."),
  ...FIG("fig5_boucle.png", "Boucle principale d'exécution du firmware"),
  ...CODE(`void loop() {
  lireEntrees();     // acquisition + valeurs FIXE + T_cap estimée + H_fin
  pasFSM();          // machine à états + transitions prioritaires
  ecrireSorties();   // seul point d'écriture vers les actionneurs
  gererIHM();        // menu LCD + boutons + réarmement
}`),
  P("Deux cadences coexistent : les **entrées de sécurité** (flamme, MQ8, MQ6, arrêt d'urgence, pression) sont lues à chaque tour ; les **capteurs lents** (DS18B20 : 750 ms de conversion ; DHT22 : 2 s minimum) sont lus toutes les secondes, en mode asynchrone pour ne jamais bloquer la boucle. Tous les temps sont mesurés avec `millis()` figé en début de tour (`t_boucle`)."),
  H2("4.2 Boucle de régulation de température (boucle fermée)"),
  P("C'est la boucle principale au sens de l'automatique : **mesure** de `T_sec` → **comparaison** avec les seuils T1/T2/T3 → **décision** du palier → **action** sur les électrovannes → la chambre chauffe → nouvelle mesure. Il s'agit d'une commande **tout-ou-rien étagée à hystérésis** (quatre niveaux de puissance), exécutée toutes les `Periode_Regul` secondes (chapitre 5)."),
  H2("4.3 Boucle de sélection de la source (supervision)"),
  P("Boucle plus lente qui choisit l'énergie : **mesure** de `T_cap` (estimée) et de `Press_H2` → **décision** solaire / H2 / GPL avec hystérésis (seuils ON/OFF, marge de retour H2) → **action** : extinction, purge et rallumage sur la nouvelle source (chapitre 6)."),
  H2("4.4 Boucle de séquencement de la combustion"),
  P("Séquence purge → allumage → régulation, avec reprise après échec : chaque échec referme le gaz et **reboucle sur une purge complète** avant un nouvel essai, jusqu'à trois essais (chapitre 7)."),
  H2("4.5 Boucle de sécurité (surveillance permanente)"),
  P("À chaque tour : fuite de gaz, arrêt d'urgence, flamme vue alors que le gaz est fermé, perte de flamme, surchauffe. Cette boucle a la **priorité absolue** : ses transitions sont évaluées avant toutes les autres et écrasent les sorties calculées (chapitre 9)."),
  H2("4.6 Boucle de fin de cycle (opérateur dans la boucle)"),
  P("**Mesure** de `H_sec` et du temps de cycle → **décision** de fin (humidité ou durée maximale) → **question à l'opérateur** (prolonger N minutes ou arrêter) → si l'opérateur prolonge, la boucle recommence à la fin des N minutes (chapitre 8). C'est une boucle où l'humain intervient dans la décision."),
  H2("4.7 Boucle d'interface homme-machine"),
  P("Lecture des boutons avec anti-rebond et détection de front (un appui = une action), édition des paramètres, rafraîchissement de l'écran toutes les 400 ms."),
  ...TAB("Récapitulatif des boucles", ["Boucle", "Mesure", "Décision", "Action", "Période", "Type"], [
    ["Exécution loop()", "—", "—", "Enchaîne les 4 étapes", "quelques ms", "Cyclique"],
    ["Température", "T_sec", "Palier (hystérésis)", "EV1/EV2/EV3", "Periode_Regul (1 s)", "Fermée"],
    ["Source", "T_cap estimée, Press_H2", "Solaire / H2 / GPL", "Vannes source, purge", "chaque tour", "Fermée (supervision)"],
    ["Combustion", "Flamme, temps", "Purge / allumage / reprise", "Vannes, Spark, purge", "chaque tour", "Séquentielle"],
    ["Sécurité", "MQ8, MQ6, AU, Flamme, T_sec", "Urgence / arrêt", "Fermeture du gaz", "chaque tour", "Surveillance"],
    ["Fin de cycle", "H_sec, temps", "Demande / prolongation", "Arrêt ou poursuite", "chaque tour", "Fermée + opérateur"],
    ["IHM", "Boutons", "Édition, navigation", "Écran, paramètres", "400 ms (écran)", "Ouverte"],
  ], [2, 2.2, 2.2, 2.2, 1.8, 1.8]),
);

/* ======================================================================
   5. RÉGULATION PAR PALIERS
   ====================================================================== */
add(H1("5. Régulation de température par paliers"),
  H2("5.1 Calcul des seuils"),
  P("Les trois seuils de palier sont calculés à partir de la température de départ `T_init` et de la consigne `T_cible`, en découpant l'intervalle en trois tiers :"),
  ...CODE(`T1 = T_init + (T_cible − T_init) / 3
T2 = T_init + 2·(T_cible − T_init) / 3
T3 = T_cible
Si T_cible ≤ T_init + 3 °C :  T1 = T2 = T3 = T_cible   (écart trop faible)`),
  P("En mode automatique, `T_init` est la température mesurée au démarrage ; en mode manuel, c'est une valeur saisie. Les seuils sont recalculés immédiatement si l'opérateur modifie `T_cible` pendant le cycle (UP/DOWN) ; `T_init` reste celle du démarrage."),
  H2("5.2 Hystérésis à quatre niveaux"),
  P("Autour de chaque seuil, une **bande morte** de largeur `Hhyst` (±Hhyst/2) évite que le palier oscille quand la température est pile sur un seuil. Le palier est **mémorisé** : la décision dépend du palier actuel et de `T_sec`, pas seulement de `T_sec`."),
  ...TAB("Transitions de palier", ["Palier actuel", "T_sec monte (la puissance baisse)", "T_sec descend (la puissance remonte)"], [
    ["100 %", "T_sec ≥ T1 + Hhyst/2 → 67 %", "—"],
    ["67 %", "T_sec ≥ T2 + Hhyst/2 → 33 %", "T_sec < T1 − Hhyst/2 → 100 %"],
    ["33 %", "T_sec ≥ T3 + Hhyst/2 → 0 %", "T_sec < T2 − Hhyst/2 → 67 %"],
    ["0 % (veille)", "—", "T_sec < T3 − Hhyst/2 → rallumage à 33 %"],
  ], [2, 4, 4]),
  ...FIG("fig3_hysteresis.png", "Hystérésis à quatre niveaux (T_init = 25 °C, T_cible = 55 °C, Hhyst = 4 °C)", 560),
  H2("5.3 Exemple numérique"),
  P("Avec `T_init = 25 °C`, `T_cible = 55 °C` et `Hhyst = 4 °C` : T1 = 35 °C, T2 = 45 °C, T3 = 55 °C. Les seuils effectifs sont donc : 37 °C (100→67 %), 47 °C (67→33 %), 57 °C (33→0 %) à la montée ; 53 °C (0→33 %), 43 °C (33→67 %), 33 °C (67→100 %) à la descente. Entre 33 et 37 °C par exemple, le palier ne change pas."),
  H2("5.4 Le palier 0 % est une coupure volontaire"),
  P("Quand `T_sec` atteint la consigne, le brûleur passe à 0 % : le gaz est fermé et le système entre en **veille** dans l'état PURGE. **Ce n'est pas une fin de séchage** (température atteinte ne veut pas dire produit sec) et ce n'est pas une panne (aucun échec n'est compté)."),
  P("Pendant la veille, le chronomètre de purge **n'est pas relancé** : la purge de 120 s est acquise une fois pour toutes. Exemple : à 10:00 la chambre atteint 57 °C → palier 0 %, purge ; à 10:02 la purge est terminée, la chambre est encore à 56 °C → veille, ventilateur en marche ; à 10:30 la chambre descend à 52,9 °C → rallumage **immédiat** à 33 %, sans attendre à nouveau 120 s."),
  H2("5.5 Période de régulation et consigne modifiable"),
  P("La comparaison `T_sec` / seuils a lieu toutes les `Periode_Regul` secondes (1 s par défaut, 1 à 60 s). Ce n'est pas la période qui évite les oscillations mais l'hystérésis : une période trop longue ferait dépasser la consigne. La sonde DS18B20 impose un minimum pratique d'environ 1 s (750 ms de conversion à 12 bits [23])."),
  H2("5.6 Palier imposé (mode dégradé)"),
  P("La température régulée `T_sec` n'a **jamais** de mode « valeur fixe » : une valeur fixe rendrait la régulation aveugle et désactiverait la sécurité surchauffe. À la place, l'opérateur peut choisir un **palier imposé** (100, 67 ou 33 %) : la régulation par `T_sec` est suspendue, mais la sécurité surchauffe reste active sur la mesure."),
  ...CODE(`// Extrait du firmware — majPalier() : hystérésis mémorisée par le palier courant
static void majPalier() {
  float demi = demiHyst();
  switch (palier) {
    case PALIER_100: if (T_sec >= T1 + demi) palier = PALIER_67; break;
    case PALIER_67:  if (T_sec >= T2 + demi) palier = PALIER_33;
                     else if (T_sec < T1 - demi) palier = PALIER_100; break;
    case PALIER_33:  if (T_sec >= T3 + demi) palier = PALIER_0;
                     else if (T_sec < T2 - demi) palier = PALIER_67; break;
    case PALIER_0:   if (T_sec < T3 - demi) palier = PALIER_33; break;
  }
}`),
);

/* ======================================================================
   6. CHOIX DE LA SOURCE
   ====================================================================== */
add(H1("6. Choix de la source d'énergie"),
  H2("6.1 Pourquoi T_cap est estimée"),
  P("Le brûleur étant intégré au capteur solaire, la sonde placée sur l'absorbeur mesure aussi la chaleur du brûleur. Si l'on s'y fiait, le capteur paraîtrait toujours « chaud » pendant la combustion, et le système ne reviendrait **jamais** au solaire. La température du capteur utilisée par la commande est donc **estimée** :"),
  ...CODE(`T_cap = T_amb + DeltaT_Sol        (DeltaT_Sol paramétrable, 20 °C par défaut, à calibrer)`),
  P("La sonde de l'absorbeur reste lue, mais uniquement pour l'affichage (`T_cap_mesure`). `T_cap` ne sert **qu'au choix de la source**, jamais à la régulation des paliers (qui utilise `T_sec`). Les limites de cette estimation sont discutées au chapitre 17 ; le modèle physique habituel d'un capteur fait intervenir l'éclairement solaire [5]."),
  H2("6.2 Seuils solaires"),
  ...CODE(`ON  = T_cible + Marge_Sol          (Marge_Sol = 0 par défaut)
OFF = ON − Hyst_Sol                (Hyst_Sol = 5 °C par défaut)`),
  P("La marge `Marge_Sol` permet de tenir compte des pertes entre le capteur et la chambre : si le capteur est exactement à `T_cible`, l'air de la chambre sera un peu plus froid."),
  H2("6.3 Repère thermique FROID / CHAUD"),
  P("Le repère indique si la chambre est déjà chaude : **CHAUD** si `T_sec ≥ Seuil_Chaud` (40 °C par défaut, paramétrable), **FROID** sinon, avec une bande de ±Hhyst/2 pour ne pas osciller. Il est indépendant du fait que l'on démarre ou que l'on soit en marche : un redémarrage juste après un cycle se fait en régime CHAUD."),
  ...L([
    "**FROID** : le soleil doit être franc (`T_cap ≥ ON`) pour choisir ou garder le solaire ; la combustion démarre à 100 %.",
    "**CHAUD** : l'inertie thermique de la chambre tolère un soleil plus faible (jusqu'à OFF) ; la combustion démarre au palier qui correspond à `T_sec` (par exemple 33 % à 48 °C), pour ne pas dépasser la consigne.",
    "Quitter la combustion pour revenir au solaire exige **toujours** `T_cap ≥ ON`.",
  ]),
  H2("6.4 Règles d'arbitrage (mode automatique)"),
  ...TAB("Règles de choix de la source", ["Situation", "Règle"], [
    ["Démarrage, régime FROID", "Solaire si T_cap ≥ ON, sinon combustion"],
    ["Démarrage, régime CHAUD", "Solaire si T_cap ≥ OFF, sinon combustion"],
    ["En combustion", "Retour au solaire si T_cap ≥ ON (extinction normale + post-purge)"],
    ["En solaire, FROID", "Combustion si T_cap < ON"],
    ["En solaire, CHAUD", "Combustion si T_cap < OFF"],
    ["Combustion : choix du gaz", "H2 si Press_H2 ≥ Press_H2_Min, sinon GPL"],
    ["En H2", "Bascule GPL si Press_H2 < Press_H2_Min"],
    ["En GPL", "Retour H2 si Press_H2 ≥ Press_H2_Min + 0,5 bar"],
  ], [3.5, 6.5]),
  ...TAB("Exemples (T_cible = 55 °C, ON = 55 °C, OFF = 50 °C, Seuil_Chaud = 40 °C)", ["Moment", "T_sec", "T_cap", "Décision", "Pourquoi"], [
    ["Démarrage 9 h", "25 °C (FROID)", "52 °C", "Combustion à 100 %", "52 < ON"],
    ["Redémarrage à chaud", "45 °C (CHAUD)", "52 °C", "Solaire", "52 ≥ OFF"],
    ["En combustion, 11 h", "50 °C", "56 °C", "→ Solaire", "56 ≥ ON"],
    ["En solaire, 15 h", "48 °C (CHAUD)", "53 °C", "Reste solaire", "entre OFF et ON"],
    ["En solaire, 16 h", "48 °C (CHAUD)", "49 °C", "→ Combustion à 33 %", "49 < OFF ; palier selon T_sec"],
    ["En solaire, chambre refroidie", "37 °C (FROID)", "52 °C", "→ Combustion à 100 %", "FROID : 52 < ON"],
  ], [2.4, 1.8, 1.2, 2.4, 2.4]),
  H2("6.5 Retour au solaire"),
  P("Le retour au solaire est une **extinction normale**, pas une urgence : le gaz est fermé, puis une **post-purge** de `Temps_Purge` (120 s) évacue les gaz résiduels, avant de passer aux ventilateurs du mode solaire."),
  H2("6.6 Bascule H2 ↔ GPL et mémoire de palier"),
  P("Quand la pression d'hydrogène devient insuffisante, le système bascule sur le GPL. Le **palier courant est conservé** (mémoire de palier) ; seule une purge complète est imposée avant le rallumage. La nouvelle source dispose d'un jeu complet de trois essais d'allumage."),
  P("Exemple : en H2 à 33 %, `T_sec = 54 °C`. La pression chute → vanne H2 fermée, purge 120 s sans chauffage, `T_sec` retombe à 50 °C. Rallumage GPL à 33 % : la règle générale s'applique aussitôt ; comme 50 ≥ T2 − 2 = 43 °C, on reste à 33 %. Si `T_sec` était tombée à 42 °C, le palier serait remonté à 67 %."),
);

/* ======================================================================
   7. COMBUSTION
   ====================================================================== */
add(H1("7. Séquences de combustion"),
  H2("7.1 Purge"),
  P("Gaz fermé, étincelle coupée, ventilateur de purge au maximum pendant `Temps_Purge` (120 s, jamais moins de 60 s même si l'opérateur le règle plus bas). **Aucune ouverture de gaz n'est possible sans une purge complète**, quelle qu'en soit la raison : démarrage, échec, bascule de source ou veille à 0 %."),
  H2("7.2 Allumage et échecs"),
  P("Ouverture de la vanne source et des électrovannes du palier courant, avec étincelle, pendant `Temps_Allumage` (4 s). Si la flamme apparaît : passage en régulation, compteur d'échecs remis à zéro. Sinon : échec, gaz fermé, purge complète, nouvel essai. Au troisième échec consécutif : ERREUR_COMBUSTION."),
  ...TAB("Chronologie de trois échecs d'allumage", ["t (s)", "Étape"], [
    ["0 → 120", "PURGE initiale"], ["120 → 124", "ALLUMAGE n° 1, pas de flamme → échec 1"],
    ["124 → 244", "PURGE 120 s"], ["244 → 248", "ALLUMAGE n° 2 → échec 2"],
    ["248 → 368", "PURGE 120 s"], ["368 → 372", "ALLUMAGE n° 3 → échec 3 → ERREUR_COMBUSTION"],
  ], [2, 8]),
  H2("7.3 Échec après une bascule"),
  P("La bascule n'est pas instantanée : elle comporte une purge. Après une bascule, la **même règle des trois essais** s'applique (décision de la version 3 ; la version 2 passait en erreur dès le premier échec). Exemple : pression H2 insuffisante à t = 0 → purge 0–120 s → allumage GPL au palier mémorisé 120–124 s → en cas d'échec, purge et nouvel essai, jusqu'à trois."),
  H2("7.4 Perte de flamme en régulation"),
  P("Si la flamme disparaît alors que le gaz est ouvert, du gaz non brûlé s'échappe : le gaz est **fermé dans la même itération** (quelques millisecondes). La première perte du cycle est suivie d'une **relance** (purge puis allumage) ; la deuxième verrouille l'installation en **URGENCE** (cause « PERTE FLAMME »), ce qui impose une intervention de l'opérateur. Ce principe de mise en sécurité avec une seule relance autorisée est celui des normes de contrôle de brûleurs [12]."),
  H2("7.5 ERREUR_COMBUSTION"),
  P("Gaz fermé, buzzer actif. L'opérateur fait défiler trois choix et valide avec OK :"),
  ...L([
    "**RÉESSAYER** : nouvelle purge puis nouvel allumage sur la même source, au palier mémorisé, avec un compteur d'échecs remis à zéro.",
    "**MANUEL** : passage en mode manuel et retour à l'écran d'accueil.",
    "**AUTOMATIQUE** : passage en mode automatique et nouvel essai.",
  ]),
  P("STOP termine le cycle. Il n'existe **aucune sortie automatique** de cet état : le système attend toujours une décision humaine."),
  ...CODE(`// Extrait du firmware — ALLUMAGE : même règle après une bascule (V3)
} else if ((t_boucle - chrono_allumage) >= (Config.Temps_Allumage * 1000UL)) {
  nb_echecs_allumage++;
  if (nb_echecs_allumage >= MAX_ECHECS_AVANT_ALARME) {
    entrerErreurCombustion();
  } else {
    armerPurge(PURGE_ECHEC);          // fermeture totale puis nouvelle purge
  }
}`),
);

/* ======================================================================
   8. FIN DE CYCLE
   ====================================================================== */
add(H1("8. Fin de cycle et prolongation"),
  H2("8.1 Critères de fin"),
  ...L([
    "**Critère 1 (prioritaire)** : humidité de l'air extrait atteinte, `H_sec ≤ H_fin`, avec `H_fin = H_produit + H_amb` recalculée en permanence.",
    "**Critère 2** : durée maximale de séchage atteinte, `Duree_Max_Cycle` (paramétrable).",
    "`T_sec` qui atteint `T_cible` **n'est pas** un critère de fin : c'est seulement le palier 0 % de la régulation.",
  ]),
  P("L'humidité relative de l'air et sa relation à la teneur en eau du produit relèvent de la psychrométrie et des isothermes de sorption ; voir par exemple [6] pour les principes du séchage industriel."),
  H2("8.2 Rôle de Temps_Min_Fin"),
  P("Au début de la chauffe, l'air qui se réchauffe voit son humidité relative chuter fortement (un air chaud peut contenir plus d'eau), alors que le produit est encore humide. `H_sec` peut alors passer sous `H_fin` dès les premières minutes. `Temps_Min_Fin` (120 min par défaut, 0 = désactivé) **ignore le critère d'humidité** pendant ce début de cycle, pour éviter une fin prématurée."),
  H2("8.3 Demande de prolongation"),
  P("Dès qu'un critère de fin est atteint, l'écran affiche le motif (« HUMIDITE ATTEINTE » ou « DUREE MAX ATTEINTE ») et propose « Prolonger N min ? ». **La régulation continue au même palier pendant la question.**"),
  ...TAB("Réponses possibles à la demande", ["Action de l'opérateur", "Effet"], [
    ["UP / DOWN", "N ± 5 min (5 à 720 min) ; relance le délai de réponse (l'opérateur est présent)"],
    ["OK", "PROLONGATION de N minutes"],
    ["STOP", "SECHAGE_TERMINE immédiat (0 %)"],
    ["Aucune action pendant Temps_Reponse (5 min)", "SECHAGE_TERMINE, brûleur à 0 %"],
  ], [4, 6]),
  H2("8.4 Prolongation"),
  ...L([
    "Pendant la prolongation, la régulation continue exactement comme avant, au palier mémorisé.",
    "À la fin des N minutes, la question est **reposée** (motif « PROLONGATION FINIE »).",
    "Si la prolongation avait été demandée pour **durée maximale** et que l'humidité est atteinte pendant la prolongation, la question est reposée **immédiatement** (motif « HUMIDITE ATTEINTE »).",
    "Si l'humidité était **déjà** atteinte, les N minutes choisies sont respectées (sinon l'opérateur ne pourrait jamais prolonger un séchage déjà à l'humidité cible).",
  ]),
  H2("8.5 Exemple d'un cycle complet simulé"),
  P("La figure suivante est obtenue en exécutant le **firmware réel**, compilé sur PC avec les mêmes simulations de capteurs que le banc de tests, bouclé sur un modèle thermique du premier ordre **illustratif** (constante de temps 1200 s, élévation de 120 °C à 100 % ; ce n'est pas le modèle identifié du mémoire). Réglages : T_cible = 55 °C, Hhyst = 4 °C, Temps_Min_Fin = 60 min, H_fin = 45 %, aucun opérateur présent. Programme : `tests/simulation_cycle.cpp` (`make -C tests simulation`)."),
  ...FIG("fig4_cycle.png", "Cycle complet simulé avec le firmware réel (modèle thermique illustratif)", 500),
  ...TAB("Lecture du cycle simulé", ["Temps", "Événement"], [
    ["0,17 min", "Appui sur START ; T_cap estimée 45 °C < ON 55 °C → combustion H2, purge"],
    ["2,3 min", "Fin de purge → allumage à 100 %"],
    ["4,3 min", "T_sec ≈ 37 °C → 67 %"],
    ["7,5 min", "T_sec ≈ 47 °C → 33 %"],
    ["24,3 min", "T_sec ≈ 57 °C → 0 % (veille, gaz fermé)"],
    ["27 min", "T_sec ≈ 53 °C → rallumage direct à 33 % (purge déjà acquise)"],
    ["24 → 80 min", "Six coupures / rallumages à 0 % ↔ 33 % : illustration du risque de cycles répétés (chapitre 17)"],
    ["83,2 min", "H_sec ≤ H_fin après Temps_Min_Fin → DEMANDE_PROLONGATION, régulation poursuivie"],
    ["88,2 min", "Aucune réponse pendant 5 min → SECHAGE_TERMINE, brûleur à 0 %"],
    ["93,2 min", "Fin du refroidissement (5 min) → ATTENTE_DEMARRAGE"],
  ], [2, 8]),
);

/* ======================================================================
   9. SÉCURITÉS
   ====================================================================== */
add(H1("9. Sécurités"),
  P("L'installation manipule de l'hydrogène et du GPL : un mélange air-gaz peut former une atmosphère explosive. La réglementation européenne ATEX [13] et les normes associées [14] encadrent les équipements utilisés dans ces zones ; la sécurité fonctionnelle des systèmes électroniques de sécurité est traitée par la CEI 61508 [15]. Les propriétés de l'hydrogène (large domaine d'inflammabilité, très faible énergie d'inflammation) rendent la détection de fuite et la purge particulièrement importantes [16]."),
  H2("9.1 URGENCE_ATEX"),
  ...TAB("Causes d'urgence", ["Cause affichée", "Déclenchement", "Réarmement possible si"], [
    ["URG: FUITE H2", "MQ8 > Seuil_MQ8", "MQ8 revenu sous le seuil"],
    ["URG: FUITE GPL", "MQ6 > Seuil_MQ6", "MQ6 revenu sous le seuil"],
    ["URG: ARRET URGENCE", "Bouton NC ouvert ou fil coupé", "Bouton relâché, câblage rétabli"],
    ["URG: PERTE FLAMME", "2e perte de flamme en régulation dans le cycle", "Flamme éteinte"],
    ["URG: FLAMME PARASITE", "Flamme vue plus de 5 s alors que le gaz est fermé", "Flamme éteinte"],
  ], [2.6, 4, 3.4]),
  P("En urgence : gaz fermé, purge et extraction à 255, buzzer, toutes les autres logiques suspendues. **Deux chemins de réarmement** : le bouton RÉARMEMENT, ou le menu (OK sur « Réarmement »). Le réarmement n'est accepté que si **aucune cause n'est encore présente** (y compris une flamme encore vue). Le cycle en cours n'est **jamais repris** : retour à ATTENTE_DEMARRAGE, l'opérateur doit rappuyer sur START."),
  H2("9.2 La flamme : toujours jugée selon le contexte"),
  ...TAB("Interprétation de l'état de la flamme", ["Contexte", "Vannes gaz", "Flamme = 0 signifie", "Action"], [
    ["ATTENTE, TERMINÉ, SOLAIRE", "fermées", "normal", "rien"],
    ["PURGE (y compris veille 0 %)", "fermées", "normal", "rien"],
    ["ALLUMAGE depuis moins de 4 s", "ouvertes + étincelle", "flamme pas encore établie", "on attend"],
    ["ALLUMAGE depuis 4 s ou plus", "ouvertes", "échec d'allumage", "gaz fermé → purge → nouvel essai (3 max)"],
    ["RÉGULATION 100/67/33 %", "ouvertes", "extinction anormale", "gaz fermé ; relance puis URGENCE"],
    ["Tout état, gaz fermé, flamme = 1", "fermées", "vanne qui fuit ou capteur défaillant", "URGENCE après 5 s"],
  ], [3, 2, 2.6, 3.4]),
  P("La flamme est lue **à chaque tour** de boucle (quelques millisecondes) et le gaz est fermé dans le même tour. Le temps de réponse global est donc imposé par le capteur : un détecteur UV ou IR répond en moins d'une seconde, compatible avec l'exigence de moins de 2 s ; un thermocouple (10 à 60 s) ne le serait pas."),
  H2("9.3 Autres sécurités"),
  ...L([
    "**STOP** : à tout moment pendant un cycle, fermeture du gaz et SECHAGE_TERMINE.",
    "**Surchauffe** : `T_sec ≥ 90 °C` → arrêt, indépendamment de la régulation par paliers.",
    "**Exclusion mutuelle** : `ecrireSorties()` refuse d'ouvrir V_H2 et V_But en même temps (dernier verrou avant le matériel).",
    "**Paramètres bornés** : la purge ne peut pas descendre sous 60 s, quel que soit le réglage (`bornerConfig()`).",
    "**Fail-safe** : arrêt d'urgence en contact NC (fil coupé = urgence) ; boucle 4-20 mA coupée = pression 0 = bascule GPL.",
  ]),
  H2("9.4 Ordre de priorité des transitions"),
  ...N([
    "Fuite de gaz ou arrêt d'urgence → URGENCE (tout est écrasé).",
    "Flamme vue gaz fermé plus de 5 s → URGENCE.",
    "STOP en cycle → SECHAGE_TERMINE.",
    "Surchauffe → SECHAGE_TERMINE.",
    "Fin de cycle → DEMANDE_PROLONGATION (humidité avant durée maximale).",
  ]),
);

/* ======================================================================
   10. PARAMÈTRES
   ====================================================================== */
add(H1("10. Paramètres opérateur et modes de fonctionnement"),
  H2("10.1 Automatique, manuel, semi-automatique"),
  ...L([
    "**Automatique** : toutes les grandeurs sont mesurées, la source est choisie par l'arbitrage.",
    "**Manuel** : l'opérateur impose la source (solaire, H2 ou GPL) et `T_init`.",
    "**Semi-automatique (dégradé)** : certaines grandeurs mesurées sont remplacées par une valeur saisie, ou le palier est imposé.",
    "Dans **tous** les modes, la sécurité reste automatique.",
  ]),
  H2("10.2 Grandeurs AUTO ou FIXE"),
  ...TAB("Remplacement d'une mesure par une valeur saisie", ["Grandeur", "FIXE autorisé ?", "Conséquence"], [
    ["T_amb, H_amb", "Oui", "H_fin et T_cap calculés sur la valeur saisie"],
    ["H_sec", "Oui, avec avertissement", "Le critère d'humidité ne se déclenche plus : fin par la durée"],
    ["Press_H2", "Oui, avec avertissement", "Réservoir vide non détecté ; découvert à l'échec d'allumage"],
    ["T_sec", "Non", "Remplacé par le « palier imposé »"],
    ["MQ8, MQ6, flamme, AU", "Jamais", "Cela désactiverait la sécurité"],
  ], [2.5, 2.5, 5]),
  H2("10.3 Liste des paramètres"),
  ...TAB("Paramètres du menu (structure Config, sauvegardée en EEPROM)", ["Paramètre", "Défaut", "Bornes", "Rôle"], [
    ["T_cible / T_init", "55 / 25 °C", "30–90 / 0–60", "Consigne ; température de départ (manuel)"],
    ["H_produit_cible", "10 %", "1–50", "Entre dans H_fin"],
    ["Duree_Max_Cycle", "600 min", "15–1440", "Critère de fin n° 2"],
    ["Prolong_Defaut", "30 min", "5–720", "N proposé à chaque demande"],
    ["Temps_Reponse", "300 s", "30–1800", "Sans réponse → fin"],
    ["Temps_Min_Fin", "120 min", "0–1440", "Verrou du critère d'humidité"],
    ["Hhyst", "5 °C", "1–15", "Bande d'hystérésis des paliers"],
    ["Periode_Regul", "1 s", "1–60", "Période de comparaison T_sec / seuils"],
    ["Regul_Auto / Palier_Impose", "AUTO / 67 %", "100/67/33 %", "Palier imposé (dégradé)"],
    ["DeltaT_Sol", "20 °C", "0–60", "T_cap = T_amb + DeltaT_Sol (à calibrer)"],
    ["Marge_Sol / Hyst_Sol", "0 / 5 °C", "0–20 / 1–20", "Seuils solaires ON / OFF"],
    ["Seuil_Chaud", "40 °C", "20–80", "Repère FROID / CHAUD"],
    ["Temps_Purge", "120 s", "60–300", "Plancher de sécurité 60 s"],
    ["Temps_Allumage", "4 s", "2–10", "Délai de confirmation de flamme"],
    ["Press_H2_Min", "2 bar", "0,5–8", "Bascule vers le GPL"],
    ["Seuil_MQ8 / Seuil_MQ6", "350", "100–900 (/1023)", "Seuils de fuite"],
    ["Fixe_… / Val_…", "AUTO", "selon grandeur", "T_amb, H_amb, H_sec, Press_H2"],
  ], [3, 1.8, 2, 3.8]),
  P("La configuration est sauvegardée dans l'EEPROM interne de l'ATmega2560 [22] et rechargée au démarrage. Une **sentinelle** détecte une EEPROM vierge ou d'une ancienne version (valeurs par défaut rechargées), et un **bornage systématique** protège contre une EEPROM corrompue."),
);

/* ======================================================================
   11. FSM -> ARDUINO
   ====================================================================== */
add(H1("11. Du modèle FSM au code Arduino"),
  H2("11.1 Démarche de conception"),
  P("La démarche suit le principe de la **conception basée sur les modèles** : la machine à états est d'abord spécifiée (états, transitions, priorités), puis traduite en code, testée, et reproduite dans un modèle de simulation [2]. Ici, le sens est assumé : **le firmware testé fait foi**, et le modèle Stateflow et le mémoire sont alignés sur lui."),
  ...FIG("fig6_chaine.png", "Chaîne de conception et de validation"),
  H2("11.2 Règles de traduction"),
  P("Le microcontrôleur n'exécute pas un diagramme : chaque notion de la FSM est traduite par une construction C++ précise. Ces règles s'inspirent des techniques classiques d'implémentation des statecharts en C [3]."),
  ...TAB("Correspondance FSM (Stateflow) → code Arduino (C++)", ["Notion FSM / Stateflow", "Traduction dans le firmware"], [
    ["État", "Valeur de `enum EtatFSM` ; variable `etat_courant`"],
    ["Sous-états (PURGE, ALLUMAGE, REGULATION)", "Deuxième variable d'état `phase_combustion` (enum PhaseCombustion), traitée par `gererCombustion()`"],
    ["Régions parallèles SOURCE ‖ PHASE", "Variables indépendantes (`Source_Active`, `phase_combustion` d'un côté ; `etat_courant` de l'autre) mises à jour dans le même tour : `etapeRegulation(false)`"],
    ["Transition", "`if (garde) { action ; changerEtat(nouvel_etat); }`"],
    ["Action d'entrée (entry)", "Code exécuté dans `changerEtat()` ou dans la fonction qui arme la transition (ex. `armerPurge()`)"],
    ["Action continue (during)", "Code du `case` de l'état, exécuté à chaque tour"],
    ["`after(N, sec)`", "Chronomètre : `chrono = t_boucle` à l'entrée ; garde `(t_boucle − chrono) >= N·1000`"],
    ["`temporalCount(sec)` (temps depuis l'entrée)", "`(t_boucle − chrono_cycle)`"],
    ["`duration(C)` (temps pendant lequel C est vraie)", "Chronomètre armé quand C devient vraie, désarmé quand elle devient fausse (flamme parasite)"],
    ["Priorité des transitions externes", "« Transitions prioritaires » évaluées **après** le `switch`, dans l'ordre de sécurité, chacune terminée par `return`"],
    ["`in(Etat)`", "Comparaison `etat_courant == ETAT_…` (ex. `cycleEnCours()`)"],
    ["Événement bouton", "Détection de front montant + anti-rebond, consommée une seule fois : `frontPris()`"],
    ["Mémoire (palier, compteurs)", "Variables globales conservées entre les tours"],
    ["Données Input / Output", "`lireEntrees()` / `ecrireSorties()` (point d'accès unique au matériel)"],
    ["Paramètres", "Structure `Config` en EEPROM, bornée par `bornerConfig()`"],
  ], [4, 6]),
  H2("11.3 Squelette du code"),
  ...CODE(`enum EtatFSM : uint8_t {
  ETAT_ATTENTE_DEMARRAGE, ETAT_CONFIG_MENU, ETAT_MODE_SOLAIRE,
  ETAT_MODE_H2, ETAT_MODE_GPL, ETAT_DEMANDE_PROLONGATION,
  ETAT_PROLONGATION, ETAT_SECHAGE_TERMINE, ETAT_ERREUR_COMBUSTION,
  ETAT_URGENCE_ATEX };

void pasFSM() {
  majRegime();                           // repère FROID / CHAUD
  switch (etat_courant) {                // 1) UN case PAR ÉTAT
    case ETAT_MODE_H2:
      Source_Active = SRC_H2;
      reglerTcibleEnCycle();             // UP/DOWN : T_cible en cycle
      etapeRegulation(true);             // arbitrage + gererCombustion()
      break;
    case ETAT_DEMANDE_PROLONGATION:
      etapeRegulation(false);            // la régulation CONTINUE
      if (frontPris(B_OK)) changerEtat(ETAT_PROLONGATION);
      else if (frontPris(B_STOP)) { fermerGaz(); changerEtat(ETAT_SECHAGE_TERMINE); }
      else if (delai_reponse_ecoule) { fermerGaz(); changerEtat(ETAT_SECHAGE_TERMINE); }
      break;
    /* ... un case pour chacun des autres états ... */
  }
  // 2) TRANSITIONS PRIORITAIRES, dans l'ordre de sécurité
  if (causeGazOuAU() != URG_AUCUNE) { declencherUrgence(...); return; }
  /* flamme parasite, STOP, surchauffe, fin de cycle ... */
}`),
  H2("11.4 Exemple de traduction : échec d'allumage"),
  P("La même règle écrite dans les deux représentations :"),
  ...CODE(`Stateflow (transition ALLUMAGE → PURGE) :
[after(Temps_Allumage, sec) && Flame==0 && nb_echecs_allumage + 1 < MAX_ECHECS]
{nb_echecs_allumage = nb_echecs_allumage + 1; raison_purge = uint8(1);}

Arduino (case PH_ALLUMAGE de gererCombustion) :
else if ((t_boucle - chrono_allumage) >= (Config.Temps_Allumage * 1000UL)) {
  nb_echecs_allumage++;
  if (nb_echecs_allumage >= MAX_ECHECS_AVANT_ALARME) entrerErreurCombustion();
  else armerPurge(PURGE_ECHEC);
}`),
  P("Remarque : le firmware **incrémente puis teste** ; en Stateflow, la garde est évaluée **avant** l'action, d'où la comparaison de `nb_echecs_allumage + 1`. Sans cette précaution, le modèle aurait autorisé un essai de plus que le firmware : c'est un exemple typique d'écart de traduction."),
  H2("11.5 Pièges de traduction rencontrés"),
  ...L([
    "**Unités** : durées du menu en minutes, firmware en millisecondes, Stateflow en secondes (chapitre 12).",
    "**Origine des chronomètres** : `after()` mesure le temps depuis l'entrée dans l'état source de la transition, pas depuis le début du cycle.",
    "**Écriture d'une entrée** : interdite en Stateflow ; d'où la variable locale `Mode_Auto_Eff`.",
    "**Niveau et front** : un bouton maintenu ne doit déclencher qu'une action (front montant).",
    "**Priorité** : en Stateflow, les transitions qui sortent d'un état parent sont évaluées avant celles de ses enfants ; dans le firmware, les transitions prioritaires sont évaluées après le switch et terminées par `return`.",
    "**Langage d'action** : le chart du modèle est en langage MATLAB (`if … end`, `~=`), pas en C.",
    "**Parallélisme** : sans régions parallèles, entrer en prolongation arrêterait la régulation, contrairement au firmware.",
  ]),
);

/* ======================================================================
   12. TRANSFORMATIONS
   ====================================================================== */
add(H1("12. Transformations entre représentations"),
  H2("12.1 Unités de temps"),
  ...TAB("Unités de temps", ["Paramètre", "Menu (opérateur)", "Firmware", "Stateflow"], [
    ["Duree_Max_Cycle", "min", "× 60 000 → ms", "s (36000 = 600 min)"],
    ["Temps_Min_Fin", "min", "× 60 000 → ms", "s (7200 = 120 min)"],
    ["Prolongation N", "min", "× 60 000 → ms", "s (Tps_Prolongation = 1800)"],
    ["Temps_Purge, Temps_Allumage, Temps_Reponse", "s", "× 1000 → ms", "s"],
  ], [3.5, 2, 2.5, 2.5]),
  H2("12.2 Noms des grandeurs"),
  ...TAB("Correspondance des noms firmware ↔ Simulink", ["Firmware", "Simulink / Stateflow", "Remarque"], [
    ["AU_Urgence", "AU_Manuel", "Nom du port d'origine du modèle"],
    ["EV1, EV2, EV3", "V_Fl_1, V_Fl_2, V_Fl_3", "Électrovannes de rampe"],
    ["PWM_Distrib / PWM_Extract", "PWM_Inj / PWM_Ext", "Ventilateurs"],
    ["Btn_Menu", "Btn_SELECT", "Défilement des choix d'erreur"],
    ["Config.Prolong_Defaut (min)", "Tps_Prolongation (s)", "Unité différente"],
    ["Config.T_init (manuel)", "T_init_manuel", "—"],
    ["T_cap (estimée)", "T_cap_est", "L'entrée T_cap du modèle n'est plus utilisée"],
    ["Config.Mode_Auto", "Mode_Auto_Eff", "Copie locale modifiable"],
  ], [3.2, 3.2, 3.6]),
  H2("12.3 Codages internes"),
  ...TAB("Codage des variables d'état", ["Variable", "Valeurs"], [
    ["palier", "0 = 100 %, 1 = 67 %, 2 = 33 %, 3 = 0 %"],
    ["Source_Active", "0 = solaire, 1 = H2, 2 = GPL"],
    ["raison_purge", "0 = démarrage, 1 = échec, 2 = bascule, 3 = palier 0 %"],
    ["cause_urgence", "1 = fuite H2, 2 = fuite GPL, 3 = arrêt d'urgence, 4 = perte de flamme, 5 = flamme parasite"],
    ["motif_demande", "0 = humidité, 1 = durée max, 2 = fin de prolongation"],
    ["Etat_LCD", "0 attente, 2 solaire, 3 H2, 4 GPL, 5 demande, 6 prolongation, 7 terminé, 8 erreur, 9 urgence"],
  ], [2.5, 7.5]),
  H2("12.4 Signaux physiques"),
  ...L([
    "**Pression H2** : transmetteur 4-20 mA sur shunt 250 Ω → 1 à 5 V → CAN 10 bits (204 à 1023) → `P = (N − 204) × 10 / (1023 − 204)` bar ; en dessous de 180 (boucle coupée) → 0 bar.",
    "**DS18B20** : −127 °C signale une sonde déconnectée ; la dernière valeur valide est conservée [23].",
    "**DHT22** : une lecture invalide (NaN) est ignorée [24].",
    "**Arrêt d'urgence** : contact NC vers 0 V ; repos = LOW, appui ou fil coupé = HIGH.",
    "**Flamme** : sortie tout-ou-rien du contrôleur de flamme, niveau configurable (`NIVEAU_FLAMME_PRESENTE`).",
  ]),
);

/* ======================================================================
   13. RÔLE DE CHAQUE ÉLÉMENT
   ====================================================================== */
add(H1("13. Rôle de chaque élément de la commande"),
  H2("13.1 Niveau conception"),
  ...TAB("Éléments de conception", ["Élément", "Nature", "Rôle"], [
    ["Cahier des charges v1, v2 + décisions v3", "Document", "Exigences ; les décisions de l'opérateur tranchent les ambiguïtés"],
    ["Machine à états (FSM)", "Spécification", "Décrit tout le comportement : états, transitions, priorités"],
    ["Firmware sechoir_hybride.ino", "Code C++ Arduino", "Implémentation exécutable ; source de vérité"],
    ["Banc de tests (tests/)", "Code C++ PC", "Exécute le firmware réel sans matériel ; 206 vérifications"],
    ["simulation_cycle.cpp", "Code C++ PC", "Firmware réel + modèle thermique illustratif → exemple de cycle"],
    ["construire_modele.m", "Script MATLAB", "Construit à partir de zéro le modèle Simulink : chart Stateflow identique au firmware, modèle physique, scénarios"],
    ["Simulation_Sechoir_Hybride.slx", "Modèle Simulink (généré)", "Simulation en boucle fermée : FSM + puissance + thermique + séchage + brûleur"],
    ["docs/FSM_SECHOIR.md, FSM_SIMULINK.md", "Documentation", "Spécifications détaillées du firmware et du modèle"],
  ], [3.5, 2.2, 4.3]),
  H2("13.2 Fonctions du firmware"),
  ...TAB("Rôle des fonctions du firmware", ["Fonction", "Rôle"], [
    ["setup()", "Sorties inactives d'abord, broches, chargement de Config, capteurs, écran"],
    ["loop()", "Boucle d'exécution : lireEntrees → pasFSM → ecrireSorties → gererIHM"],
    ["lireEntrees()", "Acquisition, fronts des boutons, valeurs FIXE, T_cap estimée, H_fin"],
    ["pasFSM()", "Machine à états : un case par état + transitions prioritaires"],
    ["majRegime()", "Repère FROID / CHAUD"],
    ["demarrerCycle()", "T_init, seuils, compteurs, choix initial de la source, purge"],
    ["etapeRegulation()", "Arbitrage de source puis solaire ou combustion"],
    ["arbitrageSource()", "Retour solaire, passage en combustion, bascule H2 ↔ GPL"],
    ["gererCombustion()", "Sous-machine PURGE → ALLUMAGE → RÉGULATION (partagée H2 / GPL)"],
    ["majPalier()", "Hystérésis à quatre niveaux"],
    ["calculerSeuils()", "T1, T2, T3 à partir de T_init et T_cible"],
    ["palierInitial()", "Palier d'allumage selon le régime ou le palier imposé"],
    ["armerPurge() / fermerGaz()", "Mise en sécurité avant toute ouverture de gaz"],
    ["declencherUrgence()", "Passage en URGENCE avec mémorisation de la cause"],
    ["entrerErreurCombustion()", "Passage en ERREUR_COMBUSTION"],
    ["changerEtat()", "Point unique de changement d'état (journal série, chronomètres d'entrée)"],
    ["ecrireSorties()", "Seul accès aux actionneurs ; verrou V_H2 / V_But"],
    ["gererIHM(), composerLCD()", "Menu, édition des paramètres, écrans"],
    ["chargerConfig(), sauverConfig(), bornerConfig()", "Persistance EEPROM et bornes de sécurité"],
    ["frontPris()", "Un appui de bouton = une seule action"],
  ], [4, 6]),
  H2("13.3 Blocs du modèle Simulink"),
  ...TAB("Blocs du modèle", ["Bloc", "Rôle"], [
    ["FSM (chart Stateflow)", "Logique de commande identique au firmware ; sorties V_H2, V_But, V_Fl_1..3, PWM, Buzzer, Etat_LCD"],
    ["CONVERSION_PUISSANCE", "Convertit l'état des électrovannes en puissance thermique injectée (Pnom = 5000 W)"],
    ["MODELE_THERMIQUE", "Réponse thermique de la chambre (Kth = 0,098 ; τ = 3530 s) → T_sec, rebouclée sur la FSM"],
    ["BRULEUR", "Capteur de flamme simulé : flamme si une vanne de gaz est ouverte (au pas précédent), sauf panne ; flamme parasite simulable"],
    ["MODELE_SECHAGE + intégrateur", "Humidité de l'air extrait H_sec (modèle illustratif), rebouclée sur la FSM"],
    ["Blocs From Workspace (sc_*)", "Scénarios : boutons, consignes, capteurs, défauts, apport solaire P_sol — définis dans `scenario_sechoir.m`"],
    ["Scope SUIVI, blocs To Workspace", "Observation en direct (T_sec, H_sec, état) et enregistrement des signaux pour les figures"],
  ], [3.5, 6.5]),
);

/* ======================================================================
   14. SIMULINK
   ====================================================================== */
add(H1("14. Modèle Simulink / Stateflow et guide de simulation"),
  H2("14.1 Structure du modèle"),
  P("Le modèle ferme la boucle de régulation : la FSM commande les électrovannes, `CONVERSION_PUISSANCE` calcule la puissance injectée, `MODELE_THERMIQUE` calcule `T_sec`, qui revient en entrée de la FSM. Le chart Stateflow reproduit exactement le firmware, avec la hiérarchie de la figure 2 : `EN_CYCLE` est un état **AND** à deux régions parallèles, dans `FONCTIONNEMENT_NORMAL` qui reste **OR** (il contient aussi ATTENTE et SECHAGE_TERMINE). Le langage d'action du chart est MATLAB ; les temporisations utilisent `after`, `temporalCount` et `duration` [19], [20], [21]."),
  H2("14.2 Mise en œuvre"),
  P("Le modèle est **construit entièrement par script, à partir de zéro** (`construire_modele.m`) : aucun fichier `.slx` n'est édité à la main, et le modèle peut être reconstruit à l'identique à tout moment. Chaque état du chart est créé directement dans son parent, ce qui garantit la hiérarchie. Une première approche, qui complétait le modèle fourni au départ, a été abandonnée : ses ports n'étaient pas câblés et son chart ne comportait aucune transition par défaut. Dans MATLAB R2025b, dossier `matlab/` :"),
  ...N([
    "`construire_modele` : crée `Simulation_Sechoir_Hybride.slx` — chart Stateflow (89 données, 20 états, transitions du firmware), câblage de ses 21 entrées, conversion de puissance, modèle thermique, séchage, brûleur, apport solaire, scénarios et enregistrement — règle le solveur, puis compile le modèle.",
    "`lancer_simulation(1)` : simule le scénario 1, affiche la chronologie des états et enregistre la figure `captures/scenario_01.png` (300 dpi). `lancer_simulation(1:12)` enchaîne les douze scénarios.",
    "`comparer_scenario(1)` : superpose la simulation Simulink et la référence du firmware (§15.3), liste l'écart de temps de chaque changement d'état et enregistre `captures/comparaison_01.png`.",
    "`capturer_modele` : exporte les images du modèle, du sous-système FSM et du chart Stateflow pour ce document.",
  ]),
  H2("14.3 Réglages faits par construire_modele"),
  ...TAB("Réglages du modèle de simulation", ["Entrée / réglage", "Valeur", "Pourquoi"], [
    ["Flame", "Bloc BRULEUR : flamme = (V_H2 OU V_But) au pas précédent, sauf panne simulée", "Une constante 1 déclencherait l'urgence « flamme parasite » gaz fermé ; une constante 0, des échecs d'allumage"],
    ["H_sec", "Bloc MODELE_SECHAGE + intégrateur (départ 90 %)", "Sinon le critère d'humidité ne se déclenche jamais"],
    ["P_sol", "Apport du capteur solaire (W), 20 / 0,098 ≈ 204 W par ciel clair", "Cohérent avec T_cap estimée = T_amb + 20 °C"],
    ["Boutons", "Impulsions de 1 s (Btn_Start à t = 10 s)", "Démarrage du cycle, réponses de l'opérateur"],
    ["Press_H2, MQ8_H2, MQ6_But, AU_Manuel", "8 bar ; 50 ; 50 ; 0", "H2 disponible, pas de fuite, pas d'arrêt d'urgence"],
    ["Tps_Prolongation", "1800 s", "Durée de prolongation proposée"],
    ["Pas de calcul", "Fixe 0,1 s (ode4), chart discret à 0,1 s", "Réactivité de la sécurité flamme"],
  ], [2.4, 3.6, 4]),
  H2("14.4 Scénarios de simulation proposés"),
  ...TAB("Scénarios de simulation et résultats attendus", ["N°", "Scénario", "Réglage", "Résultat attendu"], [
    ["1", "Démarrage à froid H2", "T_amb = 25 ; T_sec initiale 25", "Purge 120 s, allumage 100 %, puis 67, 33, 0 % (comme la figure 4)"],
    ["2", "Démarrage solaire", "T_amb = 35 (T_cap_est = 55) ; P_sol = 204 W", "MODE_SOLAIRE, aucune vanne ouverte, T_sec monte vers 55 °C"],
    ["3", "Bascule H2 → GPL", "Press_H2 : 8 → 0,5 bar à 1250 s (brûleur à 33 %)", "Purge 120 s, rallumage GPL à 33 % (palier conservé)"],
    ["4", "Retour au solaire", "T_amb et P_sol : rampe de 1200 à 2400 s", "Extinction normale + post-purge 120 s"],
    ["5", "Échecs d'allumage", "Flame forcée à 0", "3 essais espacés de purges → ERREUR_COMBUSTION"],
    ["6", "Pertes de flamme", "T_cible = 70 ; flamme perdue 5 s à 350 s et à 700 s", "Relance, puis URGENCE (cause 4) ; réarmement à 900 s"],
    ["7", "Fuite H2", "MQ8 : Step 50 → 600", "URGENCE (cause 1) ; réarmement refusé tant que MQ8 est haut"],
    ["8", "Fin par humidité", "H_sec décroissant", "DEMANDE_PROLONGATION ; sans réponse → TERMINE après 300 s"],
    ["9", "Prolongation", "Impulsion Btn_OK pendant la demande", "PROLONGATION N s, puis nouvelle demande"],
    ["10", "Mode dégradé H_sec", "Fixe_H_sec = 1", "Fin uniquement par Duree_Max_Cycle"],
    ["11", "Palier imposé", "Regul_Auto = 0, Palier_Impose = 2", "Palier 33 % quelle que soit T_sec ; T_sec atteint 90 °C vers 32 min → TERMINE"],
    ["12", "Surchauffe", "Regul_Auto = 0, Palier_Impose = 0 (100 %)", "T_sec ≥ 90 °C vers 10,5 min → SECHAGE_TERMINE"],
  ], [0.6, 2.4, 3.3, 3.7]),
  H2("14.5 Ce qu'il faut observer"),
  ...L([
    "`T_sec` avec les seuils T1, T2, T3 (vérifier l'hystérésis : figure 3).",
    "`palier`, `V_Fl_1..3`, `V_H2`, `V_But` (aucun gaz ouvert pendant une purge).",
    "`Etat_LCD` (codes du tableau 12.3) pour suivre les états.",
    "`cause_urgence`, `nb_echecs_allumage`, `nb_pertes_flamme` pour les scénarios de défaut.",
  ]),
  NOTE("Important", "Les scripts ont été vérifiés par analyse statique (chapitre 15). Les premiers essais sous MATLAB R2025b ont validé la création des données, de la hiérarchie et des transitions ; le modèle construit à partir de zéro **compile sans erreur** ; la simulation des scénarios est à confirmer."),
);

/* ======================================================================
   15. VALIDATION
   ====================================================================== */
add(H1("15. Validation"),
  H2("15.1 Banc de tests natif"),
  P("Le firmware est compilé **tel quel** sur PC : les bibliothèques matérielles (Arduino, OneWire, DallasTemperature, DHT, LiquidCrystal_I2C, EEPROM) sont remplacées par des simulations (« mocks »). Le banc règle les capteurs, appuie sur les boutons, fait avancer une horloge simulée et appelle `loop()` exactement comme la carte. Commande : `make -C tests run`."),
  ...TAB("Cas de test (206 vérifications, toutes réussies)", ["Cas", "Exigence vérifiée"], [
    ["1", "Démarrage à froid : purge → allumage → 100 %, seuils T1/T2/T3"],
    ["2", "Hystérésis à 4 niveaux, veille à 0 %, rallumage à 33 %"],
    ["3", "Échec d'allumage → purge"],
    ["4", "Perte de flamme : relance, puis URGENCE à la 2e ; réarmement"],
    ["5", "3 échecs → ERREUR_COMBUSTION ; RÉESSAYER"],
    ["6", "Bascule H2 → GPL, palier conservé"],
    ["7", "Échec après bascule : 3 essais puis ERREUR ; choix MANUEL"],
    ["8", "Retour automatique GPL → H2 avec marge"],
    ["9", "Critère d'humidité verrouillé par Temps_Min_Fin"],
    ["10", "Demande de prolongation : N réglable, prolongation, redemande, fin sans réponse, STOP"],
    ["11", "Durée max → demande ; humidité atteinte en prolongation → redemande"],
    ["12", "URGENCE : causes, deux chemins de réarmement, refus si flamme vue"],
    ["13", "Solaire passif ; régime CHAUD ; allumage à 33 % ; post-purge"],
    ["14", "Exclusion mutuelle V_H2 / V_But"],
    ["15", "Menu et persistance EEPROM"],
    ["16", "STOP depuis la combustion"],
    ["17", "Repère FROID / CHAUD au démarrage et en cycle"],
    ["18", "Flamme vue gaz fermé → URGENCE après 5 s"],
    ["19", "Grandeurs FIXE"],
    ["20", "Palier imposé ; surchauffe toujours active"],
    ["21", "T_cible modifiée en cycle ; Periode_Regul"],
    ["22", "Bascule H2 → GPL pendant la veille à 0 % : veille conservée, aucune ouverture de gaz avant le retour de la demande (défaut trouvé par la simulation, §15.3)"],
  ], [1, 9]),
  H2("15.2 Tests par mutation"),
  P("Un banc qui réussit tout ne prouve rien s'il ne sait pas détecter d'erreur. Le **test par mutation** consiste à introduire volontairement un défaut dans le code (une « mutation ») et à vérifier que le banc échoue [17], [18]. Onze mutations, chacune désactivant une règle de la version 3 (tolérer la 2e perte de flamme, ignorer la flamme parasite, supprimer le repère, allumer toujours à 100 %, etc.), ont **toutes** été détectées ; le code restauré repasse toutes les vérifications."),
  H2("15.3 Simulation de référence des scénarios et défaut trouvé"),
  P("Le programme `tests/simulation_scenarios.cpp` exécute le **firmware réel** sur les douze scénarios du tableau 14.4, bouclé sur le **même modèle physique** que Simulink (Pnom = 5000 W, Kth = 0,098 K/W, τ = 3530 s, même brûleur, même séchage). Les scénarios sont lus dans `scenario_sechoir.m` : une seule définition sert aux deux simulations. Les courbes obtenues (`matlab/reference/`) sont les **résultats attendus** du modèle Stateflow ; `comparer_scenario` mesure l'écart entre les deux."),
  P("Cette simulation a mis en évidence un défaut que le banc de tests ne couvrait pas. Quand la bascule H2 → GPL survient **pendant la veille à 0 %**, la purge de bascule se terminait par un allumage **au palier 0 %** : vanne source GPL et étincelle actives, aucune électrovanne de rampe ouverte. La flamme détectée renvoyait aussitôt en purge sans relancer la temporisation, et la vanne GPL s'ouvrait et se fermait à chaque itération. Sur le prototype, sans électrovanne de rampe ouverte, la flamme ne s'établirait pas : trois échecs d'allumage et une ERREUR_COMBUSTION injustifiée. Correction : en fin de purge, la règle de la veille (attendre T_sec < T3 − Hhyst/2 puis rallumer à 33 %) s'applique dès que le palier vaut 0 %, quelle que soit la raison de la purge. La même correction est portée dans le chart Stateflow, et le cas de test 22 vérifie ce comportement."),
  H2("15.4 Vérification du modèle Stateflow"),
  P("Le script de construction et les libellés qu'il génère (actions des 20 états, conditions et actions des transitions) ont été analysés par **MISS_HIT** [29], un analyseur statique du langage MATLAB : syntaxe correcte. Il a aussi été vérifié que chaque variable utilisée est déclarée et qu'aucune entrée n'est écrite. Sous MATLAB R2025b, les premiers essais ont révélé deux problèmes, corrigés : une syntaxe d'appel refusée par cette version (`sfroot.find`), et l'absence de transition par défaut dans le chart d'origine — raison du passage à une construction complète à partir de zéro."),
  H2("15.5 Ce qui reste à valider"),
  ...L([
    "Exécution des scripts dans MATLAB/Simulink, puis comparaison des douze scénarios avec la référence du firmware (`comparer_scenario`).",
    "Calibrage de `DeltaT_Sol` et des seuils MQ8/MQ6 sur site (24 h de préchauffage des capteurs MQ).",
    "Comportement réel du capteur de flamme (voir chapitre 17).",
    "Adresse I2C et affichage réel de l'écran 20×4.",
  ]),
);

/* ======================================================================
   16. LOGICIELS
   ====================================================================== */
add(H1("16. Logiciels et outils utilisés"),
  ...TAB("Logiciels, bibliothèques et outils", ["Outil", "Usage dans le projet", "Source"], [
    ["Arduino IDE + cœur AVR", "Compilation et téléversement du firmware sur l'Arduino Mega 2560", "[30]"],
    ["Bibliothèque OneWire", "Bus 1-Wire des sondes DS18B20", "[25]"],
    ["Bibliothèque DallasTemperature", "Lecture asynchrone des DS18B20", "[26]"],
    ["Bibliothèque DHT (Adafruit)", "Capteurs DHT22 d'humidité et de température", "[27]"],
    ["Bibliothèque LiquidCrystal_I2C", "Écran LCD 20×4 I2C", "[28]"],
    ["Bibliothèque EEPROM (cœur Arduino)", "Sauvegarde de la configuration", "[22], [30]"],
    ["GCC / g++ et GNU Make", "Compilation native du banc de tests et des simulations de référence", "[34]"],
    ["GNU Octave", "Écriture des scénarios pour le banc firmware ; vérification des scripts de tracé hors MATLAB", "[35]"],
    ["MATLAB, Simulink, Stateflow (R2025b)", "Modèle de la commande et simulation", "[31]"],
    ["MISS_HIT", "Analyse statique du script MATLAB et des libellés Stateflow", "[29]"],
    ["Python + Matplotlib", "Figures de ce document (hystérésis, cycle simulé, schémas)", "[32]"],
    ["Git / GitHub", "Gestion des versions du code et de la documentation", "[33]"],
    ["Microsoft Word", "Rédaction du mémoire", "—"],
    ["Claude Code (Anthropic)", "Assistant d'intelligence artificielle utilisé pour le développement du firmware, des tests, du modèle et de la documentation, sous la direction et la validation de l'auteur", "—"],
  ], [3, 5, 2]),
  P("_Note d'intégrité : l'usage d'un assistant d'IA est indiqué par transparence. Les choix de conception, les décisions de sécurité et la validation restent sous la responsabilité de l'auteur ; vérifier la politique de l'établissement concernant la déclaration de ces outils._"),
);

/* ======================================================================
   17. ANALYSE CRITIQUE
   ====================================================================== */
add(H1("17. Analyse critique, limites et perspectives"),
  H2("17.1 Limites identifiées"),
  ...L([
    "**Estimation de T_cap** : `T_amb + DeltaT_Sol` ne mesure pas l'ensoleillement. Une journée chaude et couverte peut être prise pour une journée ensoleillée, et une journée froide mais très ensoleillée être sous-exploitée. Le modèle physique d'un capteur fait intervenir l'éclairement G et le rendement [5].",
    "**Formule H_fin = H_produit + H_amb** : elle additionne une humidité visée du produit et l'humidité relative de l'air extérieur ; elle doit être justifiée physiquement (psychrométrie, isothermes de sorption [6]).",
    "**Cycles répétés au palier 0 %** : chaque rallumage impose une purge et use l'allumeur (six cycles par heure dans l'exemple simulé de la figure 4). La veille sans relance du chronomètre de purge limite ce coût, sans le supprimer.",
    "**Capteur de flamme** : un capteur UV peut voir l'étincelle d'allumage ; un capteur IR peut voir le rayonnement de l'absorbeur chaud (brûleur intégré au capteur). Une fausse détection serait signalée par l'urgence « flamme parasite ».",
    "**Palier imposé** : si la sonde T_sec est défaillante, la sécurité surchauffe logicielle est aveugle. Un **thermostat de sécurité matériel indépendant** à réarmement manuel est recommandé.",
    "**Paramètres de sécurité** modifiables depuis le menu de conduite (bornés) : un menu technique protégé serait préférable.",
    "`H_initial` est saisie mais n'intervient pas dans la décision de fin.",
  ]),
  H2("17.2 Perspectives"),
  ...L([
    "**Scénarios de cycle enregistrés** : jeux de paramètres complets prêts à l'emploi (par produit), choisis par l'opérateur au démarrage — prochaine évolution prévue.",
    "Estimation solaire plus juste : lire la vraie sonde seulement quand le brûleur est éteint depuis un certain temps, ou ajouter un capteur de lumière (T_cap = T_amb + Ccap × G).",
    "Identification du modèle thermique réel du séchoir et comparaison simulation / mesures.",
    "Journalisation des cycles (carte SD) pour l'analyse énergétique : part solaire, H2, GPL.",
  ]),
);

/* ======================================================================
   BIBLIOGRAPHIE
   ====================================================================== */
const biblio = [
  ["D. Harel, « Statecharts: a visual formalism for complex systems », _Science of Computer Programming_, vol. 8, n° 3, pp. 231–274, 1987.", "https://doi.org/10.1016/0167-6423(87)90035-9"],
  ["E. A. Lee et S. A. Seshia, _Introduction to Embedded Systems: A Cyber-Physical Systems Approach_, 2e éd., MIT Press, 2017.", "https://ptolemy.berkeley.edu/books/leeseshia/"],
  ["M. Samek, _Practical UML Statecharts in C/C++: Event-Driven Programming for Embedded Systems_, 2e éd., Newnes, 2008, ISBN 978-0-7506-8706-5.", "https://www.state-machine.com/psicc2"],
  ["K. J. Åström et R. M. Murray, _Feedback Systems: An Introduction for Scientists and Engineers_, 2e éd., Princeton University Press, 2021.", "https://fbswiki.org/wiki/index.php/Feedback_Systems:_An_Introduction_for_Scientists_and_Engineers"],
  ["J. A. Duffie et W. A. Beckman, _Solar Engineering of Thermal Processes_, 4e éd., John Wiley & Sons, 2013.", "https://doi.org/10.1002/9781118671603"],
  ["A. S. Mujumdar (dir.), _Handbook of Industrial Drying_, 4e éd., CRC Press, 2014, ISBN 978-1-4665-9665-8.", "https://doi.org/10.1201/b17208"],
  ["V. Belessiotis et E. Delyannis, « Solar drying », _Solar Energy_, vol. 85, n° 8, pp. 1665–1691, 2011.", "https://doi.org/10.1016/j.solener.2009.10.001"],
  ["O. V. Ekechukwu et B. Norton, « Review of solar-energy drying systems II: an overview of solar drying technology », _Energy Conversion and Management_, vol. 40, n° 6, pp. 615–655, 1999.", "https://doi.org/10.1016/S0196-8904(98)00093-4"],
  ["A. Fudholi, K. Sopian, M. H. Ruslan, M. A. Alghoul et M. Y. Sulaiman, « Review of solar dryers for agricultural and marine products », _Renewable and Sustainable Energy Reviews_, vol. 14, n° 1, pp. 1–30, 2010.", "https://doi.org/10.1016/j.rser.2009.07.032"],
  ["E. C. López-Vidaña _et al._, « Efficiency of a hybrid solar–gas dryer », _Solar Energy_, vol. 93, pp. 23–31, 2013.", "https://www.sciencedirect.com/science/article/abs/pii/S0038092X13000546"],
  ["S. Murali, P. R. Amulya, P. V. Alfiya, D. S. Aniesrani Delfiya et M. P. Samuel, « Design and performance evaluation of solar – LPG hybrid dryer for drying of shrimps », _Renewable Energy_, vol. 147, pp. 2417–2428, 2020.", "https://www.sciencedirect.com/science/article/abs/pii/S0960148119314934"],
  ["CEN, EN 298:2022, _Automatic burner control systems for burners and appliances burning gaseous or liquid fuels_, 2022.", "https://standards.iteh.ai/catalog/standards/cen/8d0d6146-ea22-4c79-9626-38cc9d3ab760/en-298-2022"],
  ["Parlement européen et Conseil, Directive 2014/34/UE relative aux appareils et systèmes de protection destinés à être utilisés en atmosphères explosibles (ATEX), 2014.", "https://eur-lex.europa.eu/legal-content/EN/TXT/PDF/?uri=CELEX:32014L0034"],
  ["CEI 60079-10-1, _Atmosphères explosives — Partie 10-1 : Classement des emplacements — Atmosphères explosives gazeuses_.", null],
  ["CEI 61508, _Sécurité fonctionnelle des systèmes électriques/électroniques/électroniques programmables relatifs à la sécurité_.", null],
  ["Y. S. H. Najjar, « Hydrogen safety: The road toward green technology », _International Journal of Hydrogen Energy_, vol. 38, n° 25, pp. 10716–10728, 2013.", "https://www.sciencedirect.com/science/article/abs/pii/S036031991301358X"],
  ["R. A. DeMillo, R. J. Lipton et F. G. Sayward, « Hints on test data selection: Help for the practicing programmer », _Computer_, vol. 11, n° 4, pp. 34–41, 1978.", "https://doi.org/10.1109/C-M.1978.218136"],
  ["Y. Jia et M. Harman, « An analysis and survey of the development of mutation testing », _IEEE Transactions on Software Engineering_, vol. 37, n° 5, pp. 649–678, 2011.", "https://doi.org/10.1109/TSE.2010.62"],
  ["MathWorks, « Control Chart Execution by Using Temporal Logic », documentation Stateflow.", "https://www.mathworks.com/help/stateflow/ug/using-temporal-logic-in-state-actions-and-transitions.html"],
  ["MathWorks, « temporalCount », documentation Stateflow.", "https://www.mathworks.com/help/stateflow/ref/temporalcount.html"],
  ["MathWorks, « duration », documentation Stateflow.", "https://www.mathworks.com/help/stateflow/ref/duration.html"],
  ["Arduino, « Mega 2560 Rev3 », documentation officielle (ATmega2560, 4 Ko d'EEPROM).", "https://docs.arduino.cc/hardware/mega-2560/"],
  ["Analog Devices (Maxim Integrated), _DS18B20 Programmable Resolution 1-Wire Digital Thermometer_, fiche technique.", "https://analog.com/media/en/technical-documentation/data-sheets/ds18b20.pdf"],
  ["Adafruit, « DHT22 temperature-humidity sensor » (AM2302), fiche produit et documentation.", "https://www.adafruit.com/product/385"],
  ["P. Stoffregen, bibliothèque OneWire pour Arduino.", "https://github.com/PaulStoffregen/OneWire"],
  ["M. Burton, bibliothèque Arduino-Temperature-Control-Library (DallasTemperature).", "https://github.com/milesburton/Arduino-Temperature-Control-Library"],
  ["Adafruit, DHT-sensor-library.", "https://github.com/adafruit/DHT-sensor-library"],
  ["Bibliothèque LiquidCrystal_I2C pour Arduino.", "https://github.com/johnrickman/LiquidCrystal_I2C"],
  ["F. Schanda, MISS_HIT — MATLAB Independent, Small & Safe, High Integrity Tools.", "https://misshit.org/"],
  ["Arduino, Arduino IDE (logiciel).", "https://www.arduino.cc/en/software"],
  ["MathWorks, Simulink et Stateflow (produits).", "https://www.mathworks.com/products/stateflow.html"],
  ["J. D. Hunter, « Matplotlib: A 2D graphics environment », _Computing in Science & Engineering_, vol. 9, n° 3, pp. 90–95, 2007.", "https://doi.org/10.1109/MCSE.2007.55"],
  ["Git, système de gestion de versions.", "https://git-scm.com/"],
  ["GNU Compiler Collection (GCC).", "https://gcc.gnu.org/"],
  ["J. W. Eaton, D. Bateman, S. Hauberg et R. Wehbring, _GNU Octave — manuel de référence_.", "https://octave.org/doc/"],
];
add(H1("Bibliographie"),
  P("Références citées dans le texte par leur numéro entre crochets. Les liens renvoient directement à la source (DOI, éditeur ou documentation officielle)."),
  ...biblio.map(([texte, url], i) => new Paragraph({
    spacing: { after: 100 }, indent: { left: 567, hanging: 567 }, alignment: AlignmentType.LEFT,
    children: [
      new TextRun({ text: `[${i + 1}]\t`, bold: true }),
      ...runs(texte),
      ...(url ? [new TextRun(" "), new ExternalHyperlink({ link: url, children: [new TextRun({ text: url, style: "Hyperlink", size: 19 })] })] : []),
    ],
  })),
  NOTE("À vérifier avant dépôt", "Les références ont été contrôlées par recherche en ligne (titre, auteurs, revue, année, DOI). Les deux normes CEI sont citées sans édition précise : indiquer l'édition réellement consultée. Adapter le style (IEEE, APA…) aux consignes de l'établissement."),
);

/* ======================================================================
   ANNEXES
   ====================================================================== */
add(H1("Annexe A — Brochage de l'Arduino Mega 2560"),
  ...TAB("Brochage complet", ["Broche", "Signal", "Sens", "Remarque"], [
    ["22", "Bus 1-Wire (T_sec index 0, T_cap index 1)", "E", "DS18B20, résistance de tirage 4,7 kΩ"],
    ["24 / 26", "DHT22 ambiant / DHT22 extraction", "E", "T_amb, H_amb / H_sec"],
    ["28", "Contrôleur de flamme", "E", "TOR"],
    ["30, 32, 34, 36", "MENU, UP, DOWN, OK", "E", "INPUT_PULLUP, contact vers 0 V"],
    ["38, 40, 44", "START, STOP, RÉARMEMENT", "E", "INPUT_PULLUP"],
    ["42", "Arrêt d'urgence", "E", "Contact NC, fil coupé = urgence"],
    ["A0, A1", "MQ-8 (H2), MQ-6 (GPL)", "E ana.", "MQ-8 en partie haute, MQ-6 près du sol"],
    ["A2", "Pression H2 (4-20 mA / 250 Ω)", "E ana.", "0 à 10 bar"],
    ["23, 25", "V_H2, V_But", "S", "Électrovannes de source"],
    ["27, 29, 31", "EV1, EV2, EV3", "S", "Électrovannes de rampe"],
    ["33", "Spark", "S", "Allumage"],
    ["35", "Buzzer", "S", "Alarme"],
    ["2, 3, 4", "PWM purge, distribution, extraction", "S PWM", "Ventilateurs"],
    ["SDA / SCL", "LCD 20×4 I2C", "Bus", "Adresse 0x27 ou 0x3F"],
  ], [1.6, 3.6, 1.2, 3.6]),
  H1("Annexe B — Glossaire"),
  ...TAB("Glossaire", ["Terme", "Définition"], [
    ["ATEX", "ATmosphères EXplosibles : réglementation européenne sur les équipements en zones explosives [13]"],
    ["EEPROM", "Mémoire non volatile interne du microcontrôleur, conservée hors tension"],
    ["FSM", "Finite State Machine, machine à états finis"],
    ["Hystérésis", "Écart entre seuil de montée et seuil de descente, qui évite les oscillations"],
    ["Mock", "Imitation logicielle d'un composant matériel, pour tester sans matériel"],
    ["Palier", "Niveau de puissance du brûleur : 100, 67, 33 ou 0 %"],
    ["Purge", "Balayage d'air de la chambre de combustion avant toute ouverture de gaz"],
    ["Régime FROID / CHAUD", "Repère indiquant si la chambre est déjà chaude (T_sec ≥ Seuil_Chaud)"],
    ["Région parallèle", "Partie d'un état AND, active en même temps que les autres régions"],
    ["Stateflow", "Outil MathWorks de modélisation par statecharts, intégré à Simulink"],
    ["Test par mutation", "Technique qui évalue un banc de tests en y injectant des défauts volontaires [17], [18]"],
  ], [3, 7]),
);

/* ======================================================================
   DOCUMENT
   ====================================================================== */
const doc = new Document({
  creator: "Séchoir solaire hybride",
  title: "Commande en température d'un séchoir solaire hybride",
  features: { updateFields: true },
  styles: {
    default: { document: { run: { font: "Calibri", size: 22 } } },
    paragraphStyles: [
      { id: "Heading1", name: "Heading 1", basedOn: "Normal", next: "Normal", quickFormat: true,
        run: { size: 32, bold: true, color: "1F3864", font: "Calibri" },
        paragraph: { spacing: { before: 240, after: 200 }, outlineLevel: 0,
          border: { bottom: { style: BorderStyle.SINGLE, size: 8, color: "1F3864", space: 4 } } } },
      { id: "Heading2", name: "Heading 2", basedOn: "Normal", next: "Normal", quickFormat: true,
        run: { size: 26, bold: true, color: "2B6CB0", font: "Calibri" },
        paragraph: { spacing: { before: 280, after: 120 }, outlineLevel: 1, keepNext: true } },
      { id: "Heading3", name: "Heading 3", basedOn: "Normal", next: "Normal", quickFormat: true,
        run: { size: 23, bold: true, color: "4A5568", font: "Calibri" },
        paragraph: { spacing: { before: 200, after: 100 }, outlineLevel: 2, keepNext: true } },
    ],
  },
  numbering: { config: [
    { reference: "puces", levels: [
      { level: 0, format: LevelFormat.BULLET, text: "•", alignment: AlignmentType.LEFT, style: { paragraph: { indent: { left: 567, hanging: 283 } } } },
      { level: 1, format: LevelFormat.BULLET, text: "–", alignment: AlignmentType.LEFT, style: { paragraph: { indent: { left: 1134, hanging: 283 } } } },
    ] },
    { reference: "numeros", levels: [
      { level: 0, format: LevelFormat.DECIMAL, text: "%1.", alignment: AlignmentType.LEFT, style: { paragraph: { indent: { left: 567, hanging: 340 } } } },
    ] },
  ] },
  sections: [{
    properties: { page: { size: { width: 11906, height: 16838 }, margin: { top: 1418, bottom: 1418, left: 1418, right: 1418 } }, titlePage: true },
    headers: { default: new Header({ children: [new Paragraph({ alignment: AlignmentType.RIGHT, children: [new TextRun({ text: "Commande en température d'un séchoir solaire hybride", size: 17, color: "718096", italics: true })] })] }) },
    footers: {
      default: new Footer({ children: [new Paragraph({ alignment: AlignmentType.CENTER, children: [
        new TextRun({ size: 18, color: "718096", children: ["Page ", PageNumber.CURRENT, " / ", PageNumber.TOTAL_PAGES] }),
      ] })] }),
      first: new Footer({ children: [new Paragraph("")] }),
    },
    children: c,
  }],
});

Packer.toBuffer(doc).then(b => {
  fs.writeFileSync(__dirname + "/../Commande_Sechoir_Solaire_Hybride.docx", b);
  console.log("ok", b.length);
});
