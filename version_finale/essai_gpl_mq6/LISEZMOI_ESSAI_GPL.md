# Essai au GPL seul : programme de test complet (MQ-6)

Programme pour Arduino Mega 2560. Il fait fonctionner tout le séchoir **au GPL uniquement** : l'hydrogène n'est jamais commandé. On le conduit depuis l'écran et le clavier, ou depuis la page HTML.

Il reprend le programme d'essai `essai_gpl_mq4` déjà testé, avec les corrections du banc du 9 novembre 2026. Une seule différence : le **vrai MQ-6** remplace le MQ-4 provisoire. L'entrée, le seuil et la logique de sécurité sont les mêmes.

## Contenu

| Élément | Rôle |
|---|---|
| `essai_gpl_mq6/essai_gpl_mq6.ino` | Programme d'essai GPL seul |
| `essai_gpl_mq6/lcd_pcf8574.h` | Pilote de l'écran I2C, sans bibliothèque à installer |
| `essai_gpl_mq6/liaison_interface.h` et `liaison_interface_impl.h` | Liaison avec la page de commande |
| `essai_gpl_mq6/interface_operateur.html` | Interface HTML de commande, à ouvrir dans Chrome ou Edge |
| `croquis_de_test/` | Tests de l'écran, du clavier et du ventilateur D4, déjà validés sur la carte |
| `preuves_tests/` | Compilation, résultats des tests, différences avec la version MQ-4, sources des tests |
| `captures_interface/` | La page HTML en train de piloter ce programme |

**Gardez les 5 fichiers du dossier `essai_gpl_mq6/` ensemble.**

## Câblage des éléments de l'essai

| Élément | Broche Mega | Remarque |
|---|---|---|
| Capteur de gaz **MQ-6** (sortie AO) | **A1** | VCC 5 V, GND ; seuil `Seuil_MQ6` = 350 par défaut |
| **Détecteur de flamme** (sortie DO) | **D28** | `NIVEAU_FLAMME_PRESENTE = HIGH` par défaut (voir § Réglages) |
| **DHT22 air extérieur** | **D24** | Donne T_amb et H_amb |
| **DHT22 extraction** | **D26** | Donne H_sec |
| **2 x DS18B20** (même bus 1-Wire) | **D22** | Résistance de 4,7 kΩ entre DATA et 5 V ; index 0 = T_sec, index 1 = T_cap |
| **Ventilateurs de purge** | **D2** (PWM) | Étage EL817, puis IRF510 |
| **Ventilateurs de distribution** | **D3** (PWM) | Même étage |
| **Ventilateurs d'extraction** | **D4** (PWM) | Même étage |
| **Électrovanne GPL V_But** | **D25** | Relais 1 voie A, polarité **À VÉRIFIER** |
| **EV1 / EV2 / EV3** (brûleur) | **D27 / D29 / D31** | Module 4 relais, actifs à l'état bas |
| **Module étincelle** (allumeur) | **D33** | Relais 1 voie B, polarité **À VÉRIFIER** |
| **Buzzer** | **D35** | Actif à l'état haut |
| **Écran LCD 20x4 I2C** | **SDA D20, SCL D21** | 5 V, GND ; adresse 0x21, cherchée au démarrage |
| **Boutons : clavier 4x4**, fils 1 à 8 | **D30 D32 D34 D36 D38 D40 D44 D46** | Brochage relevé sur votre clavier |
| **Bouton d'arrêt d'urgence** (contact NC vers GND) | **D42** | Jamais sur le clavier ; un fil coupé déclenche l'urgence |
| **Interface HTML** | **câble USB** | Bouton « Connecter USB » de la page |

**Non raccordés pendant cet essai :** V_H2 (D23), MQ-8 (A0), capteur de pression H2 (A2). Le programme les ignore : V_H2 n'est jamais commandée, et le MQ-8 et la pression sont forcés à 0.

## Touches du clavier

| Touche | Fonction |
|---|---|
| S1 | MENU |
| S2 | + |
| S10 | − |
| S6 | OK |
| S4 | START |
| S8 | STOP |
| S16 | RÉARMEMENT |

## Ce que fait le programme

**Au démarrage**
- État ATTENTE, mode **MANUEL**, source **GPL**.
- Le port série (115200 bauds) affiche :
  - l'adresse de l'écran ;
  - `[ESSAI GPL] GPL seul, MQ-6 sur A1…` ;
  - le **nombre de sondes DS18B20** (il doit être **2**).

**Au START**
1. Purge de **120 s** : gaz fermé, ventilateur de purge à fond.
2. Les électrovannes **EV1–EV3 s'ouvrent et l'étincelle démarre**.
3. **1 s plus tard, V_But s'ouvre.**
4. La flamme doit être vue dans les **4 s**.
5. Une fois la flamme confirmée, l'étincelle s'arrête et la régulation par paliers commence : 100 / 67 / 33 / 0 %.

**Sans flamme**
- 3 essais, puis **ERREUR_COMBUSTION**.
- L'opérateur choisit RÉESSAYER, MANUEL ou AUTO, au clavier ou depuis la page.

**URGENCE**
- Elle se déclenche sur :
  - MQ-6 au-dessus du seuil ;
  - bouton d'arrêt d'urgence ;
  - perte de flamme après une relance ;
  - flamme vue alors que le gaz est fermé (5 s).
- Effet : tout est fermé dans le même tour de boucle, buzzer, purge à fond.
- Le réarmement est refusé tant que la cause est présente.

**Écran et ventilateurs**
- L'écran affiche l'état et les mesures, sans clignoter.
- Au START, la ligne 4 rappelle « Pupitre MANUEL OFF ? » pendant 5 s.
- Les ventilateurs reçoivent une impulsion de 400 ms à fond au démarrage, puis leur consigne.

## Réglages à vérifier

Ce sont les seules constantes à changer, et seulement après une mesure.

1. **`ACTIF_BAS_V_BUT` et `ACTIF_BAS_SPARK`** (`false` par défaut).
   - Au repos, **aucune LED de relais ne doit être allumée**.
   - Si la LED de V_But ou de l'étincelle est allumée, passez la constante concernée à `true`.
2. **`NIVEAU_FLAMME_PRESENTE`** (`HIGH` par défaut).
   - Beaucoup de détecteurs IR donnent un niveau BAS devant une flamme.
   - Symptôme dans ce cas : URGENCE « flamme parasite » 5 s après la mise sous tension.
   - Remède : mettez `LOW`.
3. **Seuil du MQ-6** (`seuil_mq6`, réglable au menu ou depuis la page, enregistré en EEPROM).
   - Relevez la valeur du MQ-6 en air propre, capteur chaud (elle s'affiche sur la page).
   - Réglez le seuil nettement au-dessus de cette valeur.
4. **`PWM_MIN_VENTIL`** (60) : à confirmer avec `croquis_de_test/test_lcd_ventilateur_D4`.

**Préchauffage du MQ-6 :** un MQ-6 neuf ou froid donne une valeur haute pendant quelques minutes après la mise sous tension. Une URGENCE « fuite GPL » peut donc apparaître au démarrage. C'est normal : attendez que la valeur redescende sous le seuil, puis réarmez (S16 ou bouton de la page). Pour un capteur neuf, le fabricant conseille un rodage de 24 h sous tension.

## Procédure d'essai

### A. Essai à vide (bouteille FERMÉE, bobine 24 V de V_But débranchée)

1. Téléversez `essai_gpl_mq6.ino` : carte *Arduino Mega or Mega 2560*, port COM4.
2. Ouvrez le moniteur série. Vérifiez l'adresse de l'écran (0x21) et **2 sondes DS18B20**.
3. Vérifiez qu'**aucune LED de relais n'est allumée** au repos.
4. Vérifiez les mesures à l'écran ou sur la page : T_sec, T_cap, T_amb, H_amb, H_sec, MQ-6.
5. Fermez le moniteur série, ouvrez `interface_operateur.html` et cliquez **Connecter USB**.
6. Appuyez sur START. Pendant 120 s, la purge tourne à fond.
7. Ensuite, LED EV1–EV3 et étincelle, puis la LED V_But **1 s après**.
8. Sans gaz, il n'y a pas de flamme : 3 essais, puis ERREUR, buzzer, tout fermé (environ 6 min 15 s au total).
9. Testez l'arrêt d'urgence : URGENCE, vannes fermées, purge maintenue. Le réarmement est refusé tant que le bouton est enfoncé.
10. Testez une fuite simulée, en état ATTENTE (étincelle à l'arrêt), dans un local aéré et loin de toute flamme : approchez du MQ-6 un briquet **éteint**, gaz ouvert, pendant 2 à 3 s. Résultat attendu : URGENCE, et le bouton Réarmer reste grisé tant que la valeur dépasse le seuil.

### B. Essai avec gaz

**Seulement après validation de l'essai à vide.** Rebranchez la bobine, ouvrez la bouteille, puis START. La flamme doit être confirmée au plus tard 4 s après l'ouverture de V_But.

## Bibliothèques et téléversement

Bibliothèques à installer : OneWire, DallasTemperature, DHT sensor library, Adafruit Unified Sensor. LiquidCrystal_I2C n'est **pas** nécessaire.

```
arduino-cli lib install OneWire DallasTemperature "DHT sensor library" "Adafruit Unified Sensor"
arduino-cli compile --fqbn arduino:avr:mega essai_gpl_mq6
arduino-cli upload  -p COM4 --fqbn arduino:avr:mega essai_gpl_mq6
```

Compilation (avr-gcc 7.3) : **0 erreur**, programme 48 474 o (19,1 %), RAM 4 656 o (56,8 %). Il reste un avertissement de format, sans conséquence, sur une ligne d'écran existante.

Rappels :
- L'ouverture de la page en USB **redémarre la Mega** : connectez la page **avant** de lancer un cycle.
- Le moniteur série et la page ne peuvent pas ouvrir le port en même temps.

## Tests réalisés sur PC (tous réussis)

| Test | Résultat |
|---|---|
| Simulation de l'essai : ordre des vannes, V_H2 jamais active, STOP, échecs d'allumage, fuite MQ-6, arrêt d'urgence, flamme parasite, EEPROM | **28 / 28** |
| Écran simulé octet par octet sur le bus I2C (adresse 0x21, pas de clignotement, rappel au START) + ventilateurs (impulsion, seuil) + arrêt d'urgence pendant une impulsion | **19 / 19** |
| Scénario de 95 min comparé à la version MQ-4 validée, pas de 20 ms par pas de 20 ms | **identique** (284 616 pas) |
| Programme piloté par la page HTML dans Chromium : START, STOP, panne d'allumeur et « Réessayer », fuite MQ-6, arrêt d'urgence ; V_But toujours **1,00 s** après la rampe, V_H2 jamais activée | **24 / 24** |
