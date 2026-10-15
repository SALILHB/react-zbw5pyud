// Petites fonctions de mise en forme pour le document (docx-js).
const fs = require("fs");
const {
  Paragraph, TextRun, HeadingLevel, AlignmentType, Table, TableRow, TableCell,
  WidthType, ShadingType, BorderStyle, ImageRun, PageBreak,
} = require("docx");

const LARGEUR = 9070;          // A4, marges 2,5 cm (DXA)
const DOSSIER = __dirname + "/";
let numFig = 0, numTab = 0;

function runs(texte, base = {}) {
  // **gras**, `code`, _italique_
  const morceaux = texte.split(/(\*\*[^*]+\*\*|`[^`]+`|(?<![A-Za-z0-9])_[^_]+?_(?![A-Za-z0-9]))/g).filter(s => s && s.length);
  return morceaux.map(m => {
    if (m.startsWith("**")) return new TextRun({ ...base, text: m.slice(2, -2), bold: true });
    if (m.startsWith("`")) return new TextRun({ ...base, text: m.slice(1, -1), font: "Consolas", size: 20, color: "1F3864" });
    if (/^_[^_]+_$/.test(m)) return new TextRun({ ...base, text: m.slice(1, -1), italics: true });
    return new TextRun({ ...base, text: m });
  });
}

const H1 = (t) => new Paragraph({ heading: HeadingLevel.HEADING_1, pageBreakBefore: true, children: [new TextRun(t)] });
const H2 = (t) => new Paragraph({ heading: HeadingLevel.HEADING_2, children: [new TextRun(t)] });
const H3 = (t) => new Paragraph({ heading: HeadingLevel.HEADING_3, children: [new TextRun(t)] });
const P = (t, opts = {}) => new Paragraph({ alignment: AlignmentType.JUSTIFIED, spacing: { after: 120, line: 276 }, ...opts, children: runs(t) });

let instanceNum = 0;
function L(items, niveau = 0, ref = "puces", instance = 0) {
  return items.map(it => new Paragraph({ numbering: { reference: ref, level: niveau, instance }, spacing: { after: 60, line: 264 }, alignment: AlignmentType.LEFT, children: runs(it) }));
}
const N = (items) => L(items, 0, "numeros", ++instanceNum);

function CODE(texte) {
  const lignes = texte.replace(/\n$/, "").split("\n");
  return lignes.map((l, i) => new Paragraph({
    shading: { type: ShadingType.CLEAR, fill: "F3F4F6", color: "auto" },
    spacing: { before: i === 0 ? 80 : 0, after: i === lignes.length - 1 ? 140 : 0, line: 240 },
    indent: { left: 200, right: 200 },
    children: [new TextRun({ text: l.length ? l : " ", font: "Consolas", size: 17, color: "1F2937" })],
  }));
}

function NOTE(titre, texte, couleur = "FFF7E6", bord = "C05621") {
  const b = { style: BorderStyle.SINGLE, size: 12, color: bord };
  return new Table({
    width: { size: LARGEUR, type: WidthType.DXA }, columnWidths: [LARGEUR],
    rows: [new TableRow({ children: [new TableCell({
      width: { size: LARGEUR, type: WidthType.DXA },
      shading: { type: ShadingType.CLEAR, fill: couleur, color: "auto" },
      borders: { left: b, top: { style: BorderStyle.NONE, size: 0, color: "FFFFFF" }, bottom: { style: BorderStyle.NONE, size: 0, color: "FFFFFF" }, right: { style: BorderStyle.NONE, size: 0, color: "FFFFFF" } },
      margins: { top: 100, bottom: 100, left: 180, right: 180 },
      children: [
        new Paragraph({ spacing: { after: 60 }, children: [new TextRun({ text: titre, bold: true, color: bord })] }),
        ...(Array.isArray(texte) ? texte : [texte]).map(t => new Paragraph({ alignment: AlignmentType.JUSTIFIED, spacing: { after: 60 }, children: runs(t) })),
      ],
    })] })],
  });
}

function TAB(legende, entetes, lignes, largeurs) {
  numTab++;
  const total = largeurs.reduce((a, b) => a + b, 0);
  const lg = largeurs.map(w => Math.round(w * LARGEUR / total));
  lg[lg.length - 1] += LARGEUR - lg.reduce((a, b) => a + b, 0);
  const bord = { style: BorderStyle.SINGLE, size: 4, color: "BFC7D5" };
  const bords = { top: bord, bottom: bord, left: bord, right: bord };
  const cellule = (txt, i, entete) => new TableCell({
    width: { size: lg[i], type: WidthType.DXA }, borders: bords,
    shading: entete ? { type: ShadingType.CLEAR, fill: "1F3864", color: "auto" } : undefined,
    margins: { top: 50, bottom: 50, left: 90, right: 90 },
    children: String(txt).split("\n").map(l => new Paragraph({ spacing: { after: 20 }, children: entete ? [new TextRun({ text: l, bold: true, color: "FFFFFF", size: 19 })] : runs(l, { size: 19 }) })),
  });
  return [
    new Table({
      width: { size: LARGEUR, type: WidthType.DXA }, columnWidths: lg,
      rows: [
        new TableRow({ tableHeader: true, cantSplit: true, children: entetes.map((e, i) => cellule(e, i, true)) }),
        ...lignes.map((l, r) => new TableRow({ cantSplit: true, children: l.map((c, i) => {
          const cell = cellule(c, i, false);
          return cell;
        }) })),
      ],
    }),
    new Paragraph({ alignment: AlignmentType.CENTER, spacing: { before: 60, after: 200 }, children: [new TextRun({ text: `Tableau ${numTab} : ${legende}`, italics: true, size: 19, color: "4A5568" })] }),
  ];
}

function FIG(fichier, legende, largeurPx = 600) {
  numFig++;
  const buf = fs.readFileSync(DOSSIER + fichier);
  const w = buf.readUInt32BE(16), h = buf.readUInt32BE(20);
  return [
    new Paragraph({ alignment: AlignmentType.CENTER, spacing: { before: 120 }, keepNext: true, children: [
      new ImageRun({ type: "png", data: buf, transformation: { width: largeurPx, height: Math.round(largeurPx * h / w) },
        altText: { title: legende, description: legende, name: fichier } }),
    ] }),
    new Paragraph({ alignment: AlignmentType.CENTER, spacing: { before: 60, after: 200 }, children: [new TextRun({ text: `Figure ${numFig} : ${legende}`, italics: true, size: 19, color: "4A5568" })] }),
  ];
}

const SAUT = () => new Paragraph({ children: [new PageBreak()] });

module.exports = { H1, H2, H3, P, L, N, CODE, NOTE, TAB, FIG, SAUT, runs, LARGEUR };
