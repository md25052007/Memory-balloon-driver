# Phase 3 Plan

## Goal
Move from Phase 2 shared-memory MVP bridge to production-style integration aligned with virtio balloon detailed guide concepts.

## Work Items

1. Kernel integration of shared-memory control
- Replace userspace-only bridge logic with kernel-driver integration path.
- Driver should consume target updates and publish actual/ack/error safely.

2. Host daemon structure completion
- Implement `policy.c` for target decisions (manual + scripted modes).
- Implement `log.c` for structured event logs.

3. Reliability and recovery
- Handle daemon restart, stale shared memory, and command replays.
- Ensure idempotent command handling with `cmd_seq/ack_seq`.

4. Observability
- Add status metrics and debug counters.
- Improve test visibility in logs and docs.

5. Memory-pressure and guide alignment
- Add path toward pressure-aware behavior (stats + reaction hooks).
- Prepare for shrinker/OOM/stat-style extensions.

6. End-to-end testing matrix
- Functional target transitions.
- Repeated command stress.
- Restart/recovery scenarios.
- No crash/no leak criteria.

## Deliverables
- Updated driver + daemon code
- Updated scripts
- `tests/phase3_e2e.md`
- `PHASE3_STATUS.md`
