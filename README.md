# Virtio Balloon Driver - Phase 1 Baseline

## Status
Working custom virtio balloon driver (`vballoon_lab`) bound to `virtio0` in guest.

## Verified Results (from completed run)
- Start actual: `3221225472` (3 GiB)
- Inflate target set to `2147483648` (2 GiB)
- Observed transition: `3221225472 -> 2863321088 -> 2539270144 -> 2147483648`
- Deflate target set to `3221225472` (3 GiB)
- Observed return: `2147483648 -> 3221225472`
- Final tests had no fresh `BUG/Oops/Call Trace`.

## Driver Binding
`/sys/bus/virtio/devices/virtio0/driver -> /sys/bus/virtio/drivers/vballoon_lab`

## QMP Commands Used
- `query-balloon`
- `balloon {"value":2147483648}`
- `balloon {"value":3221225472}`

## Notes
- Out-of-tree module signature warning is expected.
- This repo snapshot is the stable Phase 1 baseline before Phase 2 (host daemon/shared-memory path).
