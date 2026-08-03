#!/bin/sh

set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

if [ ! -f "$ROOT/premake5.lua" ] || [ ! -d "$ROOT/EppoEngine" ]; then
    echo "Refusing to clean unexpected repository root '$ROOT'." >&2
    exit 1
fi

rm -rf -- \
    "$ROOT/build" \
    "$ROOT/.eppo" \
    "$ROOT/.vcpkg-cache" \
    "$ROOT/EppoEditor/Resources/Templates/NewProject/Scripts/bin" \
    "$ROOT/EppoEditor/Resources/Templates/NewProject/Scripts/obj" \
    "$ROOT/EppoEngineTesting/TestData/Scripts/bin" \
    "$ROOT/EppoEngineTesting/TestData/Scripts/obj" \
    "$ROOT/EppoScriptCore/bin" \
    "$ROOT/EppoScriptCore/obj"

for project in "$ROOT"/EppoEditor/Projects/*; do
    [ -d "$project" ] || continue
    rm -rf -- "$project/Scripts/bin" "$project/Scripts/obj"
done

find "$ROOT" -type d -name __pycache__ -prune -exec rm -rf -- {} \;
find "$ROOT" -type f \( \
    -name '*.sln' -o \
    -name '*.slnx' -o \
    -name '*.vcxproj' -o \
    -name '*.vcxproj.filters' -o \
    -name '*.vcxproj.user' -o \
    -name '*.ninja' -o \
    -name '.ninja_deps' -o \
    -name '.ninja_log' -o \
    -name 'compile_commands.json' -o \
    -name '*.pyc' -o \
    -name '*.pyo' \
\) -delete

echo "Removed all generated Eppo build artifacts."
