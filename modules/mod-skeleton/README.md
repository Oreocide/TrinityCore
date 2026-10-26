# mod-skeleton

One sentence: what this module does for a server owner.

## Install

```sh
git clone <this repo> modules/mod-skeleton    # or copy the directory
cmake <build dir>                             # re-run cmake so the module is picked up
cmake --build <build dir>
cp <conf dir>/worldserver.conf.d/mod_skeleton.conf.dist <conf dir>/worldserver.conf.d/mod_skeleton.conf
```

Disable it without deleting it: `cmake -DMODULE_MOD_SKELETON=disabled <build dir>`.

## Configuration

Copy `conf/mod_skeleton.conf.dist` to `mod_skeleton.conf` in the worldserver config directory. The server only reads
`*.conf`, so the defaults in the code must match the `.dist` file.

| Key | Default | Description |
| --- | --- | --- |
| `Skeleton.Enable` | `0` | Log a line at startup to prove the module is loaded. |

## Database

List the tables the module creates and which database they live in. SQL files go in `data/sql/db-<auth|characters|world|hotfixes>/`
and are applied automatically on the next worldserver start. They must be idempotent and have a name that is unique
across all update directories, for example `2026_01_01_00_world_mod_skeleton_example.sql`.

## Requirements

Core revision / required hooks / other modules this one depends on.
