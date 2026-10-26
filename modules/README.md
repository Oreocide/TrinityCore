# Modules

AzerothCore-style plug-and-play modules for TrinityCore. Every directory in `modules/` that contains a `src/`
directory is compiled into the worldserver. Drop a module in, re-run `cmake`, build. Nothing in the core has to be edited.

## Make a module

```sh
modules/create_module.sh mod-my-feature     # copies modules/mod-skeleton and renames everything
cmake <build dir>                           # re-run cmake: the module list is globbed at configure time
cmake --build <build dir>
```

The name must be lowercase letters, digits and `-`. Edit `src/`, the `.conf.dist`, and the README it generated.

To use someone else's module, clone or copy it into `modules/` and re-run `cmake`.

## Layout

```
modules/<mod-name>/
  src/                      C++ sources, any layout; every subdirectory becomes an include path
    <mod_name>_loader.cpp   defines void Add<mod_name>Scripts()  ('-' in the directory name becomes '_')
  conf/*.conf.dist          installed to <CONF_DIR>/worldserver.conf.d/ by `make install`
  data/sql/db-auth/         \
  data/sql/db-characters/    |  applied by the DB updater and recorded in the `updates` table
  data/sql/db-world/         |  (see "SQL" below)
  data/sql/db-hotfixes/     /
  <mod-name>.cmake          optional, included after the module's sources were added
  README.md
```

## The contract

1. **Entry point.** The generated loader calls `Add<ident>Scripts()` where `<ident>` is the directory name with every
   character outside `[A-Za-z0-9_]` replaced by `_` (`mod-my-feature` -> `Addmod_my_featureScripts`). It must be
   declared and defined by you, usually in `src/<ident>_loader.cpp`, and it creates your script objects.
   A missing definition is a link error.
2. **Hooks.** Use the normal script classes from `ScriptMgr.h` (`WorldScript`, `PlayerScript`, `CreatureScript`,
   `CommandScript`, ...). Modules cannot add new hooks; if you need one, that is a core change.
3. **Switch.** Each module gets a cache variable `MODULE_<IDENT>` (upper case): `static` (default) or `disabled`.
   `cmake -DMODULE_MOD_MY_FEATURE=disabled .` removes it from the build and from the SQL updater.
4. **Optional `<mod-name>.cmake`.** Included from `modules/CMakeLists.txt` after the module's sources were added.
   `MODULE_TARGET` (the shared `modules` library) and `MODULE_PATH` are set. Example:

   ```cmake
   find_package(CURL REQUIRED)
   target_link_libraries(${MODULE_TARGET} PRIVATE CURL::libcurl)
   ```

## Configuration

- Put defaults in `conf/<ident>.conf.dist`. The server loads every `*.conf` file in `worldserver.conf.d`
  (or the directory passed with `-cd`), never `*.conf.dist`. Server owners copy the `.dist` to `.conf` to change values.
- In this repository's Docker setup the container reads `docker/etc/worldserver.conf.d/`, not the directory
  `make install` copies the `.dist` files to. Put the `.conf` there.
- Every file needs the `[worldserver]` section header.
- Keys share one namespace with `worldserver.conf`: prefix them with the module name (`MyFeature.Enable`).
- Read config in `WorldScript::OnConfigLoad` so `.reload config` works.
- **Keep the default in `sConfigMgr->Get*Default(...)` equal to the `.dist` value**, because without a `.conf` file
  the `.dist` is not read.

## SQL

- Files go in `data/sql/db-<auth|characters|world|hotfixes>/`. They are applied in file-name order on worldserver start
  and recorded in the `updates` table by file name.
- **File names must be unique across the core and all modules**, so include the module name:
  `2026_01_01_00_world_mod_my_feature_spawns.sql`.
- **Write idempotent SQL** (`DELETE` then `INSERT`, `CREATE TABLE IF NOT EXISTS`, `INSERT IGNORE`). See the limitations.
- The updater finds module SQL through the **source directory** (`SourceDirectory` / `$/modules/...`). The source tree
  has to be reachable from where the worldserver runs, which is the case for the Docker setup in this repository
  (`.:/trinity/src:ro`). If it is missing, module SQL is silently skipped.
- Do not ship SQL in the skeleton. It is built and enabled in every checkout.

### Known limitations

- **Module SQL is only applied by the worldserver.** bnetserver has its own updater for the auth database
  (`Updates.EnableDatabases`, `0` by default) that does not know the module list. If you enable it, it treats applied
  `db-auth` module files as missing and prunes their `updates` rows, and the next worldserver start applies them again.
  Keep auth SQL idempotent.
- Disabling a module (or removing its directory) leaves its rows in `updates`. They are treated as orphaned and
  pruned the same way, so re-enabling later re-applies the files. Idempotent SQL makes this harmless.

## Pitfalls

- **One shared target.** All modules are compiled into a single static library with every `src/` subdirectory on the
  include path. A module header called `Config.h`, `Log.h` or `Utils.h` shadows the core's or another module's. Use unique
  header names or put them in a subfolder named after the module and include them as `"mod_my_feature/Utils.h"`.
- Put classes in an anonymous namespace or prefix them with the module name; two modules defining the same class in
  different files is an ODR violation that the linker may not report.
- `WITH_WARNINGS_AS_ERRORS` applies to modules too. Build clean.
- cmake globs the module directory at configure time. Re-run `cmake` after adding or removing a module or a source file.
- Do not edit `<build>/modules/gen_moduleloader/ModulesLoader.cpp`; it is generated from `ModulesLoader.cpp.in.cmake`.

## Distributing a module

A module is a plain directory, so it can live in its own git repository: `git init` inside `modules/mod-my-feature`
(or clone the repo into `modules/`). The core's `.gitignore` does not exclude `modules/`; add the module's directory to
your own ignore rules if your checkout tracks the core and the module in different repositories.
