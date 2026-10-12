# Interface opérateur du séchoir : guide d'utilisation (liaison USB)

L'interface remplace l'écran LCD et les boutons du pupitre. C'est une page web
(`index.html`) qui dialogue avec l'Arduino Mega par le câble USB.

- **Le Mega reste le seul automate** : régulation, sécurités et arrêt d'urgence sont dans le programme.
- **La page envoie des demandes** (START, STOP, réglages…) et affiche tout ce que fait le Mega.
- **Sans la page** (câble débranché), le Mega continue seul avec toutes ses sécurités.

L'accès par téléphone en Wi-Fi (passerelle ESP32) viendra à l'étape B.

---

## 1. Ce qu'il faut

| Élément | Remarque |
|---|---|
| Ordinateur Windows (ou Linux / macOS) | — |
| **Google Chrome** ou **Microsoft Edge** | La liaison USB (Web Serial) ne marche pas dans Firefox ni sur téléphone |
| Arduino IDE 2.x | Pour téléverser le programme |
| Arduino Mega 2560 + câble USB | — |
| Bibliothèques (Outils → Gérer les bibliothèques) | OneWire, DallasTemperature, DHT sensor library (Adafruit) + Adafruit Unified Sensor, LiquidCrystal I2C (Frank de Brabander) |

## 2. Copier les fichiers

Le dossier du programme doit porter **le même nom que le `.ino`** et contenir **les trois fichiers** :

```
sechoir_hybride\
   sechoir_hybride.ino          programme du séchoir (32 lignes ajoutées, marquées [INTERFACE])
   liaison_interface.h          liaison avec l'interface (déclarations)
   liaison_interface_impl.h     liaison avec l'interface (mise en œuvre)
interface\
   index.html                   l'interface (un seul fichier)
```

Par exemple dans `D:\memoire master claud\projet_claude\` : remplacez l'ancien `sechoir_hybride.ino` et ajoutez les deux fichiers `.h` à côté.

## 3. Téléverser le programme dans le Mega

1. Ouvrir `sechoir_hybride.ino` dans l'IDE Arduino. **Trois onglets** doivent apparaître : `sechoir_hybride.ino`, `liaison_interface.h`, `liaison_interface_impl.h`.
2. Choisir le mode, vers la ligne 75 :
   ```cpp
   #define AVEC_PUPITRE_LCD 0
   ```
   - `0` (par défaut) : **sans écran ni boutons**, conduite par l'interface seule.
   - `1` : l'écran 20×4 et les boutons marchent comme avant, **et** l'interface aussi.
3. *Outils → Type de carte → Arduino Mega or Mega 2560*, puis *Outils → Port → COMx*.
4. *Croquis → Téléverser*. L'IDE affiche environ 15 % de mémoire programme et 37 % de mémoire dynamique (53 % avec `1`).
5. **Fermer le Moniteur série** de l'IDE : un seul programme à la fois peut utiliser le port USB.

> ⚠ **Arrêt d'urgence D42 (contact NC).** Le programme lit une urgence quand D42 est en l'air. Sur le prototype, le bouton d'arrêt d'urgence est câblé entre D42 et GND. Pour un essai **sur table sans le bouton**, relier D42 à GND par un fil ; sinon l'interface affiche aussitôt « URGENCE — Arrêt d'urgence enfoncé ».

## 4. Ouvrir l'interface et se connecter

1. Double-cliquer sur `interface\index.html`. Si la page s'ouvre dans Firefox : clic droit → *Ouvrir avec* → Chrome ou Edge.
2. Cliquer **Connecter USB** (en haut), choisir *Arduino Mega 2560 (COMx)*, puis *Connexion*.
3. **Le Mega redémarre à l'ouverture du port** (comportement normal d'un Arduino). Au bout de 2 à 3 s :
   - le bandeau affiche **ATTENTE** ;
   - les mesures s'affichent ;
   - la pastille indique « USB · liaison OK ».

> Ne connectez pas la page pendant un vrai séchage : le redémarrage du Mega interromprait le cycle (gaz fermé, retour en attente).

**Sans matériel** : cliquer **Mode démo**. Un séchoir simulé (temps ×10) permet d'essayer toute la page. Les boutons jaunes simulent les pannes : soleil, réservoir vide, panne d'allumeur, fuite, arrêt d'urgence, durée max.

## 5. Utiliser l'interface

| Où | Quoi |
|---|---|
| **Bandeau coloré** | État de la machine (couleur par état), source, palier, chronomètre |
| **Tableau de bord** | T_sec en grand avec la consigne et les seuils T1, T2, T3 ; humidité face à H_fin ; flamme, pression H2, détecteurs MQ-8 et MQ-6 face à leurs seuils ; état de l'arrêt d'urgence ; sorties (électrovannes, étincelle, buzzer, ventilateurs) en lecture seule |
| **Commande** | **START** (avec confirmation), consigne **−1 / +1 °C** pendant le cycle (comme UP / DOWN) |
| **STOP** (bas de l'écran, toujours visible) | Arrêt propre du cycle : gaz fermé, refroidissement |
| **Paramètres** | Les 31 réglages du menu LCD, groupés comme au pupitre |
| **Courbes** | T_sec et seuils, humidité, palier sur 15 min, 2 h ou tout ; **Exporter CSV** (toutes les mesures reçues, pour exploiter les essais dans Excel ou MATLAB) |
| **Journal** | Changements d'état, commandes, réponses du Mega, messages du programme ; **Exporter** |

Dans l'onglet **Paramètres** :
- Modifier une valeur, puis **Envoyer les modifications**. Le Mega renvoie la valeur **réellement retenue** : s'il l'a bornée, la page l'affiche en orange.
- Les réglages de sécurité (purge, allumage, pression mini, seuils de fuite) demandent une confirmation.
- **Enregistrer en EEPROM** garde les réglages après une coupure de courant. Sans cela, ils ne valent que jusqu'au prochain redémarrage.
- Réglages possibles en ATTENTE et en SÉCHAGE TERMINÉ. Pendant un cycle, seule la consigne est modifiable (onglet Commande).

### Écrans d'alarme (ils recouvrent la page)

- **Rouge — URGENCE ATEX** (fuite, arrêt d'urgence, flamme anormale) :
  - affiche la cause et l'état de chaque cause (présente ou disparue) ;
  - le bouton **Réarmer** reste grisé tant qu'une cause est présente ;
  - une fois la cause disparue, cocher « J'ai vérifié sur place », puis Réarmer, puis Confirmer ;
  - le séchoir revient en ATTENTE, et le cycle n'est pas repris.
- **Orange — ÉCHEC D'ALLUMAGE** (3 échecs) :
  - aller vérifier sur place l'allumeur, l'arrivée de gaz et la flamme ;
  - choisir Réessayer, Passer en manuel ou Automatique ;
  - cocher la case, valider, puis confirmer ;
  - c'est le même effet que MENU puis OK au pupitre.
- **Vert — Fin de séchage atteinte** (humidité ou durée max) :
  - régler la durée N avec − / +, puis **Prolonger** ou **Terminer le séchage** ;
  - sans réponse, le séchage se termine tout seul à la fin du compte à rebours.

La page sonne et fait vibrer (si le navigateur le permet) en cas d'urgence, d'erreur et de demande. **Le buzzer du Mega reste l'alarme sur place.**

Si plus aucune trame n'arrive pendant 3 s, un bandeau rouge « Liaison perdue » s'affiche. Le Mega, lui, continue seul.

## 6. Problèmes fréquents

| Symptôme | Cause et solution |
|---|---|
| « Liaison USB indisponible » | Page ouverte dans Firefox ou sur téléphone : utiliser Chrome ou Edge sur ordinateur |
| Le port n'apparaît pas | Câble USB de charge seule (sans données), ou pilote CH340 manquant pour un clone de Mega |
| « Ouverture du port impossible » | Le Moniteur série de l'IDE (ou un autre logiciel) utilise le port : le fermer |
| URGENCE « Arrêt d'urgence enfoncé » dès le départ | D42 non relié à GND (bouton absent ou fil coupé) : voir l'avertissement du §3 |
| Valeurs figées, « Liaison perdue » | Câble débranché, ou Mega réinitialisé : cliquer Déconnecter puis Connecter USB |
| Paramètres grisés | Cycle en cours (seule la consigne se règle), urgence ou erreur en cours |
| Écran LCD noir, boutons sans effet | Normal avec `AVEC_PUPITRE_LCD 0` ; mettre `1` pour garder le pupitre |

## 7. Sécurité

- **Premiers essais : gaz fermé** (robinets fermés) et électrovannes débranchées. Plan d'essai complet : `docs/TESTS.md`.
- L'interface **ne remplace pas** l'arrêt d'urgence câblé : il doit rester en place et accessible.
- Aucune commande de l'interface ne peut ouvrir directement une électrovanne ni désactiver une sécurité. Le Mega applique les mêmes règles qu'au pupitre et refuse ce qui n'est pas permis dans l'état courant.

Protocole détaillé : `docs/PROTOCOLE.md`.
