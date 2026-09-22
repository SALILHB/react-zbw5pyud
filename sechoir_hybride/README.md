# Séchoir solaire hybride — Commande de température (Arduino Mega 2560)

Programme de commande d'un séchoir solaire hybride à trois sources
(Solaire / H₂ / GPL), structuré en machine à états finis. **Version 2** :
modulation 100/67/33/0 %, configuration centralisée sauvegardée en EEPROM,
écran 20×4 I2C, séquence de basculement H2↔GPL avec gestion d'échec.

## Contenu

| Chemin | Rôle |
|---|---|
| `sechoir_hybride/sechoir_hybride.ino` | Programme complet, commenté en français |
| `docs/FSM_SECHOIR.md` | Tableau de synthèse de la FSM v2 + avis critiques sur le cahier des charges |
| `tests/` | Banc de tests natif (mocks C++ des bibliothèques Arduino, dont EEPROM) |

## Téléversement

Bibliothèques requises dans l'IDE Arduino :

- `OneWire` (Paul Stoffregen)
- `DallasTemperature` (Miles Burton)
- `DHT sensor library` (Adafruit) + `Adafruit Unified Sensor`
- `LiquidCrystal_I2C` (Frank de Brabander)
- `EEPROM` (incluse dans le cœur Arduino AVR, rien à installer)

Carte : **Arduino Mega 2560**. Le brochage est regroupé en section 2 du fichier.

### Écran

Le programme cible désormais un **LCD 20×4 I2C** (remplace le 16×2 d'origine
— décision prise avec l'opérateur pour pouvoir afficher les ~15 paramètres du
menu et les pages de télémesure du §16 du cahier des charges). Même bus I2C
(SDA/SCL/GND/VCC inchangés), même bibliothèque : seul le module physique
change. **À vérifier au premier branchement** : l'adresse I2C du nouveau
module peut différer du 16×2 (souvent `0x27` ou `0x3F` selon le fournisseur)
— `LCD_ADRESSE` en section 3 du fichier.

## Banc de tests

```bash
make -C tests run
```

139 vérifications sur 16 cas (voir `docs/FSM_SECHOIR.md` §6), y compris les
nouveautés v2 : modulation à 4 paliers avec coupure/réveil sans allumage
inutile, échec de basculement (escalade immédiate), échecs d'allumage
répétés, persistance EEPROM, plafond de prolongation.

## Réglages à effectuer sur le prototype

Tous les paramètres opérateur sont dans la structure `Config` (section 6bis
du fichier), modifiables au menu LCD et sauvegardés en EEPROM. Trois d'entre
eux doivent impérativement être relevés sur le matériel réel avant toute
mise en gaz — voir `docs/FSM_SECHOIR.md` §7 :

- `Seuil_MQ8`, `Seuil_MQ6` — seuils de détection de fuite (procédure en commentaire) ;
- `Press_H2_Min` — pression H₂ minimale d'alimentation du brûleur ;
- `SEUIL_T_CAP_SOLAIRE_ON` / `_OFF` (constantes techniques, non exposées au menu) — disponibilité de la source solaire.

Voir `docs/FSM_SECHOIR.md` pour la synthèse complète et les avis critiques sur
le cahier des charges (§1).
