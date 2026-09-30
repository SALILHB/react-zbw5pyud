# Résultats des simulations — mémoire V4 (études D3 et E1 à E4)

Ce fichier sert à rédiger les commentaires `[À COMPLÉTER : commentaire …]` du
§6.6. **Tous les chiffres viennent des simulations.** Aucun n'est estimé à la
main, sauf les postes marqués *estimation* de E3, qui sont calculés sur les
signaux simulés.

**Moteur utilisé pour les valeurs ci-dessous : Simulink** (copie
`Etudes_Sechoir.slx` du modèle validé, `lancer_etudes('simulink')`). Ce sont
aussi les valeurs des figures et des tableaux du mémoire.

Les mêmes études ont d'abord été faites avec le **programme Arduino réel**,
compilé sur PC avec le même modèle physique (`tests/simulation_etudes.cpp`).
Les deux moteurs concordent :
- événements de la journée à 1 min près (bascule GPL à 10 h 54 contre 10 h 55) ;
- temps dans la bande à 1,5 point près ;
- énergies identiques au centième de kWh ;
- périodes à 0,1 min près ;
- puissances moyennes à 7 W près.

La petite différence de chronologie existait déjà dans le modèle validé. Le
programme coupe le palier environ 1 s plus tard que le chart à chaque cycle de
veille, soit 14,3 s d'écart cumulé après 1 h au scénario 1. La séquence des
états et des paliers, elle, est identique.

Comptes rendus complets : `matlab/captures/etudes/resultats_E1.txt` à
`resultats_E4.txt` (réécrits à chaque exécution).

## Hypothèses communes

| Grandeur | Valeur | Statut |
|---|---|---|
| Modèle thermique | UA = 10,2 W/K, C_eq = 3,60·10⁴ J/K (τ = 3530 s) | modèle validé |
| Brûleur | Pnom = 5000 W ; paliers 1 / 0,67 / 0,33 / 0 | modèle validé |
| Rendement du brûleur η_b | 0,80 | **hypothèse** (à remplacer par la valeur du Tableau 2.3) |
| PCI H2 / butane | 33,3 / 12,7 kWh/kg | donnée |
| CO2 du butane | 3,03 kg/kg | donnée |
| P_sol,max (ciel clair) | 20 / Kth = 204 W | donnée |
| Air | ρ = 1,06 kg/m³, c_p = 1006 J/(kg·K) → ṁc_p = 23,7 W/K (80 m³/h), 59,2 W/K (200 m³/h) | donnée |

---

## D3 — Scénarios 13 à 16 (Tableau 6.9)

Instants mesurés sur le programme Arduino (références `matlab/reference/scenario_13..16.csv`).

| N° | Réglage | Résultat du programme | Simulink (à relever) |
|---|---|---|---|
| 13 | `Press_H2` : 8 → 0,5 bar à 900 s, pendant la veille commencée à 597 s | GPL à 900 s ; **aucune ouverture de gaz** tant que T_sec > 52,5 °C ; rallumage GPL à **33 % à 1189 s** | |
| 14 | Flamme vue gaz fermé de 800 à 820 s (veille) ; réarmement à 900 s | **URGENCE à 805 s** (cause 5) ; retour à ATTENTE à 900 s | |
| 15 | Comme le scénario 3 (GPL à 1250 s), puis `Press_H2` = 8 bar à 2000 s | rallumage GPL à 33 % à 1370 s ; **retour sur H2 à 2000 s** ; rallumage H2 à **33 % à 2120 s** | |
| 16 | AU enfoncé de 400 à 600 s ; réarmement à 500 et 700 s ; START à 800 s | **URGENCE à 400 s** (cause 3) ; réarmement **refusé à 500 s**, **accepté à 700 s** ; allumage à 100 % à 920 s | |

Écart Simulink / programme attendu : 0,1 s (à confirmer avec `comparer_scenario(13:16)`).

---

## E1 — Journée type et absence d'une source (§6.6.2, §6.6.3)

**Hypothèses propres à E1**
- Cycle de 10 h, de 8 h à 18 h ; START à 8 h 00 min 10 s ; T_cible = 55 °C.
- T_amb(h) = 28 + 10 sin(2π(h − 9)/24) ; P_sol(h) = 204 W · k_ciel · max(0, sin(π(h − 6)/12)).
  Profils échantillonnés toutes les 60 s.
- **Humidité fixée à 90 %** (`Fixe_H_sec`). Sinon, le modèle de séchage
  illustratif termine le cycle vers 2 h et la journée ne serait pas simulée.
- **Stock d'hydrogène m0 = 28 g** (C4, C5), avec Press_H2 = 8 bar × (1 − m/m0).
  La bascule à 2 bar survient après 21,0 g consommés. C1 consomme 22,5 g, dont
  20,6 g avant 10 h. Avec m0 = 28 g, la bascule tombe donc au dernier
  rallumage de la matinée, à **10 h 54**.
- E_sol = énergie solaire reçue **pendant MODE_SOLAIRE** (définition de la
  demande). Dans le modèle, le soleil chauffe aussi la chambre pendant la
  combustion : l'énergie solaire totale reçue sur la journée est donnée à part
  (E_sol,tot).

| Cas | E_sol (kWh) | E_sol,tot | E_H2 | E_GPL | f_sol | Bande | Rallumages | H2 (g) | GPL (g) | CO2 (kg) | Événement |
|---|---|---|---|---|---|---|---|---|---|---|---|
| C1 | 0,79 | 1,46 | 0,60 | 0 | 57 % | 76 % | 6 | 22,5 | 0 | 0 | solaire à 11 h 58 ; 39 min sous 45 °C |
| C2 | 0 | 0,36 | 3,05 | 0 | 0 % | 100 % | 45 | 114,4 | 0 | 0 | jamais de solaire |
| C3 | 0,79 | 1,46 | 0 | 0,60 | 57 % | 76 % | 6 | 0 | 59,0 | 0,179 | GPL dès le départ ; solaire à 11 h 58 |
| C4 | 0,79 | 1,46 | 0,56 | 0,04 | 57 % | 76 % | 7 | 21,0 | 4,0 | 0,012 | bascule GPL à 10 h 54 ; solaire à 11 h 58 |
| C5 | 0 | 1,46 | 0,56 | 0 | 0 % | 76 % | 9 | 21,0 | 0 | 0 | bascule GPL à 10 h 54 ; **ERREUR_COMBUSTION à 11 h 00** |
| C6 | 1,46 | 1,46 | 0 | 0 | 100 % | 59 % | 0 | 0 | 0 | 0 | solaire seul ; T_max = 55,1 °C |
| C7 | 0 | 0 | 2,37 | 0 | 0 % | 100 % | 36 | 89,0 | 0 | 0 | sans soleil, H2 seul |
| C8 | 0,20 | 0,36 | 1,09 | 0 | 15 % | 40 % | 14 | 41,0 | 0 | 0 | solaire à 11 h 58 ; **291 min sous 45 °C** |

**Observations**
1. **Passage au solaire à 11 h 58 dans C1.** C'est le moment où T_amb atteint
   35 °C, donc où T_cap estimée = T_amb + 20 atteint le seuil ON de 55 °C.
   Avant 11 h 58, la commande régule sur l'hydrogène : 6 allumages, T_sec entre
   52,5 et 57,5 °C.
2. **Limite de l'estimation T_cap = T_amb + ΔT_Sol.** L'après-midi, T_amb reste
   au-dessus de 30 °C, donc T_cap estimée ≥ 50 °C (seuil OFF) jusqu'à 18 h. La
   commande reste en solaire alors que l'apport réel baisse : 204 W à 12 h,
   53 W à 17 h. T_sec quitte la bande à 15 h 40,
   passe sous 45 °C à 17 h 22 et finit à 41,2 °C à 18 h (39 min sous 45 °C dans C1).
   - Par journée chaude mais couverte (C8), l'effet est plus fort : la commande
     passe au solaire à 11 h 58 avec seulement 51 W d'apport. T_sec tombe
     sous 45 °C pendant 291 min, et le temps dans la bande chute à 40 %.
3. **Économie due au solaire.** E_gaz(C7) − E_gaz(C1) = 2,37 − 0,60 = 1,77 kWh,
   soit 75 % du gaz de C7. Mais C1 passe 24 % du temps hors de la bande,
   contre 0,4 % pour C7 : l'économie se paie en qualité de régulation
   l'après-midi.
4. **H2 épuisé, GPL disponible (C4).** Bascule sur le GPL à 10 h 54 (purge,
   palier conservé), 4,0 g de GPL (12 g de CO2), puis solaire à 11 h 58. La
   régulation n'est pas affectée (76 % dans la bande, comme C1).
5. **H2 épuisé, GPL indisponible (C5).** Après la bascule de 10 h 54, les
   essais d'allumage sur le GPL échouent. Le programme passe en
   ERREUR_COMBUSTION à 11 h 00 et y reste jusqu'à 18 h, faute d'action de
   l'opérateur : aucun arbitrage de source n'est fait dans cet état (programme,
   lignes 1505 à 1530). Le retour automatique au solaire à midi n'a donc pas
   lieu (E_sol = 0).
   - *Limite du modèle* : P_sol continue de chauffer la chambre dans le modèle
     même quand la ventilation de distribution est arrêtée. La courbe de T_sec
     de C5 après 11 h 00 est donc optimiste.

---

## E2 — Commande automatique, conduite manuelle, absence de régulation (§6.6.4)

**Hypothèses** : conditions du scénario 8 (T_amb = 25 °C, T_sec initiale
25 °C, T_cible = 55 °C), 2 h. M1 = commande automatique (chart). M2 à M4 = pilote
simple (`simuler_pilote.m`), même modèle thermique, purge de 120 s avant chaque
allumage, START à 10 s.

| Mode | T_max (°C) | Dans la bande | Hors 45–70 °C | Énergie (kWh) | Écart / M1 | Allumages | T_sec > 70 °C dès | ≥ 90 °C dès |
|---|---|---|---|---|---|---|---|---|
| M1 automatique | 57,5 | 99,8 % | 0 min | 0,91 | — | 10 | jamais | jamais |
| M2a manuel 15 min | 137,4 | 10 % | 62 min | 2,15 | +137 % | 2 | 7,8 min | 10,5 min |
| M2b manuel 30 min | 210,6 | 1 % | 106 min | 2,33 | +157 % | 1 | 7,8 min | 10,5 min |
| M3 thermostat TOR | 55,5 | 100 % | 0 min | 0,87 | −4 % | 27 | jamais | jamais |
| M4 sans régulation (33 %) | 164,9 | 2 % | 99 min | 3,24 | +257 % | 1 | 21,4 min | 32,4 min |

**Observations**
1. À 100 %, la chambre monte d'environ 8 K/min. Avec un relevé toutes les 15
   ou 30 min, l'opérateur coupe trop tard : T_sec dépasse 70 °C dès 7,8 min
   et 90 °C dès 10,5 min (M2a et M2b).
   - Au-delà de 90 °C, la sécurité du programme (T_SEC_MAX_SECURITE) aurait
     arrêté le cycle. Le modèle linéaire n'est plus représentatif à ces
     températures : les T_max de 137 et 211 °C montrent la tendance, pas une
     valeur physique.
2. Sans régulation (M4, 33 % en continu), T_sec dépasse 70 °C à 21 min et
   90 °C à 32 min. Elle tend vers T_amb + 1650 × Kth ≈ 187 °C, et l'énergie
   est 3,6 fois celle de M1.
3. Le thermostat tout-ou-rien (M3) tient la bande 100 % du temps avec 4 %
   d'énergie en moins que M1. Il le paie par **27 allumages** en 2 h, contre
   10 pour M1, chacun précédé d'une purge. La commande par paliers de M1
   réduit les cycles de l'allumeur et des électrovannes.
4. M1 reste dans la bande 99,8 % du temps et n'en sort jamais au-delà de 57,5 °C.

---

## E3 — Bilan des pertes (cas C1, 10 h) (§6.6.5)

**Hypothèses** : postes 1 et 2 issus du modèle (bilan exact : chambre + parois
= énergie fournie, écart 0,0 %). Postes 3 à 5 = *estimations* sur les signaux
simulés :
- renouvellement d'air hors purges : ṁc_p × PWM_Ext/255 × (T_sec − T_amb) ;
- purges : débit maximal d'extraction (200 m³/h), le débit de purge n'étant
  pas connu ;
- évaporation : 9,33 kg × 2,37 MJ/kg pour un cycle complet.

**Phases de purge** : en veille (palier 0 %), le programme reste en phase
PURGE avec la ventilation de purge au maximum. Ces phases (purges, veilles et
post-purge) durent **223 min sur les 10 h**.

| Poste | Énergie (kWh) | Part de l'énergie fournie |
|---|---|---|
| Échauffement de la chambre | 0,16 | 8 % |
| Pertes par les parois | 1,90 | 92 % |
| Renouvellement d'air, 80 m³/h (estimation) | 1,34 | 65 % |
| Renouvellement d'air, 200 m³/h (estimation) | 3,35 | 163 % |
| Air chaud évacué pendant les purges et veilles (estimation) | 5,27 | 256 % |
| Évaporation, cycle complet (estimation) | 6,14 | 299 % |
| Énergie fournie dans le modèle (gaz + solaire) | 2,06 | 100 % |

**Observations**
1. Le modèle fournit en moyenne **206 W** sur la journée : 1,90 kWh partent
   par les parois et 0,16 kWh restent dans la chambre (T_sec finale 41 °C).
2. Les postes que le modèle ne représente pas sont plus grands que l'énergie
   fournie : 6,1 kWh pour l'évaporation d'un cycle complet, 5,3 kWh d'air
   chaud évacué pendant les purges et les veilles.
3. S'ils étaient à la charge du brûleur, il faudrait 14,8 kWh (80 m³/h) à
   16,8 kWh (200 m³/h) sur 10 h, soit **1480 à 1680 W en moyenne**, 7 à 8 fois
   la puissance du modèle. Cela reste dans la capacité du brûleur (5000 W), et
   au-dessus du palier 33 % (1650 W) dans le cas à 200 m³/h.
4. L'air évacué pendant les veilles est le premier poste « évitable ». La
   ventilation de purge maintenue pendant toute la veille est une piste
   d'économie, à discuter avec la sécurité ATEX.

---

## E4 — Sensibilité aux paramètres et perturbations (§6.6.6)

**Hypothèses** : scénario 1 **prolongé à 2 h**, pour avoir au moins trois
cycles de veille dans chaque variante. Régime établi mesuré à partir de la
première entrée en veille (597 s dans le cas nominal) ; sans veille, sur la
seconde heure. Bruit uniforme, une valeur par seconde, avec quantification à
0,0625 °C ; suite pseudo-aléatoire fixe (`bruit_mesure.m`).

| Paramètre | Valeur | Amplitude (°C) | Période (min) | Rallumages/h | P̄ (W) | Théorie (1er ordre) |
|---|---|---|---|---|---|---|
| UA | 7,1 W/K (−30 %) | 5,0 | 16,2 | 3,3 | 189 | 16,1 min ; 214 W |
| UA | 10,2 W/K (nominal) | 5,0 | 12,1 | 4,9 | 302 | t_on 134 s, t_off 590 s → 12,1 min ; 306 W |
| UA | 13,3 W/K (+30 %) | 5,0 | 10,0 | 6,0 | 397 | 10,0 min ; 397 W |
| UA + air | 34 W/K (80 m³/h) | 5,0 | 7,8 | 8,0 | 1001 | 7,7 min ; 1019 W |
| UA + air | 69 W/K (200 m³/h) | — | — | 0 | 1650 | T∞(33 %) = 48,8 °C < 57,5 °C |
| T_amb | 15 °C | 5,0 | 9,8 | 6,2 | 401 | 9,8 min ; 408 W |
| T_amb | 35 °C (réglages par défaut) | — | — | 0 | 0 | passage en solaire |
| T_amb | 35 °C (Marge_Sol = 20 °C) | 5,0 | 16,9 | 3,2 | 183 | — |
| C_eq | × 2 | 5,0 | 24,1 | 2,3 | 289 | 24,1 min ; 306 W |
| Retard de mesure | 10 s | 5,5 | 13,1 | 4,4 | 292 | dépassement 0,37 °C |
| Retard de mesure | 30 s | 6,4 | 15,2 | 3,8 | 299 | 1,10 °C |
| Retard de mesure | 60 s | 7,7 | 18,2 | 3,2 | 309 | 2,20 °C |
| Bruit | ± 0,25 °C | 4,9 | 11,5 | 4,9 | 287 | — |
| Bruit | ± 0,5 °C | 4,8 | 10,4 | 5,5 | 291 | — |

**Observations**
1. **Accord avec les formules du mémoire.** La période simulée (12,1 min)
   est égale à t_on + t_off théoriques (134 + 590 s = 12,1 min). L'accord est
   à 1 % près pour les variantes de UA, T_amb et C_eq (P̄ : 302 W simulés,
   306 W théoriques). L'amplitude reste égale à Hhyst = 5 °C tant qu'il
   n'y a ni retard ni bruit.
2. **Retard de mesure.** Le dépassement haut simulé vaut 0,40 / 1,13 / 2,22 °C
   pour 10 / 30 / 60 s, contre pente × retard = 0,0366 K/s × retard =
   0,37 / 1,10 / 2,20 °C. Le dépassement bas reste petit (0,08 à 0,46 °C), car
   la pente de refroidissement est 4,7 fois plus faible. À 60 s, T_sec monte à
   59,7 °C et l'amplitude passe de 5,0 à 7,7 °C.
3. **Renouvellement d'air élevé (69 W/K).** Le palier 33 % ne suffit plus
   (T∞ = 48,8 °C). La commande par paliers **ne remonte pas à 67 %** : les
   paliers ne font que descendre (100 → 67 → 33 %). T_sec **plafonne à
   48,8 °C**, sous la bande, sans alarme.
   - À 34 W/K (80 m³/h), la régulation fonctionne encore, avec une chauffe
     plus longue que la veille (t_on 286 s > t_off 177 s) et 8 rallumages par
     heure.
4. **T_amb = 35 °C avec les réglages par défaut.** T_cap estimée = 55 °C =
   seuil ON : le programme démarre en solaire même sans soleil (P_sol = 0 dans
   ce cas), et T_sec reste à 35 °C. C'est la même limite de l'estimation que
   dans E1.
5. **Bruit de mesure.** À ± 0,5 °C, les basculements surviennent plus tôt :
   l'amplitude passe à 4,8 °C et les rallumages à 5,5/h au lieu de 4,9. La
   puissance moyenne ne change presque pas (291 W contre 302 W).
6. **Ouverture de porte** (UA × 10 de 1800 à 1860 s, pendant une veille) :
   - T_sec passe de 53,4 à 51,0 °C (chute de 2,4 °C) ;
   - T_sec franchit 52,5 °C et la commande rallume à 33 % à **1811 s** (11 s
     après l'ouverture) ;
   - T_sec revient dans la bande à **1899 s**, 99 s après l'ouverture ;
   - seuls les paliers 0 et 33 % sont utilisés.
