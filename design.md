# goblinmode – plugin design notes

`goblinmode` is a single VCV Rack 2 plugin (one `plugin.json`, one binary, slug `goblinmode`) that holds all of its modules. Each module lives in its own folder under `src/`.

## Modules

| Module | Folder | Notes |
|---|---|---|
| ChordonBleh | `src/ChordonBleh/` | Polyphonic chord generator. See `src/ChordonBleh/design.md`. |

## Layout

```
plugin.json          one manifest, one "modules" entry per module
Makefile
src/
  plugin.cpp/.hpp    registers every model
  <Module>/          <Module>.cpp, module-only headers, design.md
res/
  <Module>.svg       flat, named after the module, as the Rack SDK expects
test/
  <Module>/          standalone tests for Rack-independent logic
```

The Makefile compiles `src/*.cpp` and `src/*/*.cpp` and adds `-Isrc`, so module files can `#include "plugin.hpp"`.

Shared code (a `src/common/` folder) doesn't exist yet. Create it when a second module needs something that is currently private to one module, such as the panel label and font helpers in `ChordonBleh.cpp`.

## Adding a module

1. Create `src/<Name>/<Name>.cpp` defining `Model* model<Name>`.
2. Add `res/<Name>.svg`.
3. Add `extern Model* model<Name>;` to `src/plugin.hpp`.
4. Add `p->addModel(model<Name>);` to `src/plugin.cpp`.
5. Add an entry to `modules` in `plugin.json` (slug must match the one passed to `createModel`).
6. Optionally add `test/<Name>/` and `src/<Name>/design.md`, and a row in the table above.

## Building and installing

- `make -j && make install` from the plugin root. `RACK_DIR` is `~/Documents/Rack-SDK`.
- Rack extracts the package on startup, so restart Rack to load a new build.
- The version in `plugin.json` must start with `2.` or Rack 2 refuses to load the plugin.
- `build/`, `dep/`, `dist/` and `plugin.dylib` are git-ignored.
