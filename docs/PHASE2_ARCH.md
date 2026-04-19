# Phase 2 Architecture (Host Daemon + Shared Memory)

## Goal
Move balloon target control from manual QMP-only flow to a host daemon that writes desired target/commands into shared memory.

## Components
- host/balloond: host daemon process.
- guest/vballoon_lab: guest kernel balloon driver (already working Phase 1).
- guest/shm_agent (optional): userspace bridge if needed for prototyping shared-memory control.

## Phase 2 MVP
1. Shared-memory region with a simple control struct.
2. Host daemon updates target_bytes and cmd_seq.
3. Guest side observes updates and applies inflate/deflate.
4. Guest side reports actual_bytes, status, and error fields.
5. End-to-end test confirms daemon command changes balloon behavior.
