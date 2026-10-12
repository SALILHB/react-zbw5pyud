# Tests de l'interface opérateur

## 1. Tests sur PC (sans matériel)

Le programme `sechoir_hybride.ino` est compilé tel quel sur PC avec `g++` et les simulacres de `tests/mocks/` (`Serial` et `Serial1` simulés). Linux et macOS : `g++` et `make` natifs. Windows : MSYS2 (MinGW-w64) ou WSL.

```bash
cd tests
make tout          # les trois bancs ci-dessous
make run           # banc principal du programme (pupitre LCD) : 206 vérifications
make liaison       # banc de la liaison, sans pupitre (72) puis avec pupitre (79)
make comparaison   # historique d'états : origine = pupitre = commandes à distance
make clean
```

Résultat attendu :

```
Verifications : 206   Echecs : 0      (make run)
Verifications : 72    Echecs : 0      (make liaison, AVEC_PUPITRE_LCD 0)
Verifications : 79    Echecs : 0      (make liaison, AVEC_PUPITRE_LCD 1)
Historiques identiques (47 changements) : origine = pupitre = commandes a distance
```

Des avertissements `-Wformat-truncation` à la compilation sont normaux : `snprintf` tronque proprement les lignes de l'écran LCD.

### Ce que vérifie `make liaison` (`tests/test_liaison.cpp`)

| Cas | Vérification |
|---|---|
| 1 | Télémesure chaque seconde ; mêmes lignes sur l'USB et sur `Serial1` ; lignes de débogage entières, jamais mêlées à une trame |
| 2 | Émission non bloquante : avec un tampon d'émission de 10 octets, jamais plus que la place libre par tour, trames complètes |
| 3 | `GET META` : 31 champs, bornes et pas des constantes du programme. `GET CFG` : 31 valeurs |
| 4 | `SET` hors bornes, puis valeur bornée renvoyée dans l'accusé (`t_cible 200` → 90.0, `temps_purge 5` → 60, `seuil_mq8 2000` → 900, liste hors plage → valeur par défaut). Clé inconnue ou valeur non numérique refusées. Pas d'écriture EEPROM sans `SAVE`. En cycle, `SET` refusé sauf `t_cible` (enregistrée comme UP/DOWN). `SAVE` refusé en cycle |
| 5 | `START` accepté seulement en ATTENTE et TERMINE, refusé en cycle et en urgence. `STOP` accepté en SOLAIRE, H2, GPL, DEMANDE, PROLONGATION et ERREUR (→ TERMINE), sans effet en ATTENTE. `PROLONG <min>` borné, `PROLONG OK`, `TERMINER`. Commandes par `Serial1` (ESP32) |
| 6 | `ERREUR REESSAYER\|MANUEL\|AUTO` : même état d'arrivée que MENU + OK au pupitre (avec pupitre : comparaison directe avec les vrais boutons). Refusée hors ERREUR_COMBUSTION ; choix inconnu refusé |
| 7 | En URGENCE, aucune commande ne fait sortir de l'urgence (START, STOP, SET, SAVE, PROLONG, TERMINER, ERREUR, PING, GET), ni par l'USB ni par `Serial1`. `REARM` refusé tant que la cause est présente (fuite H2, fuite GPL, AU enfoncé, flamme), avec la raison ; accepté sinon → ATTENTE. L'AU matériel reste prioritaire sur un STOP simultané |
| 8 | Ligne de 200 caractères et commande inconnue : rejet sans plantage. Commande en minuscules. Commande reçue en deux morceaux, fin de ligne CRLF |
| 9 | Avec pupitre : STOP distant = STOP bouton ; `SET` refusé pendant le menu local ; `START` accepté depuis le menu |

### Ce que vérifie `make comparaison` (`tests/comparaison_historique.cpp`)

Un même scénario de 90 min est joué par trois programmes :
- la **copie figée du programme validé** (`tests/reference_origine/`), conduite par les boutons ;
- le programme avec liaison et `AVEC_PUPITRE_LCD 1`, conduit par les boutons ;
- le programme `AVEC_PUPITRE_LCD 0`, conduit **par les commandes à distance** au lieu des boutons.

Le scénario comprend :
- démarrage, régulation et veilles ;
- consigne changée en cycle ;
- bascule H2 → GPL, perte de flamme et échecs d'allumage ;
- ERREUR_COMBUSTION et reprise, puis retour H2 ;
- demande, prolongation et fin ;
- fuite H2, urgence et réarmement ;
- arrêt.

Les trois historiques (instant, état, source, phase, palier) doivent être identiques à la ligne près.

### Autres vérifications faites lors du développement

- `make reference` (16 scénarios de référence utilisés par MATLAB) avec le programme modifié : fichiers `matlab/reference/scenario_NN.csv` identiques octet pour octet.
- Page `interface/index.html` dans Chromium (Playwright) :
  - en mode démo ;
  - puis **avec le vrai programme compilé sur PC**, derrière une passerelle WebSocket qui joue le rôle de l'ESP32, avec deux clients (un pilote, un en lecture seule) ;
  - aucune erreur JavaScript, pas de débordement horizontal à 360 px.

## 2. Compilation pour la carte (Arduino IDE)

Le dossier `sechoir_hybride/` doit contenir `sechoir_hybride.ino`, `liaison_interface.h` et `liaison_interface_impl.h` : l'IDE les ouvre comme trois onglets. Choisir la carte *Arduino Mega or Mega 2560*, puis *Vérifier*.

Rapport mémoire obtenu avec avr-gcc 7.3.0 (cœur Arduino AVR ; OneWire 2.3.8, DallasTemperature 4.0.6, DHT 1.4.7, LiquidCrystal_I2C 1.1.x) :

| Version | Mémoire programme | Mémoire vive (variables globales) |
|---|---|---|
| Programme validé, sans interface | 33 826 o (12,9 %) | 3 410 o (41,6 %) |
| `AVEC_PUPITRE_LCD 0` (défaut) | 40 398 o (15,4 %) | 3 029 o (37,0 %) |
| `AVEC_PUPITRE_LCD 1` | 47 650 o (18,2 %) | 4 363 o (53,3 %) |

Limite fixée : 75 % de la SRAM. Sans pupitre, la mémoire vive diminue : les textes de l'écran ne sont plus compilés.

## 3. Plan d'essai sur le matériel réel

> **Gaz toujours fermé** (robinets des bouteilles et du réservoir H2 fermés) pendant toutes les premières étapes. L'arrêt d'urgence câblé D42 doit être en place et essayé avant tout.

### Étape 1 : USB seul, mode démo (aucun matériel)

1. Ouvrir `interface/index.html` dans Chrome ou Edge sur l'ordinateur, cliquer **Mode démo**.
2. Faire le tour des onglets ; essayer START, la consigne ±1 °C et STOP.
3. Essayer chaque bouton de la barre démo (panne d'allumeur, fuite H2, arrêt d'urgence, durée max) : écran orange d'erreur, écran rouge d'urgence (Réarmer grisé tant que la cause est présente), demande de prolongation.
4. Onglet Courbes : exporter un CSV et l'ouvrir dans Excel.

### Étape 2 : Mega réel sans charge

Le Mega est alimenté par l'USB. Les modules relais ne sont pas reliés aux électrovannes ni à l'allumeur, ou leurs bornes de puissance sont débranchées. Les capteurs sont branchés.

1. Téléverser le programme (`AVEC_PUPITRE_LCD 0`). Fermer le moniteur série de l'IDE : un seul programme peut ouvrir le port.
2. Ouvrir la page, cliquer **Connecter USB**, choisir le port du Mega. Le Mega redémarre à l'ouverture du port ; la page relit la configuration au bout de 2 s.
3. Contrôler les mesures du tableau de bord : T_sec, T_amb et H_amb (comparer avec un thermomètre), H_sec, MQ-8 et MQ-6 au repos (noter les valeurs), pression H2.
4. **Arrêt d'urgence :** appuyer sur D42 → écran rouge, cause « Arrêt d'urgence enfoncé », Réarmer grisé. Déverrouiller D42 → Réarmer actif → réarmer → ATTENTE. Débrancher le fil de D42 → urgence aussi (contact NC).
5. **Paramètres :** essayer d'envoyer une valeur hors bornes (par exemple purge 5 s) → la page affiche la valeur retenue par le Mega (60 s). Cliquer Relire. Ne cliquer **Enregistrer en EEPROM** que pour garder les réglages.
6. **Cycle à vide :** START. Écouter les relais : purge (ventilateurs), puis allumage. Sans gaz ni flamme, on obtient trois échecs puis l'écran orange ERREUR_COMBUSTION, avec le buzzer D35. Essayer « Passer en manuel », puis un nouveau START et « Réessayer », puis STOP.
7. **Perte de liaison :** pendant un cycle, débrancher le câble USB. Les relais et ventilateurs continuent (le Mega est seul maître), le buzzer sonne en cas d'erreur. Rebrancher : la page affiche « Liaison perdue » puis se reconnecte avec **Connecter USB**.

   > **Ouvrir le port USB redémarre le Mega** (réinitialisation automatique de l'Arduino par le signal DTR) : il revient en ATTENTE, gaz fermé, et le cycle en cours est perdu. Ne pas connecter la page USB pendant un vrai séchage. Le Wi-Fi par l'ESP32 (étape 3) n'a pas ce défaut. Un condensateur de 10 µF entre RESET et GND supprime ce redémarrage, mais il faut le retirer pour téléverser.

8. Noter tout écart dans le journal (onglet Journal → Exporter).

### Étape 3 : ESP32 (étape B du projet)

Câblage, téléversement et connexion du téléphone : `docs/CABLAGE_ESP32.md` (étape B). Refaire les points 3 à 7 de l'étape 2 depuis le téléphone, puis avec deux téléphones : vérifier qu'un seul pilote, que l'autre est en lecture seule et que STOP marche des deux.

### Étape 4 : premiers essais avec gaz

À faire seulement après les étapes 1 à 3, en présence d'un responsable, local ventilé, détecteurs MQ calibrés (24 h de préchauffage). Contrôler sur place le premier allumage. L'arrêt d'urgence câblé reste le seul arrêt de sécurité ; l'interface ne le remplace pas.
