#!/usr/bin/env bash
set -euo pipefail

ROOT="${HOME}/virtio-balloon"
TARGET="${1:-2147483648}"   # default 2 GiB

make -C "${ROOT}/host/balloond" >/dev/null
exec "${ROOT}/host/balloond/balloond" "${TARGET}"
