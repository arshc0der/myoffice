# Roadmap and known limitations

The list is ordered roughly by value; "good first issue" marks items that are small and self-contained.

## Cross-cutting

- [ ] Automated tests (QtTest) and CI (GitHub Actions: Linux + Windows builds) — *good first issue*
- [ ] Choose and add a `LICENSE`
- [ ] Autosave/recovery for MyExcel and MyPowerPoint
- [ ] Parse large files on a worker thread
- [ ] Translations (`tr()` is already used throughout; add `.ts` files and a language setting)
- [ ] High-contrast theme and a full accessibility review (screen-reader names exist on custom widgets)
- [ ] macOS support
- [ ] Installers (MSIX/NSIS, AppImage/Flatpak)

## MyWord

- [ ] Headers, footers and page numbers on screen and in DOCX
- [ ] Images in DOCX import/export
- [ ] Real on-screen pagination
- [ ] Draggable margin markers on the ruler
- [ ] Navigation pane (headings outline), spell-check, RTF import/export
- [ ] Styles gallery and style inheritance from DOCX

## MyExcel

- [ ] Dependency graph for recalculation (today the cache is cleared on every edit)
- [ ] Charts, conditional formatting, filters, freeze panes, merged cells
- [ ] Cross-sheet references (`Sheet2!A1`) and named ranges
- [ ] Drag-fill handle, column widths in XLSX, data validation
- [ ] More functions (`SUMIF`, `COUNTIF`, `VLOOKUP`, `MEDIAN`, date functions) — *good first issue* (see [cookbook](ADDING_FEATURES.md#add-a-spreadsheet-function))

## MyPowerPoint

- [ ] Tables and charts; grouping/ungrouping; text with multiple runs
- [ ] Presenter view with notes and timer
- [ ] Slide masters/layout gallery; theme fonts/colours
- [ ] Audio/video, hyperlinks, hidden slides
- [ ] Import of PPTX tables, groups and master placeholders

## Known limitations (current)

| Area | Limitation |
|---|---|
| MyWord | No RTF, headers/footers, spell-check; page count and page-break lines are approximate on screen |
| MyExcel | Relative references inside sorted rows are not rewritten; dependency graph not implemented |
| MyPowerPoint | PowerPoint does not play MyPowerPoint animations; they are stored in a private part |
| All | No automated tests; private Qt headers are required |
