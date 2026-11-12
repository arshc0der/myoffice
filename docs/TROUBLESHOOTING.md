# Troubleshooting

## Icons are missing

Symptoms: ribbon buttons show only text, some groups look empty, the Application Output prints `Missing icon resource: <name>`.

Check in this order:

1. **Property name.** Ribbon buttons read the icon from the action property **`iconName`**. `QAction` already has a built-in property called `icon` (a `QIcon`), so a string stored under `"icon"` is lost. Both places must use `iconName`:
   * `Workspace::makeAction` → `a->setProperty("iconName", icon);`
   * `RibbonButton::paintEvent` → `m_action->property("iconName").toString()`
2. **Resource path.** `resources/resources.qrc` must list `icons/<name>.svg` (relative to the `.qrc`, prefix `/`) and `core/iconprovider.cpp` must load `:/icons/<name>.svg`.
3. **Build system.** `MyOffice.pro` contains `RESOURCES += resources/resources.qrc` and `QT += svg`. Re-run qmake and **Rebuild** (not just Build) after editing the `.qrc`.
4. **Missing file.** The warning prints the exact name; make sure `resources/icons/<name>.svg` exists and is listed.

## Build problems

See the table in [BUILDING.md](BUILDING.md#common-build-errors).

## Application starts but a document does not open

| Message in the toast | Meaning |
|---|---|
| "The file does not exist." / "You do not have permission…" | Path or permission problem. |
| "The file is larger than the 2 GB limit." | Hard safety limit in `FileManager`. |
| "…files are not supported yet." | `rtf doc xls ppt odt pdf` are recognised but not implemented. |
| "This file type is not supported." | Unknown extension and the file looks binary. |
| "The file is corrupted or is not a valid Office document." | Missing ZIP signature for `docx/xlsx/pptx`. |
| "Not a valid DOCX/XLSX/PPTX file" | ZIP is readable but the main part is missing or malformed. |

## The document opens read-only

By design: files open in **View mode**. Click *Enable editing*, press `Save`, or turn the behaviour off in *Settings → Open files in view mode* (or the switch on the launcher).

## Animations feel slow / janky

* Turn on *Settings → Reduce animations* (or set `MYOFFICE_REDUCE_MOTION=1`).
* The zoomable MyWord canvas uses a proxied widget; very large documents benefit from a release build.

## Layout looks different from Word

MyWord shows a continuous page and marks page breaks approximately. **PDF export is exact** because it paginates with the same engine used for printing. Fonts that are not installed on the machine are substituted by Qt.

## Resetting the application state

Settings are stored with `QSettings` (organisation/application `MyOffice`): Windows registry `HKCU\Software\MyOffice\MyOffice`, Linux `~/.config/MyOffice/MyOffice.conf`. Delete that entry to reset theme, recent files and options. Recovery snapshots live in the application's local data folder under `recovery/`.

## Reporting a bug

Open an issue at https://github.com/arshc0der/myoffice/issues and include: operating system, Qt version (`About` dialog shows it), steps to reproduce, the file that triggers the problem (if shareable) and any text from the Application Output.
