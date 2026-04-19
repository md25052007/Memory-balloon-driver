# Phase 2 Status

## Objective
Add host-daemon + shared-memory control path on top of working Phase 1 virtio balloon driver baseline.

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
3. `policy.c` and `log.c` contain placeholder structure for future expansion.
4. Not yet wiring shared-memory control directly into kernel driver loop.

## Phase 3 (Recommended)
1. Replace/merge userspace bridge with kernel-driver-integrated control path.
2. Add host policy engine (`policy.c`) and structured logging (`log.c`).
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
