# File formats

All Office Open XML files are ZIP packages handled with Qt's internal `QZipReader/QZipWriter` (hence `QT += gui-private`). Writers produce minimal, schema-ordered parts; readers are tolerant and skip what they do not understand. Every reader validates the ZIP signature first and refuses oversized XML parts (DOCX: 256 MB).

## Support matrix

| Format | MyWord | MyExcel | MyPowerPoint |
|---|---|---|---|
| `.docx` | read / write | – | – |
| `.xlsx` | – | read / write | – |
| `.pptx` | – | – | read / write |
| `.csv` | – | read / write (active sheet) | – |
| `.html .htm` | read / write | – | – |
| `.md .markdown` | read / write | – | – |
| `.txt` and other text files (`log json xml ini cfg …`) | read / write | – | – |
| `.odt` | export | – | – |
| `.pdf` | export | export | export |
| `.png` | – | – | export (current slide) |
| `.rtf .doc .xls .ppt .odt(import) .pdf(import)` | not supported — a clear message is shown | | |

## DOCX

**Written:** `[Content_Types].xml`, `_rels/.rels`, `word/document.xml`, `word/styles.xml` (Normal, Heading 1–3), `word/numbering.xml` (bullet and decimal lists, 9 levels), `word/_rels/document.xml.rels`.

| Feature | Read | Write |
|---|---|---|
| Paragraphs, line breaks, tabs | ✔ | ✔ |
| Run formatting: font, size, bold, italic, underline, strike, colour, highlight/shading | ✔ | ✔ |
| Alignment, indents, spacing, proportional line height | ✔ | ✔ |
| Headings (`Heading1..6` styles) | ✔ | ✔ (1–3) |
| Bullet / numbered lists with levels | ✔ | ✔ |
| Tables incl. column & row spans | ✔ | ✔ |
| Hyperlinks | ✔ (resolved through relationships) | text only |
| Page breaks (`pageBreakBefore`, `<w:br w:type="page">`) | ✔ | ✔ |
| Page size, orientation, margins | ✔ | ✔ |
| Images, headers/footers, comments, footnotes, style inheritance, fields | ✘ | ✘ |

## XLSX

**Written:** workbook, one worksheet part per sheet, `styles.xml` (deduplicated fonts, fills, `cellXfs`), inline strings (no shared-strings part), cached values for formulas.

| Feature | Read | Write |
|---|---|---|
| Multiple sheets and names | ✔ | ✔ |
| Numbers, strings (shared & inline), booleans, errors | ✔ | ✔ |
| Formulas (cached value used if the formula is shared) | ✔ | ✔ |
| Bold, italic, underline, font colour (RGB), solid fill (RGB) | ✔ | ✔ |
| Horizontal alignment | ✔ | ✔ |
| Number formats: number, percent, currency, date, text | ✔ (mapped by id / format code) | ✔ |
| Column widths, merged cells, conditional formats, charts, theme colours, shared formulas | ✘ | ✘ |

Number-format mapping on read (`mapNumFmt`): ids 9/10 or a code containing `%` → Percent; 49 → Text; 14–22, 45–47 or a code with `y` and `m` → Date; a code containing `$` → Currency; ids 1–4 → Number.

## PPTX

**Written:** presentation, one master + one layout + theme, `slideN.xml` per slide, media for images, and a private part for MyOffice data.

| Feature | Read | Write |
|---|---|---|
| Slides, size (scaled to 1280×720) | ✔ | ✔ |
| Text boxes and shapes: rectangle, rounded rectangle, ellipse | ✔ | ✔ |
| Lines | ✔ | ✔ |
| Pictures (PNG/JPEG…) | ✔ | ✔ |
| Position, size, rotation, fill, outline, line width | ✔ | ✔ |
| Text: first run's font size, bold, italic, underline, colour; paragraph alignment; vertical anchor | ✔ | ✔ |
| Slide background (solid) | ✔ | ✔ |
| Transitions | `fade push wipe cover zoom dissolve` mapped | written as standard `<p:transition>` |
| Placeholders without own geometry | default title/body positions | n/a |
| Tables, charts, SmartArt, groups, video/audio, master styles | skipped (the reader reports how many shapes were skipped) | – |

**MyOffice private part** `ppt/myoffice/extras.xml` (relationship type `http://myoffice.local/relationships/extras`) stores what PowerPoint has no equivalent for: **object animations** (effect, trigger, duration, delay, matched by slide index and shape id), **speaker notes** and transition durations. PowerPoint ignores the part; MyPowerPoint restores it. Files authored in PowerPoint simply have no extras.

## CSV

RFC 4180 (quotes, doubled quotes, embedded newlines). Delimiter detection looks at the first line: tab, semicolon or comma. Import auto-formats percentages and ISO dates. Export writes **displayed** values, UTF-8 with BOM, CRLF line ends.

## HTML / Markdown / plain text

* HTML is produced by `QTextDocument::toHtml()`; images become `data:image/png;base64,…` so the file is self-contained. On load, data URIs are converted back into document resources.
* Markdown uses `QTextDocument::setMarkdown/toMarkdown` (CommonMark/GFM subset supported by Qt).
* Text decoding: BOM detection → UTF-8 → system 8-bit fallback.
