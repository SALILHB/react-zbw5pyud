#!/usr/bin/env python3
"""schemas_chart.py — vues lisibles de trois parties du chart Stateflow (mémoire V4, A2 à A4)

    python3 schemas_chart.py [dossier_sortie]

Une capture d'écran du chart ne permet pas de lire les conditions : elles se
superposent. Ce script redessine chaque partie avec ses états et ses liens,
et liste sous le schéma chaque transition avec sa condition EXACTE, recopiée
de matlab/construire_modele.m (ajouterTransitions, ajouterTransitionsCombustion).
Les actions {...} sont omises ; leur effet utile est indiqué entre parenthèses.
À tenir à jour si les transitions du modèle changent.

Sorties : chart_mode_h2.png, chart_region_phase.png, chart_urgence.png (300 dpi).
"""

import os
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyArrowPatch, FancyBboxPatch

FOND = "#f6efe0"
ETAT = "#efe3c8"
CONTENEUR = "#f3e9d2"
BORD = "#3a3a3a"
FLECHE = "#33449a"
EXTERNE = "#e4e4e4"
CM = 1 / 2.54
LIGNE_CM = 0.40   # hauteur d'une ligne de la liste des transitions


def etat(ax, x, y, w, h, nom, fond=ETAT, taille=8.5, tirets=False, bord=BORD):
    ax.add_patch(FancyBboxPatch((x, y), w, h, boxstyle="round,pad=0,rounding_size=1.6",
                                fc=fond, ec=bord, lw=1.1, ls="--" if tirets else "-"))
    ax.text(x + 1.2, y + h - 1.0, nom, ha="left", va="top", fontsize=taille,
            fontweight="bold", color="#111111")


def fleche(ax, p, q, lettre=None, rad=0.0, pos=0.5, decal=(0, 0)):
    ax.add_patch(FancyArrowPatch(p, q, connectionstyle=f"arc3,rad={rad}", arrowstyle="-|>",
                                 mutation_scale=11, lw=1.2, color=FLECHE, shrinkA=0, shrinkB=0))
    if lettre:
        # point de la courbe de Bézier quadratique tracée par arc3
        mx, my = (p[0] + q[0]) / 2, (p[1] + q[1]) / 2
        dx, dy = q[0] - p[0], q[1] - p[1]
        cx, cy = mx + rad * dy, my - rad * dx
        t = pos
        x = (1 - t) ** 2 * p[0] + 2 * (1 - t) * t * cx + t ** 2 * q[0]
        y = (1 - t) ** 2 * p[1] + 2 * (1 - t) * t * cy + t ** 2 * q[1]
        pastille(ax, x + decal[0], y + decal[1], lettre)


def pastille(ax, x, y, lettre):
    ax.text(x, y, lettre, ha="center", va="center", fontsize=8, fontweight="bold", color=FLECHE,
            bbox=dict(boxstyle="circle,pad=0.18", fc="white", ec=FLECHE, lw=0.9))


def defaut(ax, x, y, long=3.0):
    ax.plot([x], [y + long + 0.2], "o", ms=5, color=FLECHE)
    fleche(ax, (x, y + long), (x, y))


def hauteur_liste(lignes):
    return sum(2 + c.count("\n") for _, _, c in lignes) + 0.5 * len(lignes) + 0.2


def liste(fig, lignes, haut):
    """Liste des transitions : lettre et liaison, puis condition exacte en dessous."""
    ax = fig.add_axes([0.02, 0.0, 0.96, haut])
    ax.set_axis_off()
    ax.set_xlim(0, 1)
    ax.set_ylim(hauteur_liste(lignes), 0)
    y = 0.2
    for lettre, liaison, condition in lignes:
        pastille(ax, 0.012, y + 0.5, lettre)
        ax.text(0.035, y + 0.5, liaison, ha="left", va="center", fontsize=7.8, fontweight="bold")
        morceaux = condition.split("\n")
        for k, morceau in enumerate(morceaux):
            ax.text(0.035, y + 1.5 + k, morceau, ha="left", va="center", fontsize=7.4,
                    family="DejaVu Sans Mono")
        y += 1 + len(morceaux) + 0.5


def figure_complete(schema_cm, lignes, xlim, ylim, largeur_cm=17):
    """Schéma en haut (schema_cm de haut), liste des transitions en dessous."""
    liste_cm = hauteur_liste(lignes) * LIGNE_CM
    total = schema_cm + liste_cm
    fig = plt.figure(figsize=(largeur_cm * CM, total * CM), dpi=300)
    fig.patch.set_facecolor("white")
    ax = fig.add_axes([0.01, (liste_cm + 0.1) / total, 0.98, (schema_cm - 0.2) / total])
    ax.set_facecolor(FOND)
    ax.set_xlim(*xlim)
    ax.set_ylim(*ylim)
    ax.set_xticks([])
    ax.set_yticks([])
    for sp in ax.spines.values():
        sp.set_color("#bbbbbb")
    liste(fig, lignes, liste_cm / total)
    return fig, ax


def enregistrer(fig, sortie, nom):
    chemin = os.path.join(sortie, nom)
    fig.savefig(chemin, dpi=300, facecolor="white")
    plt.close(fig)
    print("  [ok]", chemin)


# ---------------------------------------------------------------------------
def mode_h2(sortie):
    lignes = [
        ("a", "PURGE → ALLUMAGE", "[after(Temps_Purge, sec) && raison_purge ~= 3 && palier ~= 3]"),
        ("b", "PURGE → ALLUMAGE   (fin de veille : rallumage au palier 33 %)",
         "[after(Temps_Purge, sec) && (raison_purge == 3 || palier == 3)\n"
         " && T_sec < T3_seuil - Hhyst/2]"),
        ("c", "ALLUMAGE → REGULATION", "[Flame==1]"),
        ("d", "ALLUMAGE → PURGE   (échec d'allumage : nouvel essai après purge)",
         "[after(Temps_Allumage, sec) && Flame==0 && nb_echecs_allumage + 1 < MAX_ECHECS]"),
        ("e", "ALLUMAGE → ERREUR_COMBUSTION   (échecs répétés)",
         "[after(Temps_Allumage, sec) && Flame==0 && nb_echecs_allumage + 1 >= MAX_ECHECS]"),
        ("f", "REGULATION → PURGE   (perte de flamme : gaz fermé, relance)",
         "[Flame==0 && nb_pertes_flamme < MAX_PERTES_FLAMME]"),
        ("g", "REGULATION → URGENCE_ATEX   (pertes de flamme répétées : cause 4)",
         "[Flame==0 && nb_pertes_flamme >= MAX_PERTES_FLAMME]"),
        ("h", "REGULATION → PURGE   (consigne atteinte : veille à 0 %)", "[palier==3]"),
    ]
    fig, ax = figure_complete(8.0, lignes, (0, 100), (-5, 58))
    etat(ax, 3, 8, 94, 40, "MODE_H2   (MODE_GPL identique, avec V_But)", fond=CONTENEUR)
    etat(ax, 7, 24, 22, 16, "PURGE")
    etat(ax, 39, 24, 22, 16, "ALLUMAGE")
    etat(ax, 71, 24, 22, 16, "REGULATION")
    defaut(ax, 18, 40)
    etat(ax, 34, 51, 32, 6, "ERREUR_COMBUSTION", fond=EXTERNE, taille=8)
    etat(ax, 71, -4, 26, 6.5, "URGENCE_ATEX", fond=EXTERNE, taille=8)
    fleche(ax, (29, 35), (39, 35), "a", decal=(0, 2.0))
    fleche(ax, (29, 29), (39, 29), "b", decal=(0, 2.0))
    fleche(ax, (61, 32), (71, 32), "c", decal=(0, 2.0))
    fleche(ax, (45, 24), (23, 24), "d", rad=-0.45, pos=0.5)
    fleche(ax, (56, 40), (56, 51), "e", pos=0.45, decal=(2.4, 0))
    fleche(ax, (80, 24), (14, 24), "f", rad=-0.24, pos=0.62)
    fleche(ax, (86, 24), (9, 24), "h", rad=-0.30, pos=0.30)
    fleche(ax, (91, 24), (91, 2.5), "g", pos=0.4, decal=(2.4, 0))
    enregistrer(fig, sortie, "chart_mode_h2.png")


def region_phase(sortie):
    lignes = [
        ("a", "DEMANDE_PROLONGATION, PROLONGATION → NORMAL   (prioritaire)",
         "[in(EN_CYCLE.SOURCE.ERREUR_COMBUSTION)]"),
        ("b", "NORMAL → DEMANDE_PROLONGATION   (motif : humidité atteinte)",
         "[~in(ERREUR_COMBUSTION) && t_cycle >= Temps_Min_Fin && H_sec_e <= H_fin]"),
        ("c", "NORMAL → DEMANDE_PROLONGATION   (motif : durée maximale)",
         "[~in(ERREUR_COMBUSTION) && t_cycle >= Duree_Max_Cycle]"),
        ("d", "DEMANDE_PROLONGATION → PROLONGATION", "[Btn_OK==1]"),
        ("e", "DEMANDE_PROLONGATION → SECHAGE_TERMINE", "[Btn_Stop==1]"),
        ("f", "DEMANDE_PROLONGATION → SECHAGE_TERMINE   (pas de réponse de l'opérateur)",
         "[after(Temps_Reponse, sec)]"),
        ("g", "DEMANDE_PROLONGATION → elle-même   (durée de prolongation ± 5 min)",
         "[Btn_UP==1 && Btn_UP_prev==0]   et   [Btn_DOWN==1 && Btn_DOWN_prev==0]"),
        ("h", "PROLONGATION → DEMANDE_PROLONGATION   (humidité atteinte pendant la prolongation)",
         "[humidite_deja_atteinte==0 && t_cycle >= Temps_Min_Fin && H_sec_e <= H_fin]"),
        ("i", "PROLONGATION → DEMANDE_PROLONGATION   (prolongation écoulée)",
         "[after(duree_prolongation, sec)]"),
    ]
    fig, ax = figure_complete(9.0, lignes, (0, 100), (0, 64))
    etat(ax, 3, 2, 72, 60, "PHASE   (région parallèle de EN_CYCLE)", fond=CONTENEUR, tirets=True)
    etat(ax, 22, 44, 32, 9, "NORMAL")
    etat(ax, 22, 24, 32, 11, "DEMANDE_PROLONGATION", taille=8)
    etat(ax, 22, 5, 32, 10, "PROLONGATION")
    defaut(ax, 38, 53, long=2.6)
    etat(ax, 80, 24, 18, 11, "SECHAGE_\nTERMINE", fond=EXTERNE, taille=8)
    fleche(ax, (31, 44), (31, 35), "b", decal=(-2.3, 0))
    fleche(ax, (38, 44), (38, 35), "c", decal=(-2.3, 0))
    fleche(ax, (46, 35), (46, 44), "a", decal=(2.3, 0))
    fleche(ax, (30, 24), (30, 15), "d", decal=(-2.3, 0))
    fleche(ax, (38, 15), (38, 24), "h", decal=(-2.3, 0))
    fleche(ax, (45, 15), (45, 24), "i", decal=(2.3, 0))
    fleche(ax, (54, 9), (54, 48.5), "a", rad=0.55, pos=0.22, decal=(1.0, 0))
    fleche(ax, (54, 32), (80, 32), "e", pos=0.72, decal=(0, 1.9))
    fleche(ax, (54, 27), (80, 27), "f", pos=0.72, decal=(0, -1.9))
    fleche(ax, (22, 32), (22, 27), "g", rad=1.2, pos=0.5)
    enregistrer(fig, sortie, "chart_region_phase.png")


def urgence(sortie):
    lignes = [
        ("a", "FONCTIONNEMENT_NORMAL → URGENCE_ATEX   (fuite H2 : cause 1 ; fuite GPL : 2 ; arrêt d'urgence : 3)",
         "[MQ8_H2 >= Seuil_MQ8 || MQ6_But >= Seuil_MQ6 || AU_Manuel == 1]"),
        ("b", "FONCTIONNEMENT_NORMAL → URGENCE_ATEX   (flamme vue gaz fermé : cause 5)",
         "[duration(Flame==1 && gaz_ferme==1) >= DELAI_FLAMME_PARASITE]"),
        ("c", "REGULATION → URGENCE_ATEX   (pertes de flamme répétées : cause 4)",
         "[Flame==0 && nb_pertes_flamme >= MAX_PERTES_FLAMME]"),
        ("d", "URGENCE_ATEX → ATTENTE_DEMARRAGE   (réarmement, toutes les causes disparues)",
         "[(Btn_OK==1 || Btn_Rearm==1) && MQ8_H2 < Seuil_MQ8 && MQ6_But < Seuil_MQ6\n"
         " && AU_Manuel==0 && Flame==0]"),
    ]
    fig, ax = figure_complete(8.0, lignes, (0, 100), (0, 54))
    etat(ax, 3, 22, 94, 30, "FONCTIONNEMENT_NORMAL", fond=CONTENEUR)
    etat(ax, 7, 30, 22, 13, "ATTENTE_\nDEMARRAGE", taille=8)
    etat(ax, 35, 25, 58, 22, "EN_CYCLE")
    etat(ax, 62, 28, 28, 11, "REGULATION\n(MODE_H2, MODE_GPL)", fond="#e9dcbd", taille=7.5)
    etat(ax, 30, 2, 40, 12, "URGENCE_ATEX", fond="#f2d6d0", bord="#a03030")
    defaut(ax, 18, 43)
    fleche(ax, (40, 22), (40, 14), "a", decal=(-2.3, 0))
    fleche(ax, (50, 22), (50, 14), "b", decal=(-2.3, 0))
    fleche(ax, (76, 28), (66, 14), "c", rad=-0.15, pos=0.55, decal=(2.5, 0))
    fleche(ax, (30, 8), (16, 30), "d", rad=-0.35, pos=0.5)
    enregistrer(fig, sortie, "chart_urgence.png")


if __name__ == "__main__":
    dossier = sys.argv[1] if len(sys.argv) > 1 else "."
    os.makedirs(dossier, exist_ok=True)
    mode_h2(dossier)
    region_phase(dossier)
    urgence(dossier)
