#!/usr/bin/env bash
# Creates modules/<name> from modules/mod-skeleton.
# Usage: modules/create_module.sh mod-my-feature
set -euo pipefail

NAME="${1:-}"
if [[ ! "$NAME" =~ ^[a-z0-9][a-z0-9-]*$ ]]; then
    echo "usage: $0 <mod-name>   (lowercase letters, digits and '-')" >&2
    exit 1
fi

DIR="$(cd "$(dirname "$0")" && pwd)"
if [[ -e "$DIR/$NAME" ]]; then
    echo "$DIR/$NAME already exists" >&2
    exit 1
fi

IDENT="${NAME//-/_}"     # mod_my_feature
KEY="$(echo "${IDENT#mod_}" | sed -E 's/(^|_)([a-z])/\U\2/g')"   # MyFeature

cp -r "$DIR/mod-skeleton" "$DIR/$NAME"
cd "$DIR/$NAME"
mv src/mod_skeleton.cpp "src/${IDENT}.cpp"
mv src/mod_skeleton_loader.cpp "src/${IDENT}_loader.cpp"
mv conf/mod_skeleton.conf.dist "conf/${IDENT}.conf.dist"
grep -rl --null 'mod_skeleton\|mod-skeleton\|MOD_SKELETON\|Skeleton' . | xargs -0 sed -i \
    -e "s/MOD_SKELETON/${IDENT^^}/g" -e "s/mod_skeleton/${IDENT}/g" -e "s/mod-skeleton/${NAME}/g" -e "s/Skeleton/${KEY}/g"

echo "Created modules/$NAME. Re-run cmake in your build directory to pick it up."
