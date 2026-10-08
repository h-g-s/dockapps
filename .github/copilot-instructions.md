# Copilot instructions

## Repository shape

This repository is a collection of independent Window Maker dockapps, not a single application: most top-level directories build one dockapp and own its source, assets, documentation, dependencies, and build setup. The fork's goal is to preserve these dockapps while moving legacy C code, build systems, and dependencies toward modern standards and technologies that work on current Linux distributions. There is no root-level build, test, or lint command. Before changing a dockapp, read its local `README` and `INSTALL` files when present; treat them as useful historical and app-specific context, while following the fork's modernization goal where old build or technology choices need updating.

`libdockapp/` is a standalone support library for dockapps. Its API is in `libdockapp/src/dockapp.h`; consult its README and examples when working on code that already uses it. Not every dockapp uses this library, and several have their own local support code.

Build systems vary by directory. Many projects use Autoconf/Automake, sometimes with nested `SUBDIRS`; others provide a hand-written `Makefile` or a pre-generated `configure` script. Treat each dockapp as its own build target and avoid assuming that a change or command applies across the whole repository.

## Build, test, and formatting

For an Autotools project, run commands from that dockapp's directory. If it has `configure.ac` but no `configure`, generate the build scripts with `autoreconf -fi`; if the project provides `autogen.sh` or `autogen`, use that script instead. Then configure and build:

```sh
./configure
make
```

If `configure` is already provided, start with `./configure`. Some legacy C dockapps need the workaround documented in the root README for multiple-definition linker errors:

```sh
CFLAGS="-fcommon" bash ./configure
make
```

Use that only when the app's own instructions do not prescribe a different setup. For projects with a plain `Makefile` or special dependencies/options, follow that app's `README`/`INSTALL` and `Makefile`; there is no universal root build command.

The discovered Automake test suite is in `wmbiff/wmbiff/Makefile.am`. After generating and configuring `wmbiff` as above, run both tests with:

```sh
make -C wmbiff/wmbiff check
```

To run one test only, pass its Automake test name:

```sh
make -C wmbiff/wmbiff check TESTS=test_wmbiff
make -C wmbiff/wmbiff check TESTS=test_tlscomm
```

No repository-wide lint target was found. `wmbiff/wmbiff/Makefile.am` has an `indent` formatting target using GNU indent's K&R options; it rewrites the WMBiff source files, so use it only when intentionally formatting that project.

## Project-specific conventions

- Keep changes scoped to the relevant dockapp. The projects have independent build files, documentation, runtime configuration, and assets; do not introduce a repository-wide abstraction for a change that belongs to one app.
- Modernize rather than perpetuate obsolete C standards, build tooling, or dependencies when doing so advances the fork's goal. Preserve the dockapp's user-facing purpose and behavior while updating its implementation; avoid retaining a legacy workaround when the underlying compatibility issue can be fixed cleanly.
- When changing build logic, edit the relevant source build definitions (such as `configure.ac` and `Makefile.am`) rather than generated `configure`/Makefiles when those are generated from them. Follow app-specific build instructions where still applicable, and account for each dockapp's independent build setup.
- Preserve app-specific behavior, command-line options, configuration formats, and man pages together when changing a dockapp's user-visible interface. These small programs commonly expose their primary interface through the dock/window-manager interaction and their own configuration or man page rather than a shared application framework.
- WMBiff documents a project-specific formatting rule in `wmbiff/wmbiff/Makefile.am`: GNU indent with K&R style and four-space tab indentation. Apply that convention to WMBiff only; other dockapps have their own history and style.
