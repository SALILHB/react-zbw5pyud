# Séchoir solaire hybride — Commande de température (Arduino Mega 2560)

Programme de commande d'un séchoir solaire hybride à trois sources
(Solaire / H₂ / GPL), structuré en machine à états finis.

## Contenu

| Chemin | Rôle |
|---|---|
| `sechoir_hybride/sechoir_hybride.ino` | Programme complet, commenté en français |
| `docs/FSM_SECHOIR.md` | Tableau de synthèse de la FSM + incohérences signalées |
| `tests/` | Banc de tests natif (mocks C++ des bibliothèques Arduino) |

## Téléversement

Bibliothèques requises dans l'IDE Arduino :

- `OneWire` (Paul Stoffregen)
- `DallasTemperature` (Miles Burton)
- `DHT sensor library` (Adafruit) + `Adafruit Unified Sensor`
- `LiquidCrystal_I2C` (Frank de Brabander)

Carte : **Arduino Mega 2560**. Le brochage est regroupé en section 2 du fichier.

## Banc de tests

Le programme est compilé **tel quel** sur PC, les bibliothèques matérielles
étant remplacées par des mocks :

```bash
make -C tests run
```

167 vérifications couvrant les 12 cas exigés au §11.4 du cahier des charges.

## Réglages à effectuer sur le prototype

Toutes les constantes ajustables sont regroupées en **section 3** du fichier.
Trois d'entre elles doivent impérativement être relevées sur le matériel réel
avant toute mise en gaz :

- `SEUIL_MQ8`, `SEUIL_MQ6` — seuils de détection de fuite (procédure en commentaire) ;
- `SEUIL_PRESS_H2_MIN` — pression H₂ minimale d'alimentation du brûleur ;
- `SEUIL_T_CAP_SOLAIRE_ON` / `_OFF` — disponibilité de la source solaire.

Voir `docs/FSM_SECHOIR.md` §3 pour les points signalés.
