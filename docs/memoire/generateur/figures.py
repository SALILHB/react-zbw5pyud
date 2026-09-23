import os
import csv
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, FancyArrowPatch

OUT = os.path.dirname(os.path.abspath(__file__)) + "/"
BLEU, VERT, ORANGE, ROUGE, GRIS, VIOLET = "#2b6cb0", "#2f855a", "#c05621", "#c53030", "#4a5568", "#6b46c1"
plt.rcParams.update({"font.family": "DejaVu Sans", "font.size": 10})


def boite(ax, x, y, w, h, texte, couleur, fond=None, taille=9, gras=False, style="round,pad=0.02,rounding_size=0.08", ls="-"):
    ax.add_patch(FancyBboxPatch((x, y), w, h, boxstyle=style, linewidth=1.4,
                                edgecolor=couleur, facecolor=fond or "white", linestyle=ls))
    ax.text(x + w / 2, y + h / 2, texte, ha="center", va="center", fontsize=taille,
            color="#1a202c", fontweight="bold" if gras else "normal", wrap=True)


def fleche(ax, x1, y1, x2, y2, couleur=GRIS, texte=None, rad=0.0, taille=8):
    ax.add_patch(FancyArrowPatch((x1, y1), (x2, y2), arrowstyle="-|>", mutation_scale=12,
                                 color=couleur, linewidth=1.3, connectionstyle=f"arc3,rad={rad}"))
    if texte:
        ax.text((x1 + x2) / 2, (y1 + y2) / 2 + 0.12, texte, ha="center", va="bottom", fontsize=taille, color=couleur)


# ---------------------------------------------------------------- Figure 1
def fig_architecture():
    fig, ax = plt.subplots(figsize=(10, 5.6))
    ax.set_xlim(0, 10); ax.set_ylim(0, 5.6); ax.axis("off")
    boite(ax, 0.2, 3.7, 2.5, 1.6, "CAPTEURS\nT_sec, T_cap (DS18B20)\nT_amb, H_amb, H_sec (DHT22)\nPress_H2 (4-20 mA)", BLEU, "#ebf4ff", 8.5)
    boite(ax, 0.2, 1.9, 2.5, 1.5, "SÉCURITÉ\nFlamme (UV/IR)\nMQ8 (H2), MQ6 (GPL)\nArrêt d'urgence", ROUGE, "#fff5f5", 8.5)
    boite(ax, 0.2, 0.2, 2.5, 1.4, "OPÉRATEUR\nMENU, UP, DOWN, OK\nSTART, STOP, RÉARM.", VIOLET, "#faf5ff", 8.5)
    boite(ax, 3.6, 0.6, 2.8, 4.4, "ARDUINO MEGA 2560\n\nlireEntrees()\n↓\npasFSM()\n(machine à états)\n↓\necrireSorties()\n↓\ngererIHM()\n\nConfig en EEPROM", GRIS, "#f7fafc", 9, True)
    boite(ax, 7.2, 3.7, 2.6, 1.6, "GAZ\nV_H2, V_But (source)\nEV1, EV2, EV3 (paliers)\nSpark (allumage)", ORANGE, "#fffaf0", 8.5)
    boite(ax, 7.2, 1.9, 2.6, 1.5, "VENTILATION (PWM)\nPurge, Distribution,\nExtraction", VERT, "#f0fff4", 8.5)
    boite(ax, 7.2, 0.2, 2.6, 1.4, "IHM\nLCD 20×4 I2C\nBuzzer", VIOLET, "#faf5ff", 8.5)
    for y in (4.5, 2.65, 0.9):
        fleche(ax, 2.75, y, 3.55, y if y != 4.5 else 4.2)
    for y in (4.5, 2.65, 0.9):
        fleche(ax, 6.45, y if y != 4.5 else 4.2, 7.15, y)
    ax.text(5.0, 5.35, "Architecture de la commande", ha="center", fontsize=11, fontweight="bold")
    fig.savefig(OUT + "fig1_architecture.png", dpi=200, bbox_inches="tight"); plt.close(fig)


# ---------------------------------------------------------------- Figure 2
def fig_hierarchie():
    fig, ax = plt.subplots(figsize=(10, 7.2))
    ax.set_xlim(0, 10); ax.set_ylim(0, 7.2); ax.axis("off")
    boite(ax, 0.1, 1.1, 9.8, 6.0, "", BLEU, "#f7fbff")
    ax.text(0.3, 6.85, "FONCTIONNEMENT_NORMAL (OR)", fontsize=9.5, fontweight="bold", color=BLEU)
    boite(ax, 0.4, 6.0, 2.0, 0.6, "ATTENTE_DEMARRAGE", GRIS, "white", 8)
    boite(ax, 7.6, 6.0, 2.0, 0.6, "SECHAGE_TERMINE", GRIS, "white", 8)
    boite(ax, 0.3, 1.3, 9.4, 4.5, "", GRIS, "#ffffff", ls="--")
    ax.text(0.5, 5.55, "EN_CYCLE (AND : deux régions en parallèle)", fontsize=9, fontweight="bold", color=GRIS)
    boite(ax, 0.5, 1.45, 6.1, 3.95, "", ORANGE, "#fffaf0", ls="--")
    ax.text(0.65, 5.15, "Région 1 — SOURCE (quelle énergie ?)", fontsize=8.5, fontweight="bold", color=ORANGE)
    boite(ax, 0.7, 4.25, 1.8, 0.65, "MODE_SOLAIRE", VERT, "white", 8)
    boite(ax, 2.7, 4.25, 3.7, 0.65, "ERREUR_COMBUSTION", ROUGE, "white", 8)
    for i, nom in enumerate(("MODE_H2", "MODE_GPL")):
        y0 = 2.85 - i * 1.3
        boite(ax, 0.7, y0, 5.7, 1.15, "", ORANGE, "white")
        ax.text(0.85, y0 + 0.9, nom, fontsize=8, fontweight="bold", color=ORANGE)
        for j, sous in enumerate(("PURGE", "ALLUMAGE", "REGULATION")):
            boite(ax, 1.0 + j * 1.8, y0 + 0.12, 1.55, 0.6, sous, GRIS, "#f7fafc", 7.5)
            if j < 2:
                fleche(ax, 1.0 + j * 1.8 + 1.55, y0 + 0.42, 1.0 + (j + 1) * 1.8, y0 + 0.42)
    boite(ax, 6.8, 1.45, 2.8, 3.95, "", VIOLET, "#faf5ff", ls="--")
    ax.text(6.95, 5.15, "Région 2 — PHASE (le cycle)", fontsize=8.2, fontweight="bold", color=VIOLET)
    boite(ax, 7.1, 4.2, 2.2, 0.6, "NORMAL", VIOLET, "white", 8)
    boite(ax, 7.1, 2.9, 2.2, 0.75, "DEMANDE_\nPROLONGATION", VIOLET, "white", 7.5)
    boite(ax, 7.1, 1.65, 2.2, 0.65, "PROLONGATION", VIOLET, "white", 8)
    fleche(ax, 8.2, 4.2, 8.2, 3.65, VIOLET)
    fleche(ax, 7.9, 2.9, 7.9, 2.3, VIOLET, rad=0.0)
    fleche(ax, 8.5, 2.3, 8.5, 2.9, VIOLET, rad=0.0)
    boite(ax, 3.5, 0.1, 3.0, 0.75, "URGENCE_ATEX\n(prioritaire, depuis tout état)", ROUGE, "#fff5f5", 8)
    fleche(ax, 5.0, 1.1, 5.0, 0.87, ROUGE)
    fig.savefig(OUT + "fig2_hierarchie.png", dpi=200, bbox_inches="tight"); plt.close(fig)


# ---------------------------------------------------------------- Figure 3
def fig_hysteresis():
    T1, T2, T3, d = 35, 45, 55, 2
    fig, ax = plt.subplots(figsize=(9, 4.6))
    niveaux = {0: 100, 1: 67, 2: 33, 3: 0}
    # montée (la puissance baisse)
    mont_x = [25, T1 + d, T1 + d, T2 + d, T2 + d, T3 + d, T3 + d, 62]
    mont_y = [100, 100, 67, 67, 33, 33, 0, 0]
    desc_x = [62, T3 - d, T3 - d, T2 - d, T2 - d, T1 - d, T1 - d, 25]
    desc_y = [0, 0, 33, 33, 67, 67, 100, 100]
    ax.plot(mont_x, mont_y, color=ROUGE, lw=2.2, label="T_sec monte → la puissance baisse")
    ax.plot(desc_x, desc_y, color=BLEU, lw=2.2, ls="--", label="T_sec descend → la puissance remonte")
    for T, nom in ((T1, "T1"), (T2, "T2"), (T3, "T3 = T_cible")):
        ax.axvline(T, color=GRIS, lw=0.8, ls=":")
        ax.text(T, 106, nom, ha="center", fontsize=9, color=GRIS)
        ax.axvspan(T - d, T + d, color="#edf2f7", zorder=0)
    ax.set_xlabel("T_sec (°C)")
    ax.set_ylabel("Palier de puissance (%)")
    ax.set_yticks([0, 33, 67, 100])
    ax.set_ylim(-8, 114); ax.set_xlim(25, 62)
    ax.legend(loc="lower left", fontsize=8.5, frameon=False)
    ax.text(26, 50, "Bandes grises :\nzones mortes ±Hhyst/2\n(Hhyst = 4 °C)", fontsize=8, color=GRIS)
    ax.spines[["top", "right"]].set_visible(False)
    fig.savefig(OUT + "fig3_hysteresis.png", dpi=200, bbox_inches="tight"); plt.close(fig)


# ---------------------------------------------------------------- Figure 4
def fig_cycle():
    t, T, p, e, h, hf = [], [], [], [], [], []
    with open(os.path.join(OUT, "..", "..", "..", "tests", "cycle.csv")) as f:
        for r in csv.DictReader(f):
            t.append(float(r["t_min"])); T.append(float(r["T_sec"])); p.append(int(r["palier_pct"]))
            e.append(r["etat"]); h.append(float(r["H_sec"])); hf.append(float(r["H_fin"]))
    fig, (a1, a2, a3) = plt.subplots(3, 1, figsize=(10, 7.4), sharex=True,
                                     gridspec_kw={"height_ratios": [2.2, 1.2, 1.4]})
    a1.plot(t, T, color=ROUGE, lw=1.8, label="T_sec")
    for val, nom in ((35, "T1"), (45, "T2"), (55, "T3 = T_cible")):
        a1.axhline(val, color=GRIS, lw=0.8, ls=":")
        a1.text(149, val + 0.6, nom, ha="right", fontsize=8, color=GRIS)
    a1.set_ylabel("T_sec (°C)"); a1.legend(loc="lower right", frameon=False, fontsize=8)
    a2.step(t, p, where="post", color=ORANGE, lw=1.6)
    a2.set_ylabel("Palier (%)"); a2.set_yticks([0, 33, 67, 100])
    a3.plot(t, h, color=BLEU, lw=1.8, label="H_sec (air extrait)")
    a3.plot(t, hf, color=VERT, lw=1.2, ls="--", label="H_fin = H_produit + H_amb")
    a3.axvline(60, color=GRIS, lw=0.8, ls=":")
    a3.text(60.5, 80, "Temps_Min_Fin\n(60 min)", fontsize=7.5, color=GRIS)
    a3.set_ylabel("Humidité (%)"); a3.set_xlabel("Temps (min)")
    a3.legend(loc="upper right", frameon=False, fontsize=8)
    # bandes d'état
    couleurs = {"H2": "#fffaf0", "DEMANDE": "#faf5ff", "TERMINE": "#f0fff4", "ATTENTE": "#f7fafc"}
    debut = 0
    for i in range(1, len(e) + 1):
        if i == len(e) or e[i] != e[debut]:
            for a in (a1, a2, a3):
                a.axvspan(t[debut], t[i - 1], color=couleurs.get(e[debut], "white"), zorder=0)
            court = (t[i - 1] - t[debut]) < 10
            y_lab = 66 if not court else (66 if e[debut] == "DEMANDE" else 62.5)
            x_lab = (t[debut] + t[i - 1]) / 2 + (0 if not court else (-3 if e[debut] == "DEMANDE" else 4))
            a1.text(x_lab, y_lab, e[debut], ha="center", fontsize=7.5, color=GRIS)
            debut = i
    for a in (a1, a2, a3):
        a.spines[["top", "right"]].set_visible(False)
    a1.set_ylim(20, 70); a1.set_xlim(0, 150)
    fig.tight_layout()
    fig.savefig(OUT + "fig4_cycle.png", dpi=200, bbox_inches="tight"); plt.close(fig)


# ---------------------------------------------------------------- Figure 5
def fig_boucle():
    fig, ax = plt.subplots(figsize=(9.5, 3.4))
    ax.set_xlim(0, 10); ax.set_ylim(0.1, 2.8); ax.axis("off")
    etapes = [("lireEntrees()", "capteurs, boutons,\nvaleurs FIXE, T_cap, H_fin", BLEU),
              ("pasFSM()", "1 case par état\n+ transitions prioritaires", ORANGE),
              ("ecrireSorties()", "seul accès aux\nactionneurs (verrous)", VERT),
              ("gererIHM()", "menu, écran LCD,\nréarmement", VIOLET)]
    for i, (titre, detail, c) in enumerate(etapes):
        x = 0.2 + i * 2.45
        boite(ax, x, 1.2, 2.05, 1.45, f"{titre}\n\n{detail}", c, "white", 8.2)
        if i < 3:
            fleche(ax, x + 2.07, 1.92, x + 2.43, 1.92, c)
    ax.plot([8.4, 8.4, 1.2], [1.18, 0.75, 0.75], color=GRIS, lw=1.3)
    fleche(ax, 1.2, 0.75, 1.2, 1.17, GRIS)
    ax.text(4.8, 0.35, "loop() : recommence en permanence (quelques ms par tour, sans aucun delay())",
            ha="center", fontsize=8.5, color=GRIS)
    fig.savefig(OUT + "fig5_boucle.png", dpi=200, bbox_inches="tight"); plt.close(fig)


# ---------------------------------------------------------------- Figure 6
def fig_chaine():
    fig, ax = plt.subplots(figsize=(10, 3.6))
    ax.set_xlim(0, 10); ax.set_ylim(0.2, 3.1); ax.axis("off")
    blocs = [("Cahier des charges\n+ décisions\nopérateur", GRIS),
             ("Spécification FSM\n(états, transitions,\npriorités)", VIOLET),
             ("Firmware Arduino\nsechoir_hybride.ino\n(source de vérité)", ORANGE),
             ("Banc de tests natif\n200 vérifications\n+ mutations", VERT),
             ("Modèle Stateflow\n+ simulation\nSimulink", BLEU)]
    for i, (txt, c) in enumerate(blocs):
        x = 0.1 + i * 2.0
        boite(ax, x, 1.6, 1.75, 1.35, txt, c, "white", 8)
        if i < 4:
            fleche(ax, x + 1.77, 2.27, x + 1.98, 2.27, c)
    ax.plot([8.97, 8.97, 4.97], [1.58, 1.05, 1.05], color=ROUGE, lw=1.2)
    fleche(ax, 4.97, 1.05, 4.97, 1.57, ROUGE)
    ax.text(7.0, 0.45, "En cas d'écart : le firmware fait foi,\nle modèle et le mémoire s'alignent sur lui",
            ha="center", fontsize=8.5, color=ROUGE)
    fig.savefig(OUT + "fig6_chaine.png", dpi=200, bbox_inches="tight"); plt.close(fig)


fig_architecture(); fig_hierarchie(); fig_hysteresis(); fig_cycle(); fig_boucle(); fig_chaine()
print("ok")
