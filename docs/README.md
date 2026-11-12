# MyOffice documentation

Start here if you want to understand, build, extend or debug the project.

1. **New to the project?** Read [ARCHITECTURE.md](ARCHITECTURE.md) (15 minutes) – it explains the layers and the life of a document.
2. **Building:** [BUILDING.md](BUILDING.md).
3. **Working on shared code:** [CORE.md](CORE.md) and [UI_FRAMEWORK.md](UI_FRAMEWORK.md).
4. **Working on an app:** [MYWORD.md](MYWORD.md), [MYEXCEL.md](MYEXCEL.md), [MYPOWERPOINT.md](MYPOWERPOINT.md).
5. **Adding something:** [ADDING_FEATURES.md](ADDING_FEATURES.md) is a cookbook with copy-paste recipes.
6. **Something broken?** [TROUBLESHOOTING.md](TROUBLESHOOTING.md).
7. **File compatibility:** [FILE_FORMATS.md](FILE_FORMATS.md).
8. **What is missing / planned:** [ROADMAP.md](ROADMAP.md).

## Conventions used in these documents

- Paths are relative to the repository root.
- `Class::method()` refers to the C++ symbol; the header that declares it sits next to the `.cpp` file.
- "UI thread" means the Qt main thread. Only file *reading* runs on a worker thread (see [CORE.md](CORE.md#fileio)).
