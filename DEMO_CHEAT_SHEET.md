# Demo Cheat Sheet (Minimal)

Use this exact order during demo.

- Terminal A: QEMU console
- Terminal B: Host commands
- Terminal C: Optional monitor

## Part 1: Phase2 Balloon Behavior

### [A] Start phase2 VM
```bash
cd ~/virtio-balloon
./scripts/run_qemu_phase2.sh
```

### [B] If SSH host key changed (run once)
```bash
ssh-keygen -f "$HOME/.ssh/known_hosts" -R "[127.0.0.1]:2222"
ssh-keygen -f "$HOME/.ssh/known_hosts" -R "127.0.0.1"
```

### [B] Ensure guest code exists in VM
```bash
ssh -p 2222 ubuntu@127.0.0.1 'mkdir -p ~/virtio-balloon'
scp -P 2222 -r ~/virtio-balloon/guest ubuntu@127.0.0.1:/home/ubuntu/virtio-balloon/
```

### [B] Build + load `vballoon_lab` and bind to `virtio0`
```bash
ssh -p 2222 ubuntu@127.0.0.1 '
set -e
cd ~/virtio-balloon/guest/vballoon_lab
sudo rmmod vballoon_lab 2>/dev/null || true
make clean && make -j"$(nproc)"
sudo insmod ./vballoon_lab.ko pressure_enable=1 pressure_min_free_mb=128 pressure_log_interval_ms=1000
DEV=virtio0
CUR=$(basename "$(readlink -f /sys/bus/virtio/devices/$DEV/driver 2>/dev/null || echo none)")
if [ "$CUR" = "virtio_balloon" ]; then
  echo $DEV | sudo tee /sys/bus/virtio/drivers/virtio_balloon/unbind >/dev/null
  echo $DEV | sudo tee /sys/bus/virtio/drivers/vballoon_lab/bind >/dev/null
fi
readlink -f /sys/bus/virtio/devices/$DEV/driver
'
```

### [B] Disable pressure for deterministic smoke
```bash
ssh -p 2222 ubuntu@127.0.0.1 '
echo 0 | sudo tee /sys/module/vballoon_lab/parameters/pressure_enable >/dev/null
echo 128 | sudo tee /sys/module/vballoon_lab/parameters/pressure_min_free_mb >/dev/null
'
```

### [B] Run inflate+deflate smoke proof
```bash
cd ~/virtio-balloon
./scripts/smoke_phase2.sh 2>&1 | tee proofs/phase3_pressure_run.log
```

### [B] Re-enable pressure and capture pressure proof
```bash
ssh -p 2222 ubuntu@127.0.0.1 '
echo 1 | sudo tee /sys/module/vballoon_lab/parameters/pressure_enable >/dev/null
echo 1536 | sudo tee /sys/module/vballoon_lab/parameters/pressure_min_free_mb >/dev/null
'
timeout 20s ./host/balloond/balloond 2147483648 "$HOME/virtio-balloon/logs/qmp.sock" < /dev/null 2>&1 | tee proofs/phaseD_pressure_run.log
ssh -p 2222 ubuntu@127.0.0.1 'sudo dmesg -T | grep -Ei "vballoon_lab|pressure deflate|BUG:|Oops|Call Trace"' 2>&1 | tee proofs/phaseD_pressure_dmesg_fresh.log
```

## Part 2: ivshmem Contract Demo

### [A] Stop phase2 QEMU, then start ivshmem VM
```bash
cd ~/virtio-balloon
./scripts/run_qemu_phase3_ivshmem.sh
```

### [B] Ensure guest code (safe repeat)
```bash
ssh -p 2222 ubuntu@127.0.0.1 'mkdir -p ~/virtio-balloon'
scp -P 2222 -r ~/virtio-balloon/guest ubuntu@127.0.0.1:/home/ubuntu/virtio-balloon/
```

### [B] Confirm ivshmem device exists in guest
```bash
ssh -p 2222 ubuntu@127.0.0.1 'lspci -Dnn | grep -Ei "1af4:1110|Inter-VM shared memory"'
```

### [B] Build/start `shm_agent` in guest
```bash
ssh -p 2222 ubuntu@127.0.0.1 '
set -e
cd ~/virtio-balloon/guest/shm_agent
make clean && make
sudo pkill -f "/guest/shm_agent/shm_agent" 2>/dev/null || true
sudo nohup ~/virtio-balloon/guest/shm_agent/shm_agent >/tmp/shm_agent_ivshmem.log 2>&1 &
pgrep -af "/guest/shm_agent/shm_agent" || true
sudo tail -n 20 /tmp/shm_agent_ivshmem.log || true
'
```

### [B] Sanity check host-side sockets/files
```bash
cd ~/virtio-balloon
ls -l "$HOME/virtio-balloon/logs/qmp.sock"
ls -l /dev/shm/balloon_ivshmem.bin
```

### [B] Contract runs: 2GiB then 3GiB (`cmd_seq`/`ack_seq`)
```bash
timeout 12s ./host/balloond/balloond 2147483648 "$HOME/virtio-balloon/logs/qmp.sock" < /dev/null 2>&1 | tee proofs/phaseC_ivshmem_contract_run.log
timeout 12s ./host/balloond/balloond 3221225472 "$HOME/virtio-balloon/logs/qmp.sock" < /dev/null 2>&1 | tee proofs/phaseC_ivshmem_contract_run2.log
```

### [B] Replay/no-op proof
```bash
timeout 8s ./host/balloond/balloond 3221225472 "$HOME/virtio-balloon/logs/qmp.sock" < /dev/null 2>&1 | tee proofs/phaseB_replay_guard.log
```
