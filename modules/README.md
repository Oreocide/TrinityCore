# Modules

AzerothCore-style modules for TrinityCore. Every directory here that contains `src/` is built into the
worldserver automatically (re-run `cmake` after adding or removing one).

```
modules/<mod-name>/
  src/                      C++ sources (any layout, every subdirectory is an include path)
    <mod_name>_loader.cpp   defines void Add<mod_name>Scripts()  ('-' in the directory name becomes '_')
  conf/*.conf.dist          installed to <CONF_DIR>/worldserver.conf.d/
  data/sql/db-auth/         \
  data/sql/db-characters/    |  applied by the DB updater like sql/updates/*, recorded in `updates`
  data/sql/db-world/         |  (file names must be unique across ALL update directories,
  data/sql/db-hotfixes/     /    so prefix them with the module name)
  <mod-name>.cmake          optional, included after the sources were added (target: modules)
```

- New module: `modules/create_module.sh mod-my-feature`
- Disable a module: `cmake -DMODULE_MOD_MY_FEATURE=disabled .`
- Config: the worldserver loads every `*.conf` from its config directory (`worldserver.conf.d`, or `-cd <dir>`).
  Copy `<mod>.conf.dist` to `<mod>.conf` there to change values. Every file needs the `[worldserver]`
  section header. Keys share one namespace with `worldserver.conf`, so prefix them (`Playerbots.*`).
- Hooks: modules use the normal `ScriptMgr.h` script classes (`WorldScript`, `PlayerScript`, `ServerScript`, ...).
