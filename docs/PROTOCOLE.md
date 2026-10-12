# Protocole de la liaison interface ↔ Mega

Liaison série **115 200 bauds**, texte, une ligne terminée par `\n` (un `\r` final est ignoré).
Le même protocole passe par :
- `Serial` : le câble USB, avec la page `interface/index.html` ouverte dans Chrome ou Edge ;
- `Serial1` : l'ESP32 (étape B), qui relaie chaque ligne par WebSocket.

**Le Mega reste le seul automate.** L'interface envoie des demandes et le Mega décide, avec les règles du pupitre :
- **START, STOP et OK** sont des appuis de boutons virtuels, consommés par `frontPris()` exactement comme les vrais boutons ;
- **le réarmement** passe par le drapeau existant `demande_rearm_ihm` ;
- **l'accusé de réception** est calculé *après* `pasFSM()` : si le bouton virtuel n'a pas été consommé, la commande n'a eu aucun effet et la réponse est `"ok":false`.

Code : `sechoir_hybride/liaison_interface.h` (déclarations) et `liaison_interface_impl.h` (mise en œuvre). Les modifications de `sechoir_hybride.ino` sont marquées `// [INTERFACE]`.

## Interrupteur de compilation

| `AVEC_PUPITRE_LCD` | Effet |
|---|---|
| `0` (défaut) | Ni écran ni boutons. Pas d'initialisation I²C ; boutons D30–D40 et D44 ignorés. **L'arrêt d'urgence D42 est lu exactement comme avant.** Conduite par l'interface. |
| `1` | Programme d'origine (écran 20×4 et boutons), plus l'interface. Sans commande distante, l'évolution de la machine à états est identique au programme d'origine (test `make comparaison`). |

Sans pupitre, le bouton de réarmement D44 est ignoré : on sort d'une urgence par `REARM` depuis l'interface, toujours refusé tant qu'une cause est présente. On peut aussi couper l'alimentation du Mega : il redémarre en `ATTENTE`.

## Lignes émises par le Mega

Les lignes qui commencent par `{` sont du JSON ; toutes les autres sont les messages de débogage du programme (`FSM -> H2`, `Echec d'allumage`…), affichés tels quels dans le journal.

Toutes les lignes partent sur l'USB **et** sur `Serial1`. Les messages du programme passent par un petit journal (200 octets). La liaison les réémet ligne par ligne entre deux trames : une ligne de débogage ne coupe jamais une trame JSON.

L'émission ne bloque jamais `loop()`. Chaque trame est écrite par morceaux, selon la place libre des tampons d'émission (`availableForWrite()`). Une ligne de journal perdue faute de place est comptée dans le champ `jp` de la télémesure.

### `{"t":"tel",...}` : télémesure, toutes les secondes

| Clé | Contenu |
|---|---|
| `n`, `ms` | Numéro de trame, `millis()` du Mega |
| `lcd` | Valeur de `AVEC_PUPITRE_LCD` |
| `etat`, `e` | État de la machine : nom (`nomEtat()`) et numéro, `0` = ATTENTE … `9` = URGENCE (ordre de `enum EtatFSM`) |
| `src` | Source active : `SOLAIRE`, `H2` ou `GPL` |
| `ph` | Phase de combustion (`nomPalierOuPhase()`) : `PURGE`, `ALLUMAGE`, `100%`, `67%`, `33%`, `0% (veille)` |
| `pal` | Palier : 0 = 100 %, 1 = 67 %, 2 = 33 %, 3 = 0 % |
| `auto`, `reg`, `pimp` | `Mode_Auto`, `Regul_Auto`, `Palier_Impose` |
| `chaud` | Repère thermique : 0 = FROID, 1 = CHAUD |
| `tsec`, `tc`, `t1`, `t2`, `t3`, `hh` | T_sec, T_cible, seuils de palier, hystérésis (°C) |
| `hsec`, `hfin`, `hamb`, `tamb` | Humidités (%) et température ambiante (°C) |
| `tcap`, `tcapm` | T_cap estimée (seule utilisée par la logique) et sonde de plaque (affichage) |
| `son`, `soff` | Seuils solaire ON et OFF |
| `fl`, `mq8`, `s8`, `mq6`, `s6` | Flamme, détecteurs MQ-8 et MQ-6 face à leurs seuils (sur 1023) |
| `ph2`, `ph2min` | Pression H2 et pression minimale (bar) |
| `au` | Bouton d'arrêt d'urgence enfoncé, ou fil coupé |
| `urg` | Cause d'urgence (`enum CauseUrgence`) : 1 fuite H2, 2 fuite GPL, 3 arrêt d'urgence, 4 perte de flamme, 5 flamme parasite |
| `urgp` | Cause encore présente (`causeUrgencePresente()`) : le réarmement sera refusé |
| `vh2`, `vbut`, `ev1`, `ev2`, `ev3`, `spk`, `buz` | Sorties TOR (lecture seule) |
| `pwmp`, `pwmd`, `pwme` | Ventilateurs de purge, de distribution, d'extraction (0–255) |
| `postp` | Post-purge en cours (retour au solaire) |
| `tcy`, `tmax`, `tmf` | Temps de cycle écoulé (s, −1 hors cycle), durée maximale et durée minimale avant fin (s) |
| `trep`, `trept` | En DEMANDE : secondes avant la fin automatique, et `Temps_Reponse` |
| `nprol`, `tprol` | Durée de prolongation proposée ou choisie (min), temps écoulé en PROLONGATION (s) |
| `motif` | Motif de la demande : 0 humidité, 1 durée maximale, 2 fin de prolongation |
| `choix`, `ech`, `pf` | Choix d'erreur courant, échecs d'allumage, pertes de flamme du cycle |
| `tpur`, `tref` | Purge ou post-purge restante, refroidissement restant (s, −1 sinon) |
| `jp` | Lignes de journal perdues |
| `fta`, `fha`, `fhs`, `fp` | Grandeurs en mode FIXE : T_amb, H_amb, H_sec, pression |

### `{"t":"cfg",...}` : réponse à `GET CFG`

Les 31 paramètres, avec les clés du tableau ci-dessous. Les décimaux sont au dixième, les autres en entier : `{"t":"cfg","mode_auto":1,"choix_mode":2,"t_cible":55.0,...,"press_val":5.0}`.

### `{"t":"meta",...}` : réponse à `GET META`

`{"t":"meta","version":"sechoir-if 1","champs":[{"k":"t_cible","l":"Température cible","u":"°C","ty":"n","g":"Cycle","s":0,"min":30.0,"max":90.0,"pas":1.0,"o":""},...]}`

| Clé | Contenu |
|---|---|
| `k`, `l`, `u` | Clé, libellé, unité |
| `ty` | `n` décimal, `e` entier, `b` booléen, `l` liste |
| `g` | Groupe (comme le menu LCD) |
| `s` | 1 = paramètre de sécurité combustion : la page demande une confirmation |
| `min`, `max`, `pas` | Bornes et pas : **les constantes du programme** (`T_CIBLE_MIN`, `PAS_TEMPERATURE`…), table en mémoire flash |
| `o` | Noms des valeurs d'un booléen ou d'une liste, séparés par `\|` |

La page construit son formulaire uniquement à partir de `meta` : les bornes n'existent qu'à un seul endroit, le programme.

### `{"t":"ack",...}` : accusé de réception

`{"t":"ack","cmd":"SET","ok":true,"val":"90.0","raison":"t_cible"}`

| Clé | Contenu |
|---|---|
| `cmd` | Commande (9 caractères au plus ; `?` pour une ligne trop longue) |
| `ok` | `true` si la commande a eu son effet |
| `val` | `SET` : **valeur réellement retenue après bornage** ; START, STOP… : nouvel état ; `PROLONG <min>` : N retenu |
| `raison` | Raison du refus (« sans effet en H2 », « fuite H2 : MQ-8 640 > 350 »…) ; pour un `SET` accepté : la clé |

## Commandes interface → Mega

| Commande | Effet (identique au pupitre) | Acceptée en |
|---|---|---|
| `PING` | Réponse `val` = version | toujours |
| `GET CFG`, `GET META` | Trame `cfg` ou `meta` | toujours |
| `SET <clé> <valeur>` | Valeur, puis `bornerConfig()` et `calculerSeuils()`. Pas d'écriture EEPROM | ATTENTE, TERMINE |
| `SET t_cible <valeur>` en cycle | Mêmes instructions que `reglerTcibleEnCycle()` (UP/DOWN) : valeur, bornage, seuils, **EEPROM** | SOLAIRE, H2, GPL, PROLONGATION |
| `SAVE` | `sauverConfig()` (seulement sur demande, pour ménager l'EEPROM) | ATTENTE, TERMINE |
| `START` | Bouton START | ATTENTE, MENU (pupitre), TERMINE |
| `STOP` | Bouton STOP | SOLAIRE, H2, GPL, DEMANDE, PROLONGATION, ERREUR |
| `PROLONG <min>` | Règle N comme UP/DOWN : bornes `PROLONG_MIN`–`PROLONG_MAX`, délai de réponse relancé | DEMANDE |
| `PROLONG OK` | Bouton OK : prolongation de N min | DEMANDE |
| `TERMINER` | Bouton STOP : séchage terminé | DEMANDE |
| `ERREUR <REESSAYER\|MANUEL\|AUTO>` | Règle `Choix_Erreur` puis bouton OK (= MENU puis OK au pupitre) | ERREUR |
| `REARM` | `demande_rearm_ihm` : `pasFSM()` refuse si `causeUrgencePresente()` | URGENCE |
| `PILOTE` | Traité par l'ESP32 (prise de main) ; le Mega répond seulement | — |

Avec `AVEC_PUPITRE_LCD 1`, `SET` et `SAVE` sont refusés tant que le menu local du pupitre est ouvert.

Règles de sécurité :
- **Aucune commande ne force une sortie** (électrovanne, relais, étincelle, ventilateur). Aucune ne peut écrire T_sec, MQ-8, MQ-6, la flamme ou l'arrêt d'urgence.
- **L'arrêt d'urgence câblé D42 reste prioritaire.** Un STOP arrivé au même instant ne l'empêche pas.
- **Seule `REARM` validée sort de l'URGENCE.** Elle reste refusée tant qu'une cause est présente : fuite, arrêt d'urgence enfoncé ou flamme.
- **Une ligne de plus de 63 caractères est jetée** (réponse `cmd:"?"`). Une commande inconnue est refusée.
- **Une seule ligne est traitée par tour de boucle.** Si les deux en ont une, l'USB passe d'abord, puis l'ESP32.
- **Une perte de liaison n'a aucun effet.** Le Mega continue seul avec toutes ses sécurités ; le buzzer D35 signale toujours l'urgence et l'erreur.

## Pilote (ESP32, étape B)

L'ESP32 relaie les lignes et **ne génère jamais de commande vers le Mega**.

- **Un seul pilote parmi les téléphones.** Il ne transmet au Mega que les lignes du client pilote, plus les lignes `STOP` de tous les clients.
- **Le câble USB est toujours pilote.** Il sert aux essais et au dépannage sur place.
- **Message propre à la passerelle**, envoyé à chaque client : `{"t":"pilote","vous":true|false,"n":<nombre de clients>}`. La commande `PILOTE` fait prendre la main au client qui l'envoie.
- **Connexions.** La page est servie sur `http://192.168.4.1` ; le WebSocket est sur le port 81 (`ws://192.168.4.1:81/`), une ligne par message.
