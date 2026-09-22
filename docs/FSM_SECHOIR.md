# Séchoir solaire hybride — Synthèse de la machine à états finis (v2)

Document d'accompagnement de `sechoir_hybride/sechoir_hybride.ino`.
Cette version 2 répond au second cahier des charges reçu, qui **révise
plusieurs règles du premier** (voir §1 « Avis critiques »). Les références
`§x` renvoient à ce second cahier des charges ; `§x v1` renvoie au premier
(toujours pertinent pour les points qu'il n'a pas révisés).

---

## 1. Avis critiques sur le cahier des charges v2

Ces remarques ont été formulées **avant** l'implémentation et communiquées à
l'opérateur, qui a tranché les points bloquants (voir décisions ci-dessous).
Elles sont reproduites ici pour la traçabilité du mémoire.

### 1.1 Le palier 0 % réintroduit le risque que le v1 interdisait

Le premier cahier des charges interdisait explicitement un palier 0 % : *« Ce
choix élimine par construction les cycles répétés d'extinction/rallumage »*.
Le second l'exige. Conséquence directe et **assumée** : puisque toute
réouverture de gaz exige une purge complète (§8), repasser de 0 % à 33 % à
chaque fois que `T_sec` redescend sous le seuil implique un cycle complet
purge → allumage. Autour d'une consigne stable, cela peut se produire
plusieurs fois par heure : usure de l'allumeur, gaz consommé en pure
ventilation, et une coupure de chauffe pendant toute la durée de la purge à
chaque réamorçage.

**Mitigation ajoutée, au-delà de la lettre du cahier des charges** : après la
purge consécutive à une coupure au palier 0 %, le programme **revérifie la
température avant de rouvrir le gaz** (voir `gererCombustion()`, cas
`PH_PURGE` avec `raison_purge == PURGE_PALIER_0`). Si la demande de 33 % n'est
plus là, on ne gaspille pas un allumage : le système reste en veille, gaz
fermé, ventilation active, et revérifie à chaque itération. Le rallumage se
fait alors directement au palier 33 % (jamais 100 %), cohérent avec le sens
des transitions du §4.

### 1.2 Deux contradictions internes au cahier des charges v2

- **`Temps_Prolongation`** apparaît dans la liste des paramètres de
  configuration (§17), alors que le v1 imposait explicitement l'absence de
  toute limite de temps pour `PROLONGATION` (*« elle dure jusqu'à H_fin, sans
  nouvelle limite »*). **Décision de l'opérateur (validée avant
  implémentation) : ajouter le plafond.** Conséquence assumée : le cycle peut
  se terminer sans que l'humidité cible soit atteinte. Le programme le
  signale sans ambiguïté (`arret_force_duree`, affiché en toutes lettres sur
  l'écran de fin de cycle : *« ARRET FORCE (duree) / Humidite non
  garantie »*), pour qu'aucun opérateur ne confonde une fin normale et une fin
  forcée.

- **« `T_sec` atteint durablement `T_cible` »** comme déclencheur de fin de
  cycle (§6) est risqué pris littéralement : une fois la régulation stabilisée
  (souvent en moins d'une heure), le séchoir passe le plus clair du cycle à
  `T_cible` — bien avant que le produit soit sec. **Interprétation retenue,
  volontairement restrictive** : ce critère est verrouillé par le même
  `Temps_Min_Fin` que le critère hygrométrique, et concrétisé par « palier 0 %
  maintenu depuis au moins 60 s » (`CONFIRMATION_PALIER_0_MS`), qui est
  précisément le moment où la régulation décide que la consigne est atteinte.
  Voir `conditionFinDeCycle()`.

### 1.3 `H_initial` reste sans usage dans le calcul de fin de cycle

Le cahier des charges v2 réintroduit `H_initial` (supprimée dans le v1 comme
« non utilisée par la commande ») et donne pourtant la même formule
`H_fin = H_produit_cible + H_amb`, qui ne s'en sert pas — y compris dans son
propre exemple chiffré (80 % cité mais absent du calcul). `H_initial` est donc
implémentée comme **paramètre saisi, affiché et sauvegardé, mais purement
informatif/journal** (traçabilité produit pour le mémoire), sans effet sur la
décision de fin de cycle. Si une pondération par l'humidité initiale est
souhaitée (ex. un critère relatif « % d'humidité retirée »), c'est une
évolution de formule à spécifier séparément — elle n'a pas été inventée ici.

### 1.4 Échecs d'allumage répétés hors basculement

Le cahier des charges v2 n'exige l'écran d'erreur avec menu
RÉESSAYER/MANUEL/AUTOMATIQUE (§13) que pour un **échec de basculement**. Pour
un échec d'allumage ordinaire (démarrage à froid, ou nouvelle tentative après
perte de flamme), rien n'est dit sur une éventuelle limite — la lecture
littérale autoriserait donc une boucle purge → allumage → échec indéfinie.
**Ajout de sécurité (au-delà du texte)** : après `MAX_ECHECS_AVANT_ALARME` (3)
échecs consécutifs, quelle qu'en soit la cause, la même escalade est
déclenchée (`entrerErreurCombustion(ERR_ALLUMAGE_REPETE)`). Un basculement raté
déclenche toujours l'escalade dès le premier échec, conformément au texte.

### 1.5 Retour automatique GPL → H2

Le §11 ne décrit explicitement que le sens H2 → GPL, en demandant que « la
même logique fonctionne dans l'autre sens ». Puisque le §2 établit H2
prioritaire sur le GPL (« secours uniquement »), un retour automatique dès que
la pression H2 redevient suffisante est l'extension la plus cohérente avec
cette hiérarchie — **ajoutée** dans `arbitrageSource()`, avec une marge de
0,5 bar au-dessus de `Press_H2_Min` pour éviter un aller-retour de source au
voisinage immédiat du seuil.

### 1.6 Sécurité des paramètres de purge/allumage/fuite

Le §7 demande que la durée de purge, le délai d'allumage et les seuils MQ8/MQ6
soient éditables au menu opérateur. Les rendre éditables **sans aucune
limite** permettrait de réduire la purge à quasiment rien, ce qui annule sa
fonction de sécurité. **Défense en profondeur ajoutée** : chaque champ reste
modifiable comme demandé, mais borné par une plage sûre non contournable
(`bornerConfig()`, appelée après chaque édition et après tout chargement
EEPROM) — par exemple, la purge ne peut pas descendre sous 60 s quel que soit
le réglage opérateur. **Recommandation non implémentée** (changement
d'architecture trop lourd pour ce lot) : séparer ces réglages de sécurité dans
un menu technique protégé (séquence de déverrouillage dédiée), distinct du
menu de conduite quotidien.

---

## 2. Tableau de synthèse — États principaux

| État | Rôle | Actions (sorties) | Transitions sortantes |
|---|---|---|---|
| `ATTENTE_DEMARRAGE` | État initial / repli après réarmement | Tout fermé, PWM à 0 | `Btn_Menu` → `CONFIG_MENU` ; `Btn_Start` → démarrage ; cause ATEX → `URGENCE_ATEX` |
| `CONFIG_MENU` | Saisie des 16 paramètres opérateur (LCD 20×4 + 4 boutons) | Gaz fermé, PWM à 0 | `Btn_OK` (hors Réarmement) → sauvegarde EEPROM + `ATTENTE_DEMARRAGE` ; `Btn_Start` → démarrage direct |
| `MODE_SOLAIRE` | Séchage **purement passif** : aucune électrovanne | Gaz fermé ; ventilation dédiée | Solaire insuffisant → combustion (démarrage à froid, 100 %) ; fin de cycle → `FIN_TEMPORISATION` ; durée max → `PROLONGATION` |
| `MODE_H2` | Combustion hydrogène (priorité 2) | `gererCombustion(true)` | Solaire redisponible → `MODE_SOLAIRE` ; H2 indisponible → bascule GPL ; idem fin de cycle / prolongation |
| `MODE_GPL` | Combustion GPL (priorité 3), **même code** que H2 | `gererCombustion(false)` | H2 redisponible → bascule H2 ; idem `MODE_H2` |
| `PROLONGATION` | `Duree_Max_Cycle` dépassée sans critère de fin | Régulation poursuivie sans rupture | Fin de cycle → `FIN_TEMPORISATION` ; **`Temps_Prolongation` dépassé → arrêt forcé direct vers `SECHAGE_TERMINE`** (AVIS §1.2) |
| `FIN_TEMPORISATION` | Consigne atteinte : confirmation avant arrêt (§6) | Régulation **poursuivie** ; LCD affiche le compte à rebours | `Btn_OK`/`Btn_Stop` → arrêt immédiat ; `Btn_Menu` → report (relance le délai) ; condition disparue → reprise automatique ; délai écoulé → arrêt auto |
| `SECHAGE_TERMINE` | Cycle terminé (normal ou forcé) | Gaz fermé, extraction faible | `Btn_Start` → nouveau cycle ; `Btn_Menu` → `CONFIG_MENU` ; 5 min → `ATTENTE_DEMARRAGE` |
| `ERREUR_COMBUSTION` | Basculement raté ou échecs d'allumage répétés (§13) | Gaz fermé, buzzer actif, menu 3 choix | `OK` sur RÉESSAYER/AUTOMATIQUE → nouvelle purge + reprise ; `OK` sur MANUEL → mode manuel + `ATTENTE_DEMARRAGE` ; `Btn_Stop` → `SECHAGE_TERMINE` |
| `URGENCE_ATEX` | **Parallèle et prioritaire**, accessible depuis tout état (§15) | Fermeture totale, purge à 255, buzzer | Réarmement (bouton **ou** menu), seulement si la cause a disparu → `ATTENTE_DEMARRAGE` |

### Transitions prioritaires (hors du `switch`, dans cet ordre)

1. Cause ATEX (fuite ou AU) → `URGENCE_ATEX`, tout est écrasé.
2. `Btn_Stop` en cycle → `SECHAGE_TERMINE`.
3. Surchauffe (`T_sec >= 90 °C`) → `SECHAGE_TERMINE` (indépendant du palier 0 %).
4. Plafond `Temps_Prolongation` dépassé → arrêt forcé.
5. Condition de fin de cycle (verrouillée par `Temps_Min_Fin`) → `FIN_TEMPORISATION`.
6. `Duree_Max_Cycle` dépassée → `PROLONGATION`.

---

## 3. Modulation 100/67/33/0 % (§2, §4)

| Palier | EV1 | EV2 | EV3 | Condition d'entrée (depuis le palier voisin) |
|---|---|---|---|---|
| 100 % | 1 | 1 | 1 | Démarrage à froid, ou `T_sec < T1 - Hhyst/2` |
| 67 % | 0 | 1 | 1 | `T_sec >= T1 + Hhyst/2`, ou `T_sec < T2 - Hhyst/2` |
| 33 % | 0 | 0 | 1 | `T_sec >= T2 + Hhyst/2`, ou reprise après coupure 0 % |
| 0 % | 0 | 0 | 0 | `T_sec >= T3 + Hhyst/2` (coupure **volontaire**, pas une panne) |

`T1`, `T2`, `T3` sont calculés par `calculerSeuils()` (équivalent du
`calculateTemperatureThresholds()` demandé) :

```
T1 = T_init + (T_cible - T_init) / 3
T2 = T_init + 2 (T_cible - T_init) / 3
T3 = T_cible
```

`T3 = T_cible` est une interprétation (le cahier des charges ne donne pas sa
formule explicitement, seulement la règle « on coupe quand la consigne est
atteinte ») — voir §4.3 du code pour la justification complète.

### Séquence d'allumage (H2 et GPL, fonction unique `gererCombustion()`)

```
PURGE (Temps_Purge)         gaz fermé, Spark=0, ventilation de purge
   ↓ échéance
ALLUMAGE (≤ Temps_Allumage) ouverture au palier courant + Spark
   ├── Flame=1  → PALIER_100/67/33 (selon le palier), compteur d'échecs remis à 0
   └── Flame=0  → échec :
         - si la purge en cours était un BASCULEMENT → ERREUR_COMBUSTION direct
         - sinon, si échecs consécutifs >= 3        → ERREUR_COMBUSTION
         - sinon                                     → nouvelle PURGE complète

PALIER_100/67/33   régulation par hystérésis
   ├── Flame=0 inattendue → PURGE (ou ERREUR_COMBUSTION si >=3 échecs)
   └── majPalier() → PALIER_0 → coupure volontaire, PURGE (raison PALIER_0)
```

### Basculement H2 ↔ GPL

```
Étape 1  Fermeture de la source active + des 3 EV (armerPurge(PURGE_BASCULEMENT))
Étape 2  Purge complète (Temps_Purge) — le PALIER COURANT EST CONSERVÉ
Étape 3  Fin de purge → ALLUMAGE : ouverture de la nouvelle source + du palier conservé + Spark
Étape 4  Flame=1 → reprise de la régulation à ce palier
         Flame=0 (échéance Temps_Allumage) → ERREUR_COMBUSTION immédiat (§13),
         jamais de boucle silencieuse
```

Le retour GPL → H2 suit exactement la même séquence, déclenché automatiquement
dès que `Press_H2 >= Press_H2_Min + 0,5 bar` (marge anti-court-cycle, voir
Avis §1.5).

---

## 4. Structure de configuration (`Config`, §17)

Tous les paramètres opérateur sont dans une seule structure, sauvegardée en
EEPROM (mémoire interne du Mega 2560, aucun composant supplémentaire) et
rechargée au démarrage avec repli sur des valeurs par défaut si l'EEPROM est
vierge ou invalide (sentinelle de version). Chaque champ est borné après
édition (`bornerConfig()`), y compris contre une EEPROM partiellement
corrompue.

| Champ | Unité | Bornes | Sécurité |
|---|---|---|---|
| `T_cible`, `T_init` | °C | 30–90 / 0–60 | — |
| `H_initial`, `H_produit_cible` | % | 0–100 / 1–50 | — |
| `Duree_Max_Cycle` | min | 15–1440 | — |
| `Temps_Prolongation` | min | 15–720 | Voir Avis §1.2 |
| `Hhyst` | °C | 1–15 | — |
| `Temps_Min_Fin` | min | 10–1440 | Plancher 10 min (évite un 0 accidentel) |
| `Temps_Arret_Auto` | s | 30–1800 | — |
| `Temps_Purge` | s | **60–300** | **Plancher de sécurité non contournable** |
| `Temps_Allumage` | s | 2–10 | — |
| `Press_H2_Min` | bar | 0,5–8 | — |
| `Seuil_MQ8`, `Seuil_MQ6` | / 1023 | 100–900 | — |

---

## 5. Écran — décision matérielle

Passage du LCD 16×2 I2C au **20×4 I2C**, décidé avec l'opérateur avant
implémentation (même bus I2C, même bibliothèque `LiquidCrystal_I2C` : seul le
module physique change, aucune broche supplémentaire). Ce choix était
nécessaire pour afficher lisiblement les ~15 paramètres du menu et les deux
pages de télémesure (process/combustion et contexte/consignes) exigées au
§16. L'adresse I2C du nouveau module (souvent `0x27` ou `0x3F` selon le
fournisseur) doit être vérifiée avec un scanner I2C au premier branchement —
`LCD_ADRESSE` est une constante isolée pour ce réglage.

**Non vérifié physiquement** : l'alignement exact des caractères sur un écran
réel. Le banc de tests contrôle uniquement que chaque ligne composée tient
dans 20 caractères (troncature sûre par `snprintf`, jamais de dépassement
mémoire) — voir §6.

---

## 6. Banc de tests

```
make -C tests run
```

139 vérifications sur 16 cas, dont les nouveautés v2 :

| Cas | Exigence couverte |
|---|---|
| 2 | Hystérésis à **quatre** niveaux, y compris la coupure volontaire à 0 % et le non-gaspillage d'un allumage si la demande n'est plus là au réveil |
| 5 | Escalade après 3 échecs d'allumage consécutifs (ajout, Avis §1.4) |
| 6, 7 | Basculement réussi (palier conservé) et basculement **raté** (escalade dès le 1er échec, menu REESSAYER/MANUEL/AUTOMATIQUE) |
| 8 | Retour automatique GPL → H2 avec marge anti-court-cycle (ajout, Avis §1.5) |
| 9 | Verrouillage du critère hygrométrique par `Temps_Min_Fin` |
| 10 | `FIN_TEMPORISATION` : confirmation, **report** (et non « annulation », voir note ci-dessous), arrêt automatique |
| 11 | Plafond `Temps_Prolongation` : arrêt forcé, `arret_force_duree` activé |
| 14 | Verrou défensif : `ecrireSorties()` referme les deux vannes source si jamais les deux étaient demandées simultanément |
| 15 | Persistance EEPROM : une valeur modifiée au menu survit à un rechargement (`chargerConfig()`) |

**Note de conception découverte en testant** : la première version du cas 10
prévoyait un « Menu = annulation complète, retour en régulation normale ».
Le test a révélé que cette conception se re-déclenchait aussitôt (la
condition hygrométrique restant vraie, la transition prioritaire n°5
replongeait immédiatement en `FIN_TEMPORISATION`). Corrigé en un **report**
(relance du délai de confirmation) — c'est le seul comportement cohérent tant
que la condition qui a déclenché la demande d'arrêt persiste réellement.

**Validé par mutation** : désactiver l'escalade immédiate sur échec de
basculement (`if (false)` à la place de `if (raison_purge ==
PURGE_BASCULEMENT)`) fait échouer 3 vérifications du cas 7 ; restauré ensuite,
le banc repasse à 139/139.

---

## 7. Ce qui reste à valider sur le prototype réel

- Calibrage de `Seuil_MQ8` / `Seuil_MQ6` (24 h de préchauffage, procédure en
  commentaire dans le code).
- Adresse I2C du nouvel écran 20×4 et alignement visuel des lignes.
- `Press_H2_Min` : valeur réelle selon la pression minimale d'alimentation du
  brûleur (2 bar est un point de départ, pas une mesure).
- Comportement réel du contrôleur de flamme à l'extinction volontaire
  (palier 0 %) : le programme suppose qu'il retombe à 0 sans particularité,
  ce qui n'a pu être vérifié que par simulation (voir `tests/`).
- Endurance EEPROM : l'écriture n'a lieu qu'à la sortie du menu (jamais à
  chaque pression de bouton), ce qui reste largement dans la marge des
  ~100 000 cycles d'écriture de l'EEPROM AVR pour un usage normal.
