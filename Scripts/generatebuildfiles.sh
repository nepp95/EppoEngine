#!/bin/sh

set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

if ! command -v python3 >/dev/null 2>&1; then
    echo "Python 3 was not found. Install Python 3 and rerun Scripts/generatebuildfiles.sh." >&2
    exit 1
fi

exec python3 "$ROOT/Setup.py" --generate-only "$@"
