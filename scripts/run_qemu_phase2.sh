#!/usr/bin/env bash
set -euo pipefail

ROOT="${HOME}/virtio-balloon"
IMG="${ROOT}/images/ubuntu-24.04-server-cloudimg-amd64.img"
SEED="${ROOT}/images/seed.iso"
QMP="${ROOT}/logs/qmp.sock"

rm -f "${QMP}"

exec qemu-system-x86_64 \
  -name balloon-tcg \
  -accel tcg,thread=multi \
  -machine q35 \
  -cpu max \
  -smp 2 \
  -m 3072 \
  -drive if=virtio,file="${IMG}",format=qcow2 \
  -drive if=virtio,file="${SEED}",format=raw \
  -nic user,hostfwd=tcp::2222-:22 \
  -device virtio-balloon-pci,id=balloon0,deflate-on-oom=on,free-page-reporting=on \
  -qmp unix:"${QMP}",server,nowait \
  -nographic
