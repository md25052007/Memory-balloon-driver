#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TARGET="${1:-2147483648}"   # default 2 GiB
QMP="${ROOT}/logs/qmp.sock"

make -C "${ROOT}/host/balloond" >/dev/null
exec "${ROOT}/host/balloond/balloond" "${TARGET}" "${QMP}"
