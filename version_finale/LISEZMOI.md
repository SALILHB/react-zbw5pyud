# Séchoir solaire hybride (solaire / hydrogène / GPL) : code de commande final

Programme complet pour Arduino Mega 2560, à jour du matériel validé sur banc le 9 novembre 2026.

## Contenu

| Élément | Rôle |
|---|---|
| `sechoir_hybride_clavier4x4/sechoir_hybride_clavier4x4.ino` | Programme principal : machine à états (attente, menu, solaire, H2, GPL, prolongation, fin, erreur de combustion, urgence ATEX), régulation à 4 paliers 100 / 67 / 33 / 0 %, EEPROM, écran, clavier |
| `sechoir_hybride_clavier4x4/lcd_pcf8574.h` | Pilote intégré de l'écran 20x4 I2C (aucune bibliothèque LCD à installer) |
| `sechoir_hybride_clavier4x4/liaison_interface.h` et `liaison_interface_impl.h` | Liaison avec l'interface opérateur (USB et ESP32 sur Serial1) |
| `sechoir_hybride_clavier4x4/interface_operateur.html` | Page de commande (Chrome ou Edge, bouton « Connecter USB ») |
| `croquis_de_test/` | Tests déjà validés sur la carte : écran seul, clavier + écran, écran + ventilateur D4 |

**Gardez les 5 fichiers dans le même dossier `sechoir_hybride_clavier4x4/`** : l'IDE Arduino ouvre le `.ino` et affiche les en-têtes en onglets.

## Brochage (Arduino Mega 2560)

| Fonction | Broche |
|---|---|
| DS18B20 (T_sec index 0, T_cap index 1) | D22 |
| DHT22 air extérieur / DHT22 extraction | D24 / D26 |
| Détecteur de flamme (TOR) | D28 |
| Clavier 4x4, fils 1 à 8 | D30 D32 D34 D36 D38 D40 D44 D46 |
| Arrêt d'urgence (contact NC vers GND) | D42 |
| V_H2 / V_But | D23 / D25 |
| EV1 / EV2 / EV3 (rampe) | D27 / D29 / D31 |
| Allumeur / buzzer | D33 / D35 |
| Ventilateurs purge / distribution / extraction (PWM) | D2 / D3 / D4 |
| MQ-8 (H2) / MQ-6 (GPL) / pression H2 4-20 mA | A0 / A1 / A2 |
| Écran LCD I2C, adresse 0x21 (cherchée au démarrage) | SDA D20, SCL D21 |
| ESP32 (étape B, facultatif) | Serial1 : TX1 D18, RX1 D19 |

## Clavier

| Touche | Fonction |
|---|---|
| S1 | MENU |
| S2 | + |
| S10 | − |
| S6 | OK |
| S4 | START |
| S8 | STOP |
| S16 | RÉARMEMENT |

## Bibliothèques

Installez avec le gestionnaire de bibliothèques de l'IDE :
- OneWire ;
- DallasTemperature ;
- DHT sensor library (Adafruit) ;
- Adafruit Unified Sensor.

Wire et EEPROM sont fournies avec l'IDE. **LiquidCrystal_I2C n'est plus nécessaire.**

Avec arduino-cli :

```
arduino-cli lib install OneWire DallasTemperature "DHT sensor library" "Adafruit Unified Sensor"
arduino-cli compile --fqbn arduino:avr:mega sechoir_hybride_clavier4x4
arduino-cli upload  -p COM4 --fqbn arduino:avr:mega sechoir_hybride_clavier4x4
```

Avec avr-gcc 7.3 : **0 erreur**, programme 48 082 o (18,9 %), RAM 4 543 o (55,5 %).

## Avant la mise en service

1. **Polarité des relais**, à vérifier :
   - `ACTIF_BAS_V_BUT` et `ACTIF_BAS_SPARK` sont « À VÉRIFIER ».
   - Bouteilles fermées et bobines 24 V débranchées, Mega sous tension : **aucune LED de relais ne doit être allumée au repos**.
   - Sinon, passez la constante concernée à `true`.
2. **Détecteur de flamme** : s'il donne un niveau BAS quand il voit une flamme, mettez `NIVEAU_FLAMME_PRESENTE = LOW`.
   - Le symptôme sinon : URGENCE « flamme parasite » 5 s après la mise sous tension.
3. **Seuil minimal des ventilateurs** : `PWM_MIN_VENTIL = 60` est à confirmer pour chaque groupe avec `croquis_de_test/test_lcd_ventilateur_D4`.
4. **Pupitre** : les interrupteurs MANUEL (NE555) doivent être sur OFF en automatique. L'écran le rappelle 5 s au START.
5. **EEPROM** : les réglages enregistrés auparavant sont rechargés. Vérifiez le mode (AUTO / MANUEL) et la source au menu.
6. **Premier essai au gaz** : faites-le avec le programme `essai_gpl_mq4` (GPL seul, livré dans `sechoir_materiel_valide.zip`), jamais directement avec l'hydrogène.

## Sécurités du programme (non modifiables sans revue)

**Urgence ATEX, prioritaire.** Elle se déclenche sur :
- fuite H2 (MQ-8) ou GPL (MQ-6) au-dessus du seuil ;
- arrêt d'urgence D42 (contact NC : un fil coupé déclenche aussi l'alarme) ;
- perte de flamme après une relance ;
- flamme vue alors que le gaz est fermé (5 s).

Effet : toutes les vannes fermées dans le même tour de boucle, buzzer, purge à 255. Le réarmement n'est accepté que lorsque la cause a disparu.

**Combustion**
- Purge de 120 s avant chaque allumage.
- Allumage en 4 s maximum, 3 essais.
- Ensuite ERREUR_COMBUSTION : l'opérateur choisit RÉESSAYER, MANUEL ou AUTO.

**Verrous**
- V_H2 et V_But ne sont jamais ouvertes ensemble : contrôle dans `ecrireSorties()`.
- Délai d'attente du bus I2C : un écran en défaut ne bloque pas la boucle.

## Interface opérateur

1. Fermez le moniteur série.
2. Ouvrez `interface_operateur.html` dans Chrome ou Edge.
3. Cliquez **Connecter USB** et choisissez la Mega.

La page affiche l'état, les mesures, les relais et les courbes, et permet START / STOP / réglages / réarmement. L'arrêt d'urgence reste matériel.

L'ouverture du port USB redémarre la Mega : connectez la page **avant** de lancer un cycle.
