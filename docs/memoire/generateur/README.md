# Générateur du document « Commande de température du séchoir solaire hybride »

Le fichier Word `../Commande_Sechoir_Solaire_Hybride.docx` est produit par ces sources.

```bash
make -C ../../../tests simulation      # simulation du cycle -> tests/cycle.csv
python3 figures.py                     # figures fig1..fig6 (matplotlib)
npm install && npm run build           # document Word (docx-js)
```

- `build.js` : contenu du document (chapitres, tableaux, bibliographie).
- `lib.js` : mise en forme (titres, listes, code, encadrés, tableaux, figures).
- `figures.py` : figures ; la figure 4 est tracée à partir de `tests/cycle.csv`.

Le sommaire est un champ Word : il se remplit à l'ouverture (accepter la mise à
jour des champs) ou par clic droit → *Mettre à jour les champs*.
