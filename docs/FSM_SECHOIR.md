# Séchoir solaire hybride — Synthèse de la machine à états finis

Document d'accompagnement du programme `sechoir_hybride/sechoir_hybride.ino`.
Les références `§x` renvoient au cahier des charges.

---

## 1. Tableau de synthèse — États principaux (§7.1)

| État | Rôle | Actions (sorties) | Transitions sortantes |
|---|---|---|---|
| `ATTENTE_DEMARRAGE` | État initial et état de repli après réarmement | `V_H2`=`V_But`=`EV1`=`EV2`=`EV3`=`Spark`=0 ; `PWM_Purge`/`Distrib`/`Extract` = 0/0/0 ; `Buzzer`=0 | `Btn_Menu` → `CONFIG_MENU`<br>`Btn_Start` → `demarrerCycle()` → `MODE_SOLAIRE`/`MODE_H2`/`MODE_GPL`<br>cause ATEX → `URGENCE_ATEX` |
| `CONFIG_MENU` | Saisie des consignes sur LCD 16×2 + 4 boutons | Toutes sorties gaz fermées ; PWM 0/0/0 | `Btn_OK` (hors champ Réarmement) → `ATTENTE_DEMARRAGE`<br>`Btn_Start` → démarrage de cycle<br>cause ATEX → `URGENCE_ATEX` |
| `MODE_SOLAIRE` | Séchage **purement passif** : aucune électrovanne (§2, §12) | Gaz fermé ; `PWM_Purge`=0, `Distrib`=220 (élevé), `Extract`=150 (moyen) ; purge ré-armée en permanence | auto & `T_cap < 45 °C` → combustion, **démarrage à froid palier 100 %** + purge 120 s<br>`H_extr ≤ H_fin` → `SECHAGE_TERMINE`<br>`chrono_cycle > Duree_Max_Cycle` → `PROLONGATION`<br>`Btn_Stop` → `SECHAGE_TERMINE`<br>cause ATEX → `URGENCE_ATEX` |
| `MODE_H2` | Combustion hydrogène (priorité 2) — sous-phases §7.2 | `gererCombustion(true)` : `V_H2`=1 hors purge, `V_But`=0 ; `EV1/2/3` selon `palier` ; PWM selon phase | auto & `T_cap ≥ 55 °C` → `MODE_SOLAIRE`<br>auto & `Press_H2 < 2 bar` → `MODE_GPL` (**palier conservé**, purge 120 s)<br>`H_extr ≤ H_fin` → `SECHAGE_TERMINE`<br>durée max → `PROLONGATION`<br>`Btn_Stop` → `SECHAGE_TERMINE`<br>cause ATEX → `URGENCE_ATEX` |
| `MODE_GPL` | Combustion GPL de secours (priorité 3) — **même code** que H₂ | `gererCombustion(false)` : `V_But`=1 hors purge, `V_H2`=0 ; reste identique à `MODE_H2` | auto & `T_cap ≥ 55 °C` → `MODE_SOLAIRE`<br>idem `MODE_H2` pour la fin de cycle, la prolongation, l'arrêt et l'urgence |
| `PROLONGATION` | Durée max dépassée sans atteindre `H_fin` (§6) | Régulation poursuivie **sans rupture** sur `Source_Active` : ni nouvelle purge, ni retour à 100 % | `H_extr ≤ H_fin` → `SECHAGE_TERMINE` (**aucune limite de temps**)<br>`Btn_Stop` → `SECHAGE_TERMINE`<br>cause ATEX → `URGENCE_ATEX` |
| `SECHAGE_TERMINE` | `H_extr ≤ H_fin` atteint, arrêt propre, ou surchauffe | Gaz fermé ; `PWM_Purge`=0, `Distrib`=0, `Extract`=90 (refroidissement) | `Btn_Start` → nouveau cycle<br>`Btn_Menu` → `CONFIG_MENU`<br>après 5 min → `ATTENTE_DEMARRAGE`<br>cause ATEX → `URGENCE_ATEX` |
| `URGENCE_ATEX` | État **parallèle et prioritaire**, atteignable depuis tout état (§7.4) | Fermeture immédiate de `V_H2`, `V_But`, `EV1/2/3` ; `Spark`=0 ; `PWM_Purge`=255, `Distrib`=0, `Extract`=255 ; `Buzzer`=1 ; message LCD | `Btn_Rearm` **ou** menu « Réarmement » + `Btn_OK`, **uniquement si la cause a disparu** → `ATTENTE_DEMARRAGE` |

### Transitions prioritaires (évaluées hors du `switch`, après lui — §7.5)

| Rang | Condition | Effet |
|---|---|---|
| 1 | `MQ8_H2 > SEUIL_MQ8` **ou** `MQ6_But > SEUIL_MQ6` **ou** `AU_Urgence` | → `URGENCE_ATEX`, toutes sorties écrasées en position de repli, aucune autre transition évaluée |
| 2 | `Btn_Stop` en cycle | → `SECHAGE_TERMINE`, gaz fermé |
| 3 | `T_sec ≥ 90 °C` en cycle | → `SECHAGE_TERMINE` (sécurité surchauffe ajoutée, cf. §3 incohérence n°3) |
| 4 | `H_extr ≤ H_fin` en cycle | → `SECHAGE_TERMINE` |
| 5 | `t − chrono_cycle ≥ Duree_Max_Cycle` et état ≠ `PROLONGATION` | → `PROLONGATION` |

---

## 2. Tableau de synthèse — Sous-phases de combustion (§7.2)

Fonction unique `gererCombustion(bool utiliseH2)`, partagée par `MODE_H2`, `MODE_GPL`
et `PROLONGATION`. Seule la vanne source diffère (`V_H2` ↔ `V_But`).

| Phase | Rôle | Actions | Transitions |
|---|---|---|---|
| `PURGE` | Balayage obligatoire de 120 s avant toute mise en gaz (§7.3 règle 1) | Tout le gaz fermé, `Spark`=0, `PWM_Purge`=255, `Distrib`=60, `Extract`=200 | `t − chrono_purge ≥ 120 s` → `ALLUMAGE` |
| `ALLUMAGE` | Mise en gaz **au palier courant** + étincelle, 4 s maximum | Vanne source ouverte, `EV` selon `palier`, `Spark`=1 | `Flame=1` → `PALIER_100` / `PALIER_67` / `PALIER_33` selon `palier`<br>`t − chrono_allumage ≥ 4 s` → fermeture totale → `PURGE` |
| `PALIER_100` | Pleine puissance | `EV1+EV2+EV3`, `Spark`=0, `Distrib`=255, `Extract`=200 | `T_sec ≥ T1+2,5` → `PALIER_67`<br>`Flame=0` → fermeture immédiate → `PURGE` |
| `PALIER_67` | ≈ 67 % de puissance | `EV2+EV3`, `Distrib`=210, `Extract`=165 | `T_sec ≥ T2+2,5` → `PALIER_33`<br>`T_sec < T1−2,5` → `PALIER_100`<br>`Flame=0` → `PURGE` |
| `PALIER_33` | **Plancher** ≈ 33 % — il n'existe pas de palier 0 % (§4.1) | `EV3` seule, `Distrib`=165, `Extract`=130 | `T_sec < T2−2,5` → `PALIER_67`<br>`Flame=0` → `PURGE`<br>*aucune transition vers un arrêt* |

### Hystérésis de 5 °C (§4.2), exemple de référence `T_init`=25 °C, `T_cible`=55 °C

`T1` = 35 °C, `T2` = 45 °C → seuils effectifs **32,5 / 37,5 / 42,5 / 47,5 °C**

| Transition | Condition | Vérifiée par le test |
|---|---|---|
| 100 % → 67 % (fermeture `EV1`) | `T_sec ≥ 37,5` | cas 2 |
| 67 % → 100 % (réouverture `EV1`) | `T_sec < 32,5` | cas 2 |
| 67 % → 33 % (fermeture `EV2`) | `T_sec ≥ 47,5` | cas 2 |
| 33 % → 67 % (réouverture `EV2`) | `T_sec < 42,5` | cas 2 |
| Non-basculement dans la bande | 37,0 / 47,0 / 43,0 / 33,0 °C | cas 2 |

L'hystérésis est **mémorisée par le palier courant** : `majPalier()` est un
`switch` sur `palier` qui n'autorise qu'un seul franchissement de seuil par
itération. Trois comparaisons indépendantes recalculées à chaque pas
détruiraient l'hystérésis (§4.2).

---

## 3. Incohérences et points signalés (§11.5)

Ces points ont été **signalés et non contournés silencieusement**, conformément
à la consigne §1.

### Incohérence n°1 — Redondance entre `palier` et `phase_combustion`

Le §9.5 demande à la fois `palier` (0/1/2) et `phase_combustion`
(`PALIER_100`/`PALIER_67`/`PALIER_33`). Ces deux variables portent **la même
information** dans les états de régulation ; les laisser évoluer indépendamment
est la garantie d'une désynchronisation tôt ou tard.

**Traitement retenu :** `palier` est l'unique source de vérité. `phase_combustion`
en est **dérivée** à chaque itération par `phaseDuPalier(palier)`. Les deux
variables du dictionnaire existent bien, mais une seule est écrite par la
régulation.

### Incohérence n°2 — `H_fin` peut dépasser 100 %

`H_fin = H_produit_cible + H_amb` (§5). Avec `H_produit_cible` = 15 % et une
ambiance humide à 90 % (saison des pluies, nuit), `H_fin` = 105 % : la condition
`H_extr ≤ H_fin` est alors **vraie en permanence** et le cycle se termine dès la
première itération, séchoir vide de toute action.

Plus généralement, ce critère mesure une **humidité d'air excédentaire**, pas
l'humidité du produit : il suppose un régime de ventilation constant. Il reste
exploitable comme critère empirique, mais il doit être **corrélé par pesée** lors
de la campagne d'essais du mémoire avant d'être considéré comme un critère de
qualité produit.

**Traitement retenu :** `H_fin` est écrêté à `H_FIN_MAX` = 95 % (constante en
tête de fichier). L'écrêtage n'élimine pas le risque : si l'air extrait est
déjà à 92 %, le cycle s'arrêtera aussitôt. **Recommandation : borner
`H_produit_cible + H_amb` par construction en refusant le démarrage quand
`H_amb` dépasse ~70 %.**

### Incohérence n°3 — Le plancher 33 % interdit toute limitation de température

Le §4.1 impose qu'au moins une électrovanne reste ouverte tant que la combustion
est active, et le §12 interdit le palier 0 %. Conséquence directe : **rien dans
la régulation ne peut arrêter la chauffe**. Si la sonde `T_sec` se décale ou se
détache, ou si la charge de produit est faible, la chambre monte sans limite.
Pour un séchage alimentaire (typiquement 50–60 °C), c'est un risque de
dégradation du produit, voire d'incendie.

**Traitement retenu :** ajout d'une sécurité **distincte de la régulation** —
`T_sec ≥ T_SEC_MAX_SECURITE` (90 °C) provoque un arrêt propre vers
`SECHAGE_TERMINE`. Ce n'est **pas** un palier 0 % : c'est un arrêt de sécurité,
au même titre que la perte de flamme. La constante est en tête de fichier.
**Recommandation matérielle : doubler cette protection par un thermostat de
sécurité à réarmement manuel câblé en série sur l'alimentation des
électrovannes, indépendant de l'Arduino.**

### Incohérence n°4 — `T1`/`T2` indéfinis si `T_cible ≤ T_init`

Le §4.3 ne dit rien du cas `T_cible ≤ T_init` (possible en mode manuel, ou en
mode auto par temps chaud où `T_sec` au démarrage dépasse la consigne saisie).
`T1` et `T2` seraient alors au-dessus de `T_cible` et la modulation inversée.

**Traitement retenu :** `calculerSeuils()` rabat `T1 = T2 = T_cible` lorsque
l'écart est inférieur à `ECART_MIN_SEUILS` (3 °C). Le brûleur démarre alors à
100 % puis tombe directement au plancher 33 % une fois la consigne franchie.

### Point n°5 — `Press_H2` : seuil et défaut de boucle non spécifiés

Le §9.6 laisse `SEUIL_PRESS_H2_MIN` « à définir ». Une valeur de **2 bar** a été
retenue sur 10 bar de pleine échelle — **à ajuster sur le prototype** selon la
pression d'alimentation minimale du brûleur.

Par ailleurs, le transmetteur étant en 4–20 mA, un courant **inférieur à 4 mA**
signale une boucle coupée et non une pression nulle. `convertirPression()`
renvoie alors 0 bar, ce qui provoque la bascule vers le GPL : comportement
fail-safe volontaire. Le seuil de détection `ADC_PRESS_DEFAUT` suppose un shunt
de **250 Ω** (1–5 V) ; à corriger si le prototype utilise une autre valeur.

### Point n°6 — `SEUIL_MQ8` / `SEUIL_MQ6` non calibrés

Le §9.6 les donne « à calibrer ». Valeur provisoire : 350 (sur 1023). La
procédure de calibrage est rappelée en commentaire dans le code. **Ces seuils
doivent impérativement être relevés sur le prototype, capteurs préchauffés
24 h, avant toute mise en gaz.**

### Point n°7 — Bascule H₂ → GPL sur échecs d'allumage répétés

Le cahier des charges ne déclenche la bascule que sur la pression. En pratique,
un brûleur H₂ qui échoue 3 fois de suite à l'allumage boucle indéfiniment
purge → allumage → purge. Le compteur `nb_echecs_allumage` est implémenté et
affichable, **mais aucune bascule automatique n'a été ajoutée** : ce serait une
règle de conduite non spécifiée. À arbitrer lors de la soutenance.

### Point n°8 — Câblage de l'arrêt d'urgence

Le §9.2 décrit `AU_Urgence` comme un booléen sans préciser la polarité. Le
programme suppose un **contact NC** (normalement fermé) câblé vers 0 V avec
`INPUT_PULLUP` : au repos la broche est à `LOW`, l'appui **ou une rupture de
fil** la met à `HIGH` et déclenche l'alarme. C'est la seule polarité acceptable
pour un arrêt d'urgence. **Vérifier le câblage du prototype.**

### Point n°9 — Blocage des temporisations par `delay()`

`PURGE_DUREE` (120 s) et `ALLUMAGE_TIMEOUT` (4 s) sont implémentés par
comparaison de `millis()`, jamais par `delay()`. C'est indispensable : un
`delay()` de 120 s rendrait la détection de fuite et l'arrêt d'urgence
inopérants pendant toute la purge. Les capteurs lents (DS18B20, DHT22) sont lus
en mode **asynchrone** pour la même raison.

---

## 4. Banc de tests (§11.4)

Le programme Arduino est compilé **tel quel** sur PC : `tests/test_sechoir.cpp`
inclut `sechoir_hybride.ino` et les bibliothèques matérielles sont remplacées
par les mocks de `tests/mocks/`. Le banc pilote les capteurs et les boutons puis
appelle `loop()`, exactement comme le ferait la carte.

```
make -C tests run
```

| Cas | Exigence couverte |
|---|---|
| 1 | Démarrage à froid : purge 120 s → allumage → palier 100 % |
| 2 | Les **quatre** transitions d'hystérésis + les **quatre** non-basculements dans la bande |
| 3 | Plancher 33 % : aucune extinction, même 25 °C au-dessus de la consigne |
| 4 | Échec d'allumage après 4 s : fermeture totale, nouvelle purge complète |
| 5 | Perte de flamme : fermeture des EV **dans l'itération même** (§7.3 règle 2) |
| 6 | Bascule H₂ → GPL : palier conservé à 67 %, purge de 120 s respectée |
| 7 | Fin de cycle sur `H_extr ≤ H_fin`, avec `H_fin` recalculé en continu |
| 8 | Passage automatique en `PROLONGATION`, sans nouvelle limite de temps |
| 9 | `URGENCE_ATEX` prioritaire + réarmement par les **deux** chemins, refusé tant que la cause persiste |
| 10 | `MODE_SOLAIRE` strictement passif, et retour à 100 % sur démarrage à froid |
| 11 | Menu LCD : navigation, édition d'une consigne, validation |
| 12 | Arrêt propre par `Btn_Stop` |

**Validation du banc par mutation.** Le banc a été vérifié en réintroduisant
délibérément deux défauts :

| Défaut injecté | Résultat |
|---|---|
| `H_HYST` ramenée de 5 °C à 0 °C | 5 échecs — les trois non-basculements et deux transitions |
| Électrovannes appliquées **avant** le test de flamme (défaut historique §7.3 règle 2) | 4 échecs sur le cas 5 |

Le banc détecte donc bien les régressions qu'il est censé prévenir.
