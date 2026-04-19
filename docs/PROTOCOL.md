# Shared Memory Protocol (Phase 2 MVP)

## Version
- version = 1

## Layout (packed)
- u32 magic = 0x42414C4E (BALN)
- u16 version
- u16 flags
- u64 target_bytes
- u64 actual_bytes
- u64 cmd_seq
- u64 ack_seq
- u32 status
- u32 last_error

## Semantics
- Host daemon writes target_bytes, increments cmd_seq.
- Guest side applies requested target and updates actual_bytes.
- Guest side sets ack_seq = cmd_seq after processing.
- status=0 means OK; non-zero indicates error path.
