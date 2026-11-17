# Copilot instructions for VirtualClub

## Build and validation

This is a C++20 Qt 6 application built with CMake and Ninja. Configure from the repository root:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

For a release-style build and local installation:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
cmake --install build --prefix dist
```

The project has no configured test or lint target (`CMakeLists.txt` does not define `enable_testing()`/`add_test()` or a lint target). Use the build as the primary validation check. There are no repository test commands or single-test commands to run.

Required system dependencies are Qt 6 (Widgets, Multimedia, Svg, LinguistTools), Python development headers/interpreter, pybind11, and a C++ toolchain. Desktop Linux additionally needs pkg-config and FUSE 3; Windows needs Clang/clang-cl and WinFsp. yaml-cpp, lexbor, nlohmann/json, and cpptrace (Debug/RelWithDebInfo) are fetched by CMake. The CI workflow downloads a relocatable Qt bundle with `.github/scripts/install_qt.py` before configuring.

Use Clang rather than MSVC. The CI configures with `clang`/`clang++` on Linux and `clang-cl` on Windows. Debug builds create an `assets` symlink beside the executable; non-CI release builds copy `assets` there.

## Architecture

- `src/main.cpp` is the application bootstrap. It creates `ProfileSingleApp`, parses `--profile` and `--mod`, initializes global `QSettings`, performs compatibility migration, embeds Python through pybind11, loads fonts/audio/translations, creates `MainWindow`, and dispatches background mod launches.
- `MainWindow` assembles the sidebar and `Router`. Screens under `src/screens/` are route-level widgets for home, installed mods, online mods, settings, mod details, and profile dialogs. `src/ui/` contains reusable widgets and navigation/transition primitives; `Router` supports fixed routes and simple `:parameter` route segments.
- Configuration is split intentionally: global launcher settings use `QSettings` keys named `GSTK_*`, while profile-specific state is stored as YAML through `ProfileSettings`/`YamlSettings` using `STK_*` keys. Installed mods are persisted separately in `profiles/mods/<profile>.yaml`; profile identity also scopes the single-instance lock and local IPC.
- `ModsIndex` is the profile-scoped catalog and installer. It reads Ren’Py metadata and archives using the embedded Python archive readers, stores mod metadata in YAML, and writes cached icons below the application data directory.
- `SessionManager` owns active mounts and launched games. `VFSInstance`/`vfs.cpp` expose a layered virtual filesystem: writable per-mod delta data over the mod directory over the base game, with launcher patches from `assets/patches`. A mounted mod is launched with its bundled `pythonw` and launcher-specific environment variables such as `MVC_MOD_ID`, `MVC_APPDATA_PATH`, and optional save/developer settings.
- `src/utils/` holds persistence, profile/process coordination, archive and download helpers, background loading, translation, and other shared services. Runtime resources are split between compiled Qt resources in `resources/` and user-editable launcher patches in `assets/`.
- FUSE-backed VFS code is compiled for desktop platforms. Android and Termux define conditions that leave `MVC_VFS_AVAILABLE` unset, so VFS/session behavior must remain guarded by that macro.

## Repository-specific conventions

- Follow the existing Qt ownership and signal/slot style. Widgets are generally created with parents and screens are created by route factories; route transitions decide when dynamically-created outgoing screens are removed.
- Keep settings keys in `src/consts.hpp`. Use `STK_*` for per-profile `ProfileSettings` values and `GSTK_*` for global `QSettings` values rather than inventing ad-hoc keys.
- `YamlSettings` uses slash-delimited paths and saves automatically on destruction. When changing persisted data, account for existing YAML files and the compatibility migration logic in `main.cpp`.
- UI text should use Qt translation (`tr()`/`QT_TR_NOOP`) and screens should register updates with `addRetranslateCallback` when text is created dynamically. Available translations are compiled from `translations/*.ts` by `qt_add_translations`.
- Reuse shared UI helpers such as `Button`, `IconButton`, `MESWidgets`, `LucideIcons`, `GradientBackground`, and `Router` instead of duplicating styling or navigation logic. The application’s visual language is set centrally in `main.cpp` and uses the Quicksand fonts bundled in `resources/`.
- Source discovery is CMake-based (`file(GLOB_RECURSE ... CONFIGURE_DEPENDS ...)`), so new implementation/header files under `src/` are normally picked up automatically. Qt classes using `Q_OBJECT` in implementation files may need the same explicit generated-moc include pattern already used in files such as `src/screens/OnlineModsIndex.cpp`.
- Preserve platform guards and path conventions when touching process/VFS code. Windows uses WinFsp and Windows file APIs; Unix desktop uses FUSE 3; Android/Termux intentionally omit the VFS library path.
- Do not move `assets/patches` into Qt resources: those files are intentionally deployed beside the executable so the launcher can load them as editable runtime content.
