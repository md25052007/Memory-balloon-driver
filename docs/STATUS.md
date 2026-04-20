# Project Status

## Objective
Add host-daemon + shared-memory control path on top of a working virtio balloon driver baseline.

## Implemented
1. Shared-memory protocol v1
   - `magic`, `version`, `target_bytes`, `actual_bytes`
   - `cmd_seq`, `ack_seq`, `status`, `last_error`
2. Host daemon (`host/balloond`)
   - creates/maps shared memory
   - publishes target updates
   - runtime target updates (interactive when terminal attached)
   - non-interactive mode for automation scripts
3. Guest bridge agent (`guest/shm_agent`)
   - maps shared memory
   - detects new `cmd_seq`
   - updates `actual_bytes`
   - acknowledges with `ack_seq`
4. Automation scripts
   - `scripts/start_balloond.sh`
   - `scripts/run_qemu_phase2.sh`
   - `scripts/smoke_phase2.sh`

## Validation Result
- `scripts/smoke_phase2.sh` completes successfully.
- Command/ack flow confirmed:
  - host updates `target_bytes` and increments `cmd_seq`
  - guest updates `actual_bytes` and sets `ack_seq = cmd_seq`
- No crashes in MVP flow.

## Known Scope Limitations
1. Guest shared-memory bridge is userspace (`shm_agent`) MVP.
2. Shared-memory protocol is local prototype, not yet integrated with full vhost-user backend.
3. Shared-memory control is not yet wired directly into the kernel driver loop.
4. Host-side adaptive policy logic is still minimal and planned for future work.

## Next Recommended Work
1. Replace/merge userspace bridge with kernel-driver-integrated control path.
2. Add a dedicated host policy module and structured logging enhancements.
3. Add robust error handling and restart recovery.
4. Add memory-pressure signals and statistics path (closer alignment to detailed guide).
5. Add vhost-user backend integration if required by final architecture.

## 2026-04-19 17:23 IST - Smoke Test Pass
- Command: ./scripts/smoke_phase2.sh | tee proofs/phase2_smoke_ok.log
- Result: PASS
- Observed sync: cmd_seq=10, ack_seq=10
- Observed memory: target=2147483648, actual=2147483648

### 2026-04-19 20:02 IST (Real QMP Inflate)
- Command: `timeout 15s ./host/balloond/balloond 2147483648 /home/maithreya/virtio-balloon/logs/qmp.sock < /dev/null | tee proofs/phase2_qmp_inflate_ok.log`
- Result: PASS
- Observed transition: `actual` moved from `3221225472` down to `2147483648`
- Observed sync at steady state: `cmd_seq=5`, `ack_seq=5`

### 2026-04-19 20:04 IST (Real QMP Deflate)
- Command: `timeout 15s ./host/balloond/balloond 3221225472 /home/maithreya/virtio-balloon/logs/qmp.sock < /dev/null | tee proofs/phase2_qmp_deflate_ok.log`
- Result: PASS
- Observed transition: `actual` moved from `2147483648` up to `3221225472`
- Observed sync at steady state: `cmd_seq=6`, `ack_seq=6`

### 2026-04-19 20:32 IST (Explicit Full-Log Smoke on Real QMP Path)
- Command: ./scripts/smoke_phase2.sh (full cat output enabled)
- Result: PASS
- Inflate transition observed clearly:
  - start: actual=3221225472, target=2147483648
  - converged: actual=2147483648, ack_seq=9
- Deflate transition observed clearly:
  - start: actual=2170552320, target=3221225472
  - converged: actual=3221225472, ack_seq=10

### Interpretation Note (Important)
- In pressure_min_free_mb=1536 run, not reaching 2147483648 is expected.
- Reason: driver policy prioritizes pressure-triggered deflate when free memory is below threshold.
- So target-following is intentionally overridden by pressure logic in that mode.
- This run proves pressure-signal behavior.
- Separate run with pressure_min_free_mb=128 proves normal target convergence (inflate/deflate baseline).

### 2026-04-19 23:53 IST (Phase A Stabilization Final Pass)
- Pre-state set command:
  - `timeout 10s ./host/balloond/balloond 3221225472 /home/maithreya/virtio-balloon/logs/qmp.sock < /dev/null`
- Validation command:
  - `./scripts/smoke_phase2.sh | tee proofs/phaseA_stabilize_run2.log`
- Result: PASS
- Inflate transition:
  - start: `actual=3221225472`, `target=2147483648`, `cmd_seq=20`, `ack_seq=19`
  - converged: `actual=2147483648`, `target=2147483648`, `ack_seq=20`
- Deflate transition:
  - start: `actual=2147483648`, `target=3221225472`, `cmd_seq=21`, `ack_seq=20`
  - converged: `actual=3221225472`, `target=3221225472`, `ack_seq=21`
- Notes:
  - pressure path remained enabled (`pressure_enable=1`) with tuned baseline threshold (`pressure_min_free_mb=128`)
  - script diagnostics and strict convergence checks are active

### 2026-04-20 02:04 IST (Phase B Replay Guard Validation)
- Command:
  - timeout 8s ./host/balloond/balloond 3221225472 /home/maithreya/virtio-balloon/logs/qmp.sock < /dev/null | tee proofs/phaseB_replay_guard.log
- Result: PASS
- Evidence:
  - daemon logged: target unchanged and no pending cmd ... skipping publish
  - cmd_seq remained 25
  - ack_seq remained 25
- Interpretation:
  - replay-safe no-op behavior is working (no unnecessary command publish).

### 2026-04-20 02:xx IST (Phase C Transport Proof via ivshmem)
- QEMU `ivshmem-plain` was attached successfully via QMP (`object-add` + `device_add`) using a pre-created PCIe root port.
- Guest detection:
  - `01:00.0 RAM memory [0500]: Red Hat, Inc. Inter-VM shared memory [1af4:1110]`
- BAR mapping evidence:
  - `/sys/bus/pci/devices/0000:01:00.0/resource2` (1 MiB) present.
- Host -> Guest proof:
  - Host wrote marker `PHASEC_OK_1234` into `/dev/shm/balloon_ivshmem.bin`.
  - Guest read same marker by `mmap()` on `resource2`.
- Guest -> Host proof:
  - Guest wrote marker `GUEST_TO_HOST_OK` into mapped BAR offset.
  - Host readback from `/dev/shm/balloon_ivshmem.bin` matched.
- Proof:
  - `proofs/phaseC_ivshmem_transport_ok.log`

Interpretation:
- Real shared-memory transport visible to guest is now proven.
- This completes Phase C transport foundation; next substep is wiring control fields onto this region for daemon/driver path.

### 2026-04-20 03:57 IST (Phase C Contract over ivshmem: PASS)
- Host shared-memory backend switched to file-backed ivshmem region: /dev/shm/balloon_ivshmem.bin.
- Guest shm_agent switched from POSIX shm to PCI BAR mmap path:
  - /sys/bus/pci/devices/0000:01:00.0/resource2
- Validation command:
  - timeout 12s ./host/balloond/balloond 3221225472 /home/maithreya/virtio-balloon/logs/qmp.sock < /dev/null | tee proofs/phaseC_ivshmem_contract_run2.log
- Observed sequence:
  - publish: cmd_seq=4, initial ack_seq=3
  - ack catch-up: ack_seq=4
  - convergence: actual=3221225472, target=3221225472
- Result: PASS (daemon<->guest command/ack is flowing over ivshmem transport)

### 2026-04-20 IST (Phase D Pressure Revalidation after Phase C)
- Host run command:
  - timeout 20s ./host/balloond/balloond 2147483648 /home/maithreya/virtio-balloon/logs/qmp.sock < /dev/null | tee proofs/phaseD_pressure_run.log
- Guest evidence command:
  - ssh -p 2222 ubuntu@127.0.0.1 'sudo dmesg -T | grep -Ei "vballoon_lab|pressure deflate|BUG:|Oops|Call Trace"' | tee proofs/phaseD_pressure_dmesg_fresh.log
- Result: PASS
- Notes:
  - guest dmesg timestamps are guest clock/timezone based
  - host command runtime (IST) is used as report reference
- Proof files:
  - proofs/phaseD_pressure_run.log
  - proofs/phaseD_pressure_dmesg_fresh.log

