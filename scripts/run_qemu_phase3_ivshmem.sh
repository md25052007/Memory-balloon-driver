#!/usr/bin/env bash
set -euo pipefail

ROOT="${HOME}/virtio-balloon"
IMG="${ROOT}/images/ubuntu-24.04-server-cloudimg-amd64.img"
SEED="${ROOT}/images/seed.iso"
QMP="${ROOT}/logs/qmp.sock"
IVSHMEM_FILE="/dev/shm/balloon_ivshmem.bin"

rm -f "${QMP}"
mkdir -p "${ROOT}/logs"
touch "${IVSHMEM_FILE}"

exec qemu-system-x86_64 \
  -name balloon-phase3-ivshmem \
  -accel tcg,thread=multi \
  -machine pc \
  -cpu max \
  -smp 2 \
  -m 3072 \
  -drive if=virtio,file="${IMG}",format=qcow2 \
  -drive if=virtio,file="${SEED}",format=raw \
  -nic user,hostfwd=tcp::2222-:22 \
  -device virtio-balloon-pci,id=balloon0,deflate-on-oom=on,free-page-reporting=on \
  -object memory-backend-file,id=ivshmem0,mem-path="${IVSHMEM_FILE}",size=1M,share=on \
  -device ivshmem-plain,memdev=ivshmem0 \
  -qmp unix:"${QMP}",server,nowait \
  -nographic
