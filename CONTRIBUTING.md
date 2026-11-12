# Contributing to MyOffice

Thank you for helping! This guide explains how to set up, what the code should look like and how to get a change merged. If you are new to the code base read [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) first and use [docs/ADDING_FEATURES.md](docs/ADDING_FEATURES.md) as a recipe book.

## Ways to contribute

- **Report bugs** — use the bug template; include OS, Qt version, steps and a sample file.
- **Fix bugs / add features** — see [docs/ROADMAP.md](docs/ROADMAP.md); items marked *good first issue* are small.
- **Write tests** — there is no test suite yet (see the recipe in the cookbook).
- **Improve documentation** — corrections and examples are always welcome.
- **Translate** — `tr()` is used for user-visible strings.

## Development setup

1. Install Qt 6.5+ (with Svg and private headers) and a C++17 compiler — [docs/BUILDING.md](docs/BUILDING.md).
2. Fork the repository, then clone your fork:
   ```bash
   git clone https://github.com/<you>/myoffice.git && cd myoffice
   git remote add upstream https://github.com/arshc0der/myoffice.git
   ```
3. Create a branch from `main`: `git switch -c fix/word-table-merge`.
4. Open `MyOffice.pro` in Qt Creator (or `qmake && make`).

## Workflow

1. Keep changes **focused**: one bug or feature per pull request.
2. Build in **debug** and **release**; run the application and exercise what you changed in all three apps if you touched shared code (`core/`, `ui/`).
3. Test with **light and dark themes** and with *view mode* on and off.
4. Update documentation (`docs/…`) when behaviour, public methods or file formats change.
5. Open a pull request against `main` and fill in the template.

### Commit messages

Use short, imperative subjects, optionally with a scope (Conventional Commits style):

```
fix(excel): keep absolute refs when filling down
feat(ppt): add push-down slide transition
docs: describe the autosave flow
```

## Code style

The existing code is the reference — match it.

| Topic | Convention |
|---|---|
| Language | C++17, Qt 6 APIs; no raw `new` without a Qt parent or smart pointer owning it |
| Indentation | 4 spaces, no tabs; function and class braces on their own line, control-flow braces on the same line |
| Naming | `CamelCase` types, `camelCase` functions, `m_` prefix for members, `kName` for constants, `UPPER` never for constants |
| Strings | `QStringLiteral("…")` for constants, `tr("…")` for **every** user-visible string, `QLatin1String` for comparisons |
| Headers | `#pragma once`; forward-declare instead of including when a pointer is enough |
| Includes | Project includes use paths from the repository root (`"core/thememanager.h"`) |
| Comments | Explain *why*, not *what*; document public classes in the header |
| Signals/slots | New-style `connect` with lambdas; context object as 3rd argument so connections are cleaned up |
| Qt Designer / `.ui` files | Not used — widgets are built in code |
| Style sheets | Avoid; paint custom widgets or use `QPalette` (a table view header style sheet is the accepted exception) |

### UI rules

- Colours come from `ThemeManager::palette()`; numbers from `Design::` tokens.
- Animation durations go through `ThemeManager::duration()` (honours *reduce motion*).
- Every clickable custom widget needs an accessible name, keyboard activation and a visible focus state.
- Commands that modify a document are created with `editAction()` / `editsDocument = true` so view mode disables them.
- Icons are SVGs using `currentColor`; never ship raster icons.

### Data-safety rules (important for an office suite)

- **Never write a user file non-atomically** — use `QSaveFile`.
- Validate untrusted input (size, signature, malformed XML) and fail with a readable message instead of crashing.
- Bound loops and recursion that depend on file content.
- A destructive operation must be undoable or confirmed.

## Pull-request checklist

- [ ] Builds without new warnings (`CONFIG += warn_on` is enabled)
- [ ] Manually tested (describe how in the PR)
- [ ] Works in light and dark themes
- [ ] View mode respected (no edits possible while read-only)
- [ ] New source files are listed in the right `.pri`; new icons in `resources.qrc`
- [ ] All new UI strings use `tr()`
- [ ] Docs / README updated if behaviour changed
- [ ] No unrelated reformatting

## Reviewing

Maintainers look at correctness, data safety, consistency with the architecture, and readability. Expect small requests for changes; they are about the code, not about you.

## Security

Document formats are an attack surface. If you find a crash or memory-safety problem triggered by a crafted file, please report it privately to the maintainer (open a GitHub security advisory on the repository) instead of a public issue.

## Conduct

Be respectful and constructive. Harassment and personal attacks are not tolerated. Maintainers may remove comments or contributions that violate this.
