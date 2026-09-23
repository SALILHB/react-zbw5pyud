# Séchoir solaire hybride — Synthèse de la machine à états finis (v3)

Document d'accompagnement de `sechoir_hybride/sechoir_hybride.ino`.
La **version 3** applique les décisions prises avec l'opérateur lors de la
revue complète de la logique (§0). Elle part de la version 2 (second cahier
des charges, références `§x`) ; `§x v1` renvoie au premier cahier des charges.
Le firmware est la **source de vérité** : le modèle Simulink
(`docs/FSM_SIMULINK.md`) et le mémoire s'alignent sur lui.

---

## 0. Décisions de la version 3

| # | Sujet | Décision |
|---|---|---|
| 1 | Fin de cycle | Critère 1 (prioritaire) : `H_sec <= H_fin` après `Temps_Min_Fin`. Critère 2 : `Duree_Max_Cycle`. Les deux mènent à **`DEMANDE_PROLONGATION`**. `T_sec` qui atteint `T_cible` **n'est plus** un critère de fin : c'est seulement le palier 0 % de la régulation. |
| 2 | Prolongation | L'opérateur choisit N min (proposé : `Prolong_Defaut`, réglable UP/DOWN). OK → `PROLONGATION` ; STOP → fin ; **sans réponse pendant `Temps_Reponse` (5 min) → `SECHAGE_TERMINE`, brûleur à 0 %**. À la fin des N min, on redemande. Si l'humidité est atteinte pendant une prolongation alors qu'elle ne l'était pas encore, on redemande tout de suite. |
| 3 | Supprimé | `FIN_TEMPORISATION` (report, annulation auto), prolongation automatique et son plafond `Temps_Prolongation`, critère « palier 0 % durable ». |
| 4 | `T_cap` | La sonde de plaque voit la chaleur du brûleur intégré au capteur : on ne reviendrait jamais au solaire. **`T_cap = T_amb + DeltaT_Sol`** (estimation, `DeltaT_Sol` paramétrable, à calibrer). La sonde reste lue pour affichage (`T_cap_mesure`). `T_cap` ne sert qu'à choisir la source. |
| 5 | Seuils solaires | `ON = T_cible + Marge_Sol`, `OFF = ON − Hyst_Sol` (paramétrables ; `Marge_Sol = 0` par défaut). |
| 6 | Repère FROID / CHAUD | **CHAUD si `T_sec >= Seuil_Chaud`** (bande ±`Hhyst`/2), FROID sinon. FROID : le solaire doit atteindre ON pour être choisi ou gardé, allumage à 100 %. CHAUD : le solaire est toléré jusqu'à OFF (inertie de la chambre), allumage au palier correspondant à `T_sec`. Quitter la combustion pour le solaire exige toujours ON. |
| 7 | Retour au solaire | Extinction **normale** (pas une urgence) suivie d'une **post-purge** de `Temps_Purge`. |
| 8 | Perte de flamme en régulation | Gaz fermé dans la même itération. 1re perte du cycle → relance (purge + allumage) ; 2e → **URGENCE** (cause « PERTE FLAMME »). |
| 9 | Flamme vue, gaz fermé | Plus de 5 s (`DELAI_FLAMME_PARASITE_MS`) → **URGENCE** (cause « FLAMME PARASITE » : vanne qui fuit ou capteur défaillant). |
| 10 | Échec après bascule H2↔GPL | Même règle que partout : 3 essais, chacun précédé d'une purge, puis `ERREUR_COMBUSTION`. La nouvelle source repart avec un compteur à 0. |
| 11 | Grandeurs FIXE / AUTO | `T_amb`, `H_amb`, `H_sec`, `Press_H2` : mesurées (AUTO) ou saisies (FIXE), pour continuer à fonctionner si un capteur tombe en panne. **Jamais** pour `T_sec`, MQ8, MQ6, flamme, arrêt d'urgence. |
| 12 | `T_sec` en panne | Pas de mode fixe : **palier imposé** (`Regul_Auto = false`, `Palier_Impose`). La sécurité surchauffe reste active sur la mesure. |
| 13 | Période de régulation | La comparaison `T_sec` / seuils se fait toutes les `Periode_Regul` secondes (1 s par défaut). |
| 14 | `T_cible` en cours de cycle | UP/DOWN pendant le cycle modifient `T_cible` ; `T1/T2/T3` sont recalculés tout de suite, `T_init` reste celle du démarrage. |

---

## 1. Avis critiques (hérités de la v2, mis à jour)

### 1.1 Le palier 0 % réintroduit le risque que le v1 interdisait

Le premier cahier des charges interdisait un palier 0 % (*« Ce choix élimine
par construction les cycles répétés d'extinction/rallumage »*). Le second
l'exige. Puisque toute réouverture de gaz exige une purge complète (§8),
repasser de 0 % à 33 % implique un cycle purge → allumage, parfois plusieurs
fois par heure autour d'une consigne stable.

**Mitigation** : après une coupure au palier 0 %, le programme revérifie la
température avant de rouvrir le gaz. Si la demande n'est plus là, il reste en
veille (gaz fermé, ventilation active) **sans relancer le chronomètre de
purge** : la purge est acquise une fois pour toutes pendant la veille. Le
rallumage se fait directement à 33 %.

### 1.2 Critère « T_sec atteint durablement T_cible » — **supprimé en v3**

La v2 l'avait conservé (verrouillé par `Temps_Min_Fin` et 60 s de palier 0 %).
L'opérateur l'a supprimé en v3 : une température atteinte ne signifie pas un
produit sec. Le plafond `Temps_Prolongation`, autre point litigieux de la v2,
est lui aussi supprimé : chaque prolongation exige désormais une action de
l'opérateur, ce qui rend un plafond global inutile.

### 1.3 `H_initial` reste sans usage dans le calcul de fin de cycle

`H_fin = H_produit_cible + H_amb` ne fait pas intervenir `H_initial`, qui est
donc saisie, affichée et sauvegardée à titre informatif. **Remarque pour le
mémoire** : cette formule additionne une humidité visée du produit et
l'humidité relative de l'air extérieur ; elle doit être justifiée
physiquement.

### 1.4 Échecs d'allumage répétés

Après `MAX_ECHECS_AVANT_ALARME` (3) échecs consécutifs, `ERREUR_COMBUSTION`
avec menu RÉESSAYER / MANUEL / AUTOMATIQUE. **V3** : la même règle s'applique
après une bascule H2↔GPL (la v2 escaladait dès le 1er échec de bascule).

### 1.5 Retour automatique GPL → H2

H2 est prioritaire sur le GPL (§2). Retour automatique dès que
`Press_H2 >= Press_H2_Min + 0,5 bar` (marge anti-court-cycle).

### 1.6 Sécurité des paramètres de purge/allumage/fuite

Chaque champ reste modifiable, mais borné par `bornerConfig()` (la purge ne
peut pas descendre sous 60 s). Le délai de flamme parasite (5 s) est une
constante de sécurité non réglable au menu. **Recommandation non
implémentée** : un menu technique protégé pour les réglages de sécurité.

### 1.7 `T_cap = T_amb + DeltaT_Sol` : limites à écrire dans le mémoire (v3)

Ce choix de l'opérateur (option B parmi quatre proposées) contourne la sonde
contaminée par le brûleur, mais **ne mesure pas le soleil** : une journée
chaude et couverte sera prise pour une journée ensoleillée, et une journée
froide mais très ensoleillée sera sous-exploitée. `DeltaT_Sol` doit être
calibré sur site. Les alternatives plus justes physiquement restent
possibles plus tard : lire la vraie sonde uniquement brûleur éteint depuis un
certain temps, ou ajouter un capteur de lumière (T_cap = T_amb + Ccap × G).

### 1.8 Palier imposé : pas de protection si la sonde T_sec meurt (v3)

Le palier imposé sert quand la régulation par `T_sec` n'est pas souhaitée.
Mais si la sonde `T_sec` est défaillante, la sécurité surchauffe logicielle
est elle aussi aveugle. **Recommandation matérielle** : un thermostat de
sécurité indépendant à réarmement manuel sur la chambre, qui coupe
l'alimentation des électrovannes quel que soit le programme.

### 1.9 Capteur de flamme UV / IR (v3)

Temps de réponse < 1 s, compatible avec l'exigence « < 2 s ». Le programme lit
la flamme à chaque itération (quelques ms) et ferme le gaz dans la même
itération. **Points à vérifier sur site** : un capteur UV peut voir l'étincelle
d'allumage, et un capteur IR peut voir le rayonnement de l'absorbeur chaud
(brûleur intégré au capteur) : risque de fausse détection, qui serait
signalée par l'urgence « FLAMME PARASITE ».

---

## 2. Tableau de synthèse — États principaux

Dans le firmware, `etat_courant` porte l'état **affiché**. La source
(`Source_Active`) et la combustion (`phase_combustion`) continuent de tourner
pendant `DEMANDE_PROLONGATION` et `PROLONGATION` (`etapeRegulation(false)`) :
c'est l'équivalent des deux régions parallèles SOURCE et PHASE du modèle
Simulink.

| État | Rôle | Actions (sorties) | Transitions sortantes |
|---|---|---|---|
| `ATTENTE_DEMARRAGE` | État initial / repli après réarmement | Tout fermé, PWM à 0 | `Btn_Menu` → `CONFIG_MENU` ; `Btn_Start` → démarrage |
| `CONFIG_MENU` | Saisie des paramètres (LCD 20×4 + 4 boutons) | Gaz fermé, PWM à 0 | `Btn_OK` → sauvegarde EEPROM + `ATTENTE_DEMARRAGE` ; `Btn_Start` → démarrage direct |
| `MODE_SOLAIRE` | Séchage passif, aucune électrovanne | Gaz fermé ; post-purge éventuelle puis ventilation solaire | Solaire insuffisant (sous ON en FROID, sous OFF en CHAUD) → combustion ; fin de cycle → `DEMANDE_PROLONGATION` |
| `MODE_H2` | Combustion hydrogène (priorité 2) | `gererCombustion(true)` | `T_cap >= ON` → solaire ; H2 indisponible → bascule GPL ; fin de cycle → `DEMANDE_PROLONGATION` |
| `MODE_GPL` | Combustion GPL (priorité 3), même code que H2 | `gererCombustion(false)` | H2 redisponible → bascule H2 ; idem `MODE_H2` |
| `DEMANDE_PROLONGATION` | Fin atteinte : « Prolonger N min ? » | Régulation **poursuivie** au même palier | OK → `PROLONGATION` ; STOP → `SECHAGE_TERMINE` ; sans réponse `Temps_Reponse` → `SECHAGE_TERMINE` (0 %) |
| `PROLONGATION` | Prolongation de N min choisie | Régulation poursuivie, palier conservé | N min écoulées → `DEMANDE_PROLONGATION` ; humidité atteinte (si pas encore) → `DEMANDE_PROLONGATION` |
| `SECHAGE_TERMINE` | Cycle terminé | Gaz fermé (0 %), extraction faible | `Btn_Start` → nouveau cycle ; `Btn_Menu` → `CONFIG_MENU` ; 5 min → `ATTENTE_DEMARRAGE` |
| `ERREUR_COMBUSTION` | 3 échecs d'allumage consécutifs | Gaz fermé, buzzer, menu 3 choix | RÉESSAYER / AUTOMATIQUE → purge + reprise au palier mémorisé ; MANUEL → mode manuel + `ATTENTE_DEMARRAGE` ; `Btn_Stop` → `SECHAGE_TERMINE` |
| `URGENCE_ATEX` | Prioritaire, depuis tout état | Fermeture totale, purge et extraction à 255, buzzer, cause affichée | Réarmement (bouton **ou** menu) seulement si aucune cause présente (fuite, AU, **flamme encore vue**) → `ATTENTE_DEMARRAGE` |

### Transitions prioritaires (hors du `switch`, dans cet ordre)

1. **a.** Fuite (MQ8 / MQ6) ou arrêt d'urgence → `URGENCE_ATEX`, tout est écrasé.
   **b.** Flamme vue gaz commandé fermé pendant plus de 5 s → `URGENCE_ATEX`.
   *(La perte de flamme répétée en régulation déclenche aussi l'urgence, depuis `gererCombustion()`.)*
2. `Btn_Stop` en cycle → `SECHAGE_TERMINE` (sauf en `DEMANDE_PROLONGATION`, qui le gère lui-même).
3. Surchauffe (`T_sec >= 90 °C`) → `SECHAGE_TERMINE`.
4. Fin de cycle → `DEMANDE_PROLONGATION` :
   - en phase normale : humidité atteinte (prioritaire), sinon `Duree_Max_Cycle` ;
   - en prolongation : humidité atteinte si elle ne l'était pas encore, sinon fin des N min.

---

## 3. Modulation 100/67/33/0 % (§2, §4)

| Palier | EV1 | EV2 | EV3 | T_sec monte (puissance baisse) | T_sec descend (puissance remonte) |
|---|---|---|---|---|---|
| 100 % | 1 | 1 | 1 | `>= T1 + Hhyst/2` → 67 % | — |
| 67 % | 0 | 1 | 1 | `>= T2 + Hhyst/2` → 33 % | `< T1 − Hhyst/2` → 100 % |
| 33 % | 0 | 0 | 1 | `>= T3 + Hhyst/2` → 0 % | `< T2 − Hhyst/2` → 67 % |
| 0 % | 0 | 0 | 0 | — (veille, gaz fermé) | `< T3 − Hhyst/2` → rallumage à 33 % |

```
T1 = T_init + (T_cible − T_init) / 3
T2 = T_init + 2 (T_cible − T_init) / 3
T3 = T_cible
```
Exemple : `T_init = 25`, `T_cible = 55`, `Hhyst = 4` → T1 = 35, T2 = 45,
T3 = 55. La comparaison a lieu toutes les `Periode_Regul` secondes. En palier
imposé, pas de comparaison : `palier = Palier_Impose`.

**Palier d'allumage** (démarrage de cycle, ou solaire devenu insuffisant) :
palier imposé si `Regul_Auto = false` ; sinon 100 % en régime FROID, et en
régime CHAUD le palier qui correspond à `T_sec` (T_sec < T1 → 100 %, < T2 →
67 %, < T3 → 33 %, sinon veille à 0 %).

### Séquence d'allumage (H2 et GPL, fonction unique `gererCombustion()`)

```
PURGE (Temps_Purge)         gaz fermé, Spark=0, ventilation de purge
   ↓ échéance
ALLUMAGE (≤ Temps_Allumage) ouverture au palier courant + Spark
   ├── Flame=1  → PALIER_100/67/33, compteur d'échecs remis à 0
   └── Flame=0  → échec n : si n >= 3 → ERREUR_COMBUSTION
                            sinon     → nouvelle PURGE complète (120 s)

PALIER_100/67/33   régulation par hystérésis (toutes les Periode_Regul s)
   ├── Flame=0 inattendue → gaz fermé immédiatement ;
   │      1re fois dans le cycle → PURGE puis relance
   │      2e fois               → URGENCE (PERTE FLAMME)
   └── palier 0 % → coupure volontaire, PURGE (raison PALIER_0) → veille
```

Exemple chronologique de 3 échecs : purge 0–120 s, allumage 120–124 s (échec
1), purge 124–244 s, allumage 244–248 s (échec 2), purge 248–368 s, allumage
368–372 s (échec 3) → `ERREUR_COMBUSTION`.

### Basculement H2 ↔ GPL

```
Étape 1  Fermeture de la source active + des 3 EV (armerPurge(PURGE_BASCULEMENT))
Étape 2  Purge complète (Temps_Purge) — le PALIER COURANT EST CONSERVÉ (mémoire)
Étape 3  Fin de purge → ALLUMAGE de la nouvelle source au palier conservé
Étape 4  Flame=1 → la régulation reprend et applique aussitôt la règle
         d'hystérésis (si T_sec a baissé pendant la purge, le palier remonte)
         Flame=0 → règle normale des 3 essais (compteur remis à 0 à la bascule)
```

### Arbitrage solaire (mode automatique)

| Situation | Règle |
|---|---|
| Démarrage, régime FROID | solaire si `T_cap >= ON`, sinon combustion |
| Démarrage, régime CHAUD | solaire si `T_cap >= OFF`, sinon combustion |
| En combustion | retour au solaire si `T_cap >= ON` (extinction normale + post-purge) |
| En solaire, FROID | combustion si `T_cap < ON` |
| En solaire, CHAUD | combustion si `T_cap < OFF` |

---

## 4. Structure de configuration (`Config`, §17)

Sauvegardée en EEPROM (sentinelle `0xC0E0` : une EEPROM de la v2 est
réinitialisée aux valeurs par défaut), bornée par `bornerConfig()` après
chaque édition et chaque chargement.

| Champ | Unité | Défaut | Bornes | Remarque |
|---|---|---|---|---|
| `T_cible`, `T_init` | °C | 55 / 25 | 30–90 / 0–60 | `T_cible` modifiable aussi en cycle |
| `H_initial`, `H_produit_cible` | % | 80 / 10 | 0–100 / 1–50 | — |
| `Duree_Max_Cycle` | min | 600 | 15–1440 | critère de fin n°2 |
| `Prolong_Defaut` | min | 30 | 5–720 | N proposé à chaque demande |
| `Temps_Reponse` | s | 300 | 30–1800 | sans réponse → fin |
| `Temps_Min_Fin` | min | 120 | 0–1440 | 0 = verrou désactivé |
| `Hhyst` | °C | 5 | 1–15 | — |
| `Periode_Regul` | s | 1 | 1–60 | DS18B20 : 750 ms |
| `Regul_Auto`, `Palier_Impose` | — | AUTO / 67 % | 100/67/33 % | palier imposé |
| `DeltaT_Sol` | °C | 20 | 0–60 | **à calibrer** |
| `Marge_Sol`, `Hyst_Sol` | °C | 0 / 5 | 0–20 / 1–20 | seuils solaires |
| `Seuil_Chaud` | °C | 40 | 20–80 | repère FROID / CHAUD |
| `Temps_Purge` | s | 120 | **60–300** | plancher de sécurité |
| `Temps_Allumage` | s | 4 | 2–10 | — |
| `Press_H2_Min` | bar | 2 | 0,5–8 | — |
| `Seuil_MQ8`, `Seuil_MQ6` | / 1023 | 350 | 100–900 | — |
| `Fixe_T_amb` / `Val_T_amb` | — / °C | AUTO / 25 | −10–55 | — |
| `Fixe_H_amb` / `Val_H_amb` | — / % | AUTO / 40 | 0–100 | — |
| `Fixe_H_sec` / `Val_H_sec` | — / % | AUTO / 60 | 0–100 | fin par durée seulement si FIXE |
| `Fixe_Press` / `Val_Press` | — / bar | AUTO / 5 | 0–10 | réservoir vide non détecté si FIXE |

---

## 5. Écran — décision matérielle

LCD **20×4 I2C** (même bus et même bibliothèque que le 16×2 d'origine).
Écrans v3 : cause d'urgence en toutes lettres (« URG: FUITE H2 »,
« URG: PERTE FLAMME », « URG: FLAMME PARASITE »…), demande de prolongation
(motif, durée N, compte à rebours), régime FROID/CHAUD en télémesure,
avertissement « FIXE: … » quand une grandeur est saisie au lieu d'être
mesurée. L'alignement sur un écran physique n'a pas été vérifié ; `snprintf`
garantit qu'aucune ligne ne dépasse 20 caractères.

---

## 6. Banc de tests

```
make -C tests run
```

**200 vérifications sur 21 cas**, dont les nouveautés v3 :

| Cas | Exigence couverte |
|---|---|
| 4 | Perte de flamme : 1re → relance après purge ; 2e → URGENCE « PERTE FLAMME » ; réarmement |
| 7 | Échec après bascule : 3 essais avec purge entre chaque, puis `ERREUR_COMBUSTION` |
| 9 | Humidité atteinte, verrouillée par `Temps_Min_Fin` → `DEMANDE_PROLONGATION` |
| 10 | Demande : durée réglable, OK → prolongation respectée, fin des N min → redemande, sans réponse → fin à 0 %, STOP → fin |
| 11 | Durée max → demande ; humidité atteinte pendant la prolongation → redemande immédiate |
| 12 | Urgence : cause affichée, réarmement refusé tant que la flamme est vue |
| 13 | Solaire gardé entre OFF et ON en régime CHAUD ; allumage à 33 % selon `T_sec` ; retour au solaire avec post-purge |
| 17 | Repère FROID/CHAUD au démarrage et en cycle |
| 18 | Flamme vue gaz fermé → URGENCE après 5 s, chrono remis à zéro si elle disparaît |
| 19 | Grandeurs FIXE : la valeur saisie remplace la mesure, `T_cap` estimée dessus |
| 20 | Palier imposé maintenu malgré `T_sec`, surchauffe toujours active |
| 21 | `T_cible` modifiée en cycle (seuils recalculés, `T_init` inchangée) ; `Periode_Regul` respectée |

**Validé par mutation** : 11 mutations, chacune désactivant une règle v3
(tolérer la 2e perte de flamme, ignorer la flamme parasite, supprimer le
repère, allumer toujours à 100 %, ignorer l'humidité en prolongation, ignorer
les valeurs fixes, supprimer la post-purge, escalader au 1er échec après
bascule, supprimer le délai de réponse, ignorer `Periode_Regul`, ignorer le
palier imposé). Chacune fait échouer au moins une vérification ; restauré, le
banc repasse à 200/200.

---

## 7. Ce qui reste à valider sur le prototype réel

- **Calibrage de `DeltaT_Sol`** (voir Avis §1.7) — sans lui, la bascule
  solaire n'a pas de sens physique.
- Calibrage de `Seuil_MQ8` / `Seuil_MQ6` (24 h de préchauffage).
- Capteur de flamme UV/IR : fausse détection possible sur l'étincelle ou sur
  l'absorbeur chaud (Avis §1.9).
- Thermostat de sécurité matériel indépendant (Avis §1.8).
- Adresse I2C de l'écran 20×4 et alignement visuel des lignes.
- `Press_H2_Min` réel selon le brûleur (2 bar est un point de départ).
- Endurance EEPROM : écriture à la sortie du menu et à chaque réglage de
  `T_cible` en cycle (seuls les octets modifiés sont réécrits) — largement
  dans la marge des ~100 000 cycles de l'EEPROM AVR.
