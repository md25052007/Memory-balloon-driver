# Proof Logs

Captured output from real runs on QEMU (Ubuntu 24.04 guest, 3 GiB RAM, April 2026).
The file prefixes come from the development phases; this table maps each log to what it proves.

| Feature | Log | What to look for |
|---|---|---|
| Inflate 3 GiB → 2 GiB | [`phase2_qmp_inflate_ok.log`](phase2_qmp_inflate_ok.log) | `actual` falls from `3221225472` to `actual=2147483648 target=2147483648` |
| Deflate 2 GiB → 3 GiB | [`phase2_qmp_deflate_ok.log`](phase2_qmp_deflate_ok.log) | `actual` rises to `actual=3221225472 target=3221225472` |
| Full smoke test (inflate + deflate) | [`phase3_pressure_run.log`](phase3_pressure_run.log) | `BEGIN INFLATE LOG`, `BEGIN DEFLATE LOG`, ends with `smoke_phase2: completed (real QMP path)` |
| Early smoke run | [`phase2_smoke_ok.log`](phase2_smoke_ok.log) | Convergence at 2 GiB with `ack_seq` reaching `cmd_seq` |
| Replay / duplicate-command guard | [`phaseB_replay_guard.log`](phaseB_replay_guard.log) | `target unchanged and no pending cmd ..., skipping publish` |
| ivshmem transport (both directions) | [`phaseC_ivshmem_transport_ok.log`](phaseC_ivshmem_transport_ok.log) | Host→guest `PHASEC_OK_1234`, guest→host `GUEST_TO_HOST_OK` |
| Shared-memory command/ack | [`phaseC_ivshmem_daemon_run.log`](phaseC_ivshmem_daemon_run.log), [`phaseC_ivshmem_contract_run.log`](phaseC_ivshmem_contract_run.log), [`phaseC_ivshmem_contract_run2.log`](phaseC_ivshmem_contract_run2.log) | `cmd_seq` increments on a new target and `ack_seq` catches up |
| Pressure-triggered deflate | [`phaseD_pressure_dmesg_fresh.log`](phaseD_pressure_dmesg_fresh.log) | Guest `dmesg`: `vballoon_lab: pressure deflate (free < 1536 MB)` |
| Inflate under memory pressure | [`phaseD_pressure_run.log`](phaseD_pressure_run.log) | Host-side view while the pressure guard limits inflation |

## Reading a host log line

```text
actual=2147483648 target=2147483648 cmd_seq=24 ack_seq=24 shm_status=0 shm_err=0 qmp_err=0
```

- `actual` / `target`: balloon size reported by QEMU (`query-balloon`) and the requested size, in bytes.
- `cmd_seq` / `ack_seq`: host command sequence and guest acknowledgement sequence in shared memory.
- `shm_status`, `shm_err`, `qmp_err`: `0` means no error.

Use QMP `actual` as the measure of convergence. The guest `shm_agent` acknowledges commands in
shared memory, but the balloon target itself is applied through QMP and the virtio-balloon device
(see [`../docs/STATUS.md`](../docs/STATUS.md)).
