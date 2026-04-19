# Memory Balloon Driver Project

## Current Status
This repo now has:
1. **Phase 1**: working custom guest virtio balloon driver (`guest/vballoon_lab/vballoon_lab.c`)
2. **Phase 2**: working host daemon (`host/balloond`) controlling balloon target through **real QMP** and publishing state via shared memory

## Phase 1 Summary (Guest Driver)
- Device binding confirmed:
  - `/sys/bus/virtio/devices/virtio0/driver -> /sys/bus/virtio/drivers/vballoon_lab`
- Verified behavior from earlier run:
  - Start actual: `3221225472` (3 GiB)
  - Inflate to `2147483648` (2 GiB): transition observed
  - Deflate back to `3221225472` (3 GiB): transition observed
- No fresh kernel `BUG/Oops/Call Trace` in final validation runs

## Phase 2 Summary (Host Daemon + Shared Memory + QMP)
- Host daemon (`host/balloond`) now:
  - writes target into shared memory (`target_bytes`, `cmd_seq`)
  - sends target to QEMU via QMP `balloon` command
  - polls QMP `query-balloon` for `actual`
  - updates shared-memory `actual_bytes`, `ack_seq`, `status`, `last_error`
- Real QMP smoke test script:
  - `scripts/smoke_phase2.sh`
  - prints full inflate/deflate logs explicitly

## Latest Verified Results
- Real QMP inflate proof:
  - `proofs/phase2_qmp_inflate_ok.log`
- Real QMP deflate proof:
  - `proofs/phase2_qmp_deflate_ok.log`
- Detailed phase status:
  - `PHASE2_STATUS.md`

## Key Commands
- Start QEMU (with QMP socket):
  - `./scripts/run_qemu_phase2.sh`
- Run real QMP smoke:
  - `./scripts/smoke_phase2.sh`

## Notes
- `guest/shm_agent` is still an MVP userspace bridge for shared-memory flow.
- `host/balloond/src/policy.c` is still minimal and intended for Phase 3 expansion.
- This project is now beyond simulator-only behavior on host side for Phase 2 balloon control path.
