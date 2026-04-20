# Memory Balloon Driver (virtio-style) - Final Project README

This repository implements a virtio-style memory balloon project with host-side control, guest-side ballooning, shared-memory command contract, and pressure-triggered behavior.

## 0) Folder Name Requirement

Most scripts assume the project is located at:

```bash
~/virtio-balloon
```

If you clone from GitHub, use one of these:

```bash
# clone directly into expected path
cd ~
git clone https://github.com/md25052007/Memory-balloon-driver.git virtio-balloon

# OR rename after clone
cd ~
git clone https://github.com/md25052007/Memory-balloon-driver.git
mv Memory-balloon-driver virtio-balloon
```

## 1) Project Goal

Problem statement target:
- Dynamically **inflate/deflate** memory available to a VM.
- Reclaim guest pages on inflate (`alloc_page()` path).
- Return guest pages on deflate (`__free_page()` path).
- Communicate with host daemon through shared-memory contract.
- Demonstrate VM memory pressure behavior.

## 2) What Is Implemented

### A. Guest kernel balloon driver (`guest/vballoon_lab/vballoon_lab.c`)
- Registers as a `virtio_balloon`-ID driver.
- Maintains ballooned page lists and inflight queues.
- Inflates by allocating pages and publishing PFNs to virtqueue.
- Deflates by freeing ballooned pages back to guest.
- Exposes pressure controls as module params:
  - `pressure_enable`
  - `pressure_min_free_mb`
  - `pressure_log_interval_ms`

### B. Host daemon (`host/balloond`)
- Publishes target memory command (`target_bytes`, `cmd_seq`) in shared memory.
- Sends balloon target to QEMU via real QMP `balloon` command.
- Polls QMP `query-balloon` for actual bytes.
- Emits runtime telemetry including:
  - `actual`, `target`, `cmd_seq`, `ack_seq`, `shm_status`, `shm_err`, `qmp_err`.

### C. Shared-memory protocol contract
- Protocol in [docs/PROTOCOL.md](/Users/donth/Desktop/OS_PRO/repo_audit_clone/docs/PROTOCOL.md)
- Fields:
  - `magic`, `version`, `target_bytes`, `actual_bytes`, `cmd_seq`, `ack_seq`, `status`, `last_error`
- Strict ownership:
  - host writes `target_bytes`, `cmd_seq`
  - guest side writes `actual_bytes`, `ack_seq`, `status`, `last_error`

### D. Shared-memory transport proof
- ivshmem transport validated with host->guest and guest->host marker exchange.
- Proof file: [proofs/phaseC_ivshmem_transport_ok.log](/Users/donth/Desktop/OS_PRO/repo_audit_clone/proofs/phaseC_ivshmem_transport_ok.log)

### E. Pressure behavior proof
- Pressure-triggered deflate observed repeatedly in guest `dmesg`.
- Proof file: [proofs/phaseD_pressure_dmesg_fresh.log](/Users/donth/Desktop/OS_PRO/repo_audit_clone/proofs/phaseD_pressure_dmesg_fresh.log)

## 3) Key Observed Outputs (from proofs)

### Inflate convergence (3 GiB -> 2 GiB)
From [proofs/phase2_qmp_inflate_ok.log](/Users/donth/Desktop/OS_PRO/repo_audit_clone/proofs/phase2_qmp_inflate_ok.log):
- starts around `actual=3221225472`
- converges to `actual=2147483648 target=2147483648`
- `ack_seq` catches up to `cmd_seq`

### Deflate convergence (2 GiB -> 3 GiB)
From [proofs/phase2_qmp_deflate_ok.log](/Users/donth/Desktop/OS_PRO/repo_audit_clone/proofs/phase2_qmp_deflate_ok.log):
- starts around `actual=2147483648 target=3221225472`
- converges to `actual=3221225472 target=3221225472`
- `ack_seq` catches up to `cmd_seq`

### Replay/no-op guard
From [proofs/phaseB_replay_guard.log](/Users/donth/Desktop/OS_PRO/repo_audit_clone/proofs/phaseB_replay_guard.log):
- `target unchanged and no pending cmd ..., skipping publish`
- confirms duplicate publish suppression.

### Full smoke with both sections
From [proofs/phase3_pressure_run.log](/Users/donth/Desktop/OS_PRO/repo_audit_clone/proofs/phase3_pressure_run.log):
- contains `BEGIN INFLATE LOG`
- contains `BEGIN DEFLATE LOG`
- ends with `smoke_phase2: completed (real QMP path)`

## 4) Repository Layout

- Host daemon code: [host/balloond/src/main.c](/Users/donth/Desktop/OS_PRO/repo_audit_clone/host/balloond/src/main.c), [host/balloond/src/shm.c](/Users/donth/Desktop/OS_PRO/repo_audit_clone/host/balloond/src/shm.c), [host/balloond/src/qmp.c](/Users/donth/Desktop/OS_PRO/repo_audit_clone/host/balloond/src/qmp.c)
- Guest kernel module: [guest/vballoon_lab/vballoon_lab.c](/Users/donth/Desktop/OS_PRO/repo_audit_clone/guest/vballoon_lab/vballoon_lab.c)
- Guest shared-memory agent (bridge): [guest/shm_agent/main.c](/Users/donth/Desktop/OS_PRO/repo_audit_clone/guest/shm_agent/main.c)
- QEMU scripts:
  - [scripts/run_qemu_phase2.sh](/Users/donth/Desktop/OS_PRO/repo_audit_clone/scripts/run_qemu_phase2.sh)
  - [scripts/run_qemu_phase3_ivshmem.sh](/Users/donth/Desktop/OS_PRO/repo_audit_clone/scripts/run_qemu_phase3_ivshmem.sh)
- Smoke script: [scripts/smoke_phase2.sh](/Users/donth/Desktop/OS_PRO/repo_audit_clone/scripts/smoke_phase2.sh)
- TA runbook: [TA_DEMO_DOCUMENTATION.md](/Users/donth/Desktop/OS_PRO/repo_audit_clone/TA_DEMO_DOCUMENTATION.md)

## 5) Fast Demo Flow

Use the TA runbook directly:
- [TA_DEMO_DOCUMENTATION.md](/Users/donth/Desktop/OS_PRO/repo_audit_clone/TA_DEMO_DOCUMENTATION.md)

Short version:
1. Start VM: `./scripts/run_qemu_phase2.sh`
2. Build/load guest module and bind to `virtio0`
3. Temporarily disable pressure for deterministic smoke
4. Run smoke: `./scripts/smoke_phase2.sh 2>&1 | tee proofs/phase3_pressure_run.log`
5. Re-enable pressure and capture pressure-deflate evidence

## 6) Why `policy.c` Exists

File: [host/balloond/src/policy.c](/Users/donth/Desktop/OS_PRO/repo_audit_clone/host/balloond/src/policy.c)

It is a reserved extension point for host policy logic (future rules like bounds, hysteresis, pressure-aware host decisions, and adaptive targeting). In current submission, control policy is intentionally minimal and implemented in `main.c` to keep the path deterministic for demo and validation.

## 7) Current Gaps / Future Work

The project is strong for demo scope, but one major advanced integration remains:
- shared-memory control is still bridged through guest userspace `shm_agent`; kernel-direct shared-memory control integration is future work.

Also possible improvements:
- richer host policy engine in `policy.c`
- deeper kernel pressure hooks beyond threshold mode
- stronger restart/recovery automation

## 8) Final Status

- Dynamic inflate/deflate: **done**
- QMP-controlled host daemon path: **done**
- Shared-memory command/ack contract: **done**
- ivshmem transport proof: **done**
- Pressure-triggered deflate proof: **done**
- Kernel-direct shared-memory integration: **not yet done (explicitly documented)**

For submission context details, see:
- [PHASE2_STATUS.md](/Users/donth/Desktop/OS_PRO/repo_audit_clone/PHASE2_STATUS.md)
- [FINAL_SUBMISSION_STATUS.md](/Users/donth/Desktop/OS_PRO/repo_audit_clone/FINAL_SUBMISSION_STATUS.md)
