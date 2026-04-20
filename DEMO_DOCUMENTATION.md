# Memory Balloon Driver - Demo & Reproduction Documentation

This document is the exact runbook to reproduce the submitted project results from the code folder.

## 1. What To Show In Demo

In the live demo, show these 5 checkpoints:

1. Guest custom balloon driver (`vballoon_lab`) is loaded and bound to `virtio0`.
2. Host daemon (`balloond`) controls guest memory target through QMP.
3. Shared-memory command/ack flow works (`cmd_seq` and `ack_seq` behavior).
4. Inflate and deflate both converge (`2 GiB` and `3 GiB`).
5. Pressure-signal behavior appears in guest `dmesg` (`pressure deflate`).

## 2. Important Path Requirement

Scripts in this repo assume project path is:

```bash
~/virtio-balloon
```

So after extracting/cloning, place folder at that exact location.

Mandatory check:

```bash
cd ~
test -d ~/virtio-balloon && echo "OK: folder is in home" || echo "MOVE_REQUIRED"
```

If your folder is currently somewhere else, move it into home and rename it to `virtio-balloon`:

```bash
mv /path/to/your/current/folder ~/virtio-balloon
```

If you clone from GitHub and the default folder name is `Memory-balloon-driver`, do one of these:

Option A (recommended): clone directly into the expected folder name

```bash
cd ~
git clone https://github.com/md25052007/Memory-balloon-driver.git virtio-balloon
```

Option B: rename after cloning

```bash
cd ~
git clone https://github.com/md25052007/Memory-balloon-driver.git
mv Memory-balloon-driver virtio-balloon
```

## 3. Environment Requirements (Host)

Host OS: Ubuntu 24.04+ (or compatible Linux with QEMU + KVM/TCG).

Install prerequisites:

```bash
sudo apt update
sudo apt install -y \
  qemu-system-x86 qemu-utils ovmf socat cloud-image-utils \
  build-essential git make flex bison bc \
  libelf-dev libssl-dev libncurses-dev dwarves pahole cpio rsync
```

## 4. One-Time VM Image Setup

From host:

```bash
mkdir -p ~/virtio-balloon/images ~/virtio-balloon/logs
cd ~/virtio-balloon/images

wget -O ubuntu-24.04-server-cloudimg-amd64.img \
  https://cloud-images.ubuntu.com/noble/current/noble-server-cloudimg-amd64.img
qemu-img resize ubuntu-24.04-server-cloudimg-amd64.img +20G

cat > user-data << 'EOF'
#cloud-config
users:
  - name: ubuntu
    sudo: ALL=(ALL) NOPASSWD:ALL
    lock_passwd: false
    plain_text_passwd: ubuntu
    shell: /bin/bash
ssh_pwauth: true
chpasswd:
  expire: false
EOF

cat > meta-data << 'EOF'
instance-id: balloon-tcg
local-hostname: balloon-guest
EOF

cloud-localds seed.iso user-data meta-data
```

### Reproducibility note (important)

For consistent demo results across different laptops:
- always use the exact cloud image and `seed.iso` generation steps above;
- do not reuse an old or modified VM disk from another setup;
- if in doubt, recreate the image and `seed.iso` before running the demo.

## 5. Demo Execution (Terminal-by-Terminal)

Use **3 terminals**:
- Terminal A: QEMU console
- Terminal B: Host control commands
- Terminal C: Optional verification/log viewing

### Step A1: Start QEMU (Terminal A)

```bash
cd ~/virtio-balloon
./scripts/run_qemu_phase2.sh
```

Keep Terminal A running.

### Step B1: Prepare guest workspace via SSH (Terminal B)

If the VM was recreated and SSH shows `REMOTE HOST IDENTIFICATION HAS CHANGED`, clear stale host keys first:

```bash
ssh-keygen -f "$HOME/.ssh/known_hosts" -R "[127.0.0.1]:2222"
ssh-keygen -f "$HOME/.ssh/known_hosts" -R "127.0.0.1"
```

```bash
cd ~/virtio-balloon
ssh -p 2222 ubuntu@127.0.0.1
```

In guest shell:

```bash
mkdir -p ~/virtio-balloon
exit
```

Back on host shell (Terminal B), copy guest sources into guest VM:

```bash
scp -P 2222 -r ~/virtio-balloon/guest ubuntu@127.0.0.1:/home/ubuntu/virtio-balloon/
```

### Step B2: Build and load guest kernel module (Terminal B)

```bash
ssh -p 2222 ubuntu@127.0.0.1 '
set -e
HDR="linux-headers-$(uname -r)"
MISSING=""
for p in build-essential "$HDR"; do
  dpkg -s "$p" >/dev/null 2>&1 || MISSING="$MISSING $p"
done

if [ -n "$MISSING" ]; then
  echo "Missing packages:$MISSING"
  sudo apt-get update
  sudo DEBIAN_FRONTEND=noninteractive apt-get install -y $MISSING
else
  echo "All required packages already installed."
fi

cd ~/virtio-balloon/guest/vballoon_lab
make clean
make -j"$(nproc)"
sudo rmmod vballoon_lab 2>/dev/null || true
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

Expected output:
- `/sys/bus/virtio/drivers/vballoon_lab`

What this output means:
- Your custom kernel module is loaded.
- The `virtio0` balloon device is bound to your module, not the default `virtio_balloon` driver.

If output is different:
- Rerun Step B2 once.
- Then verify with:

```bash
ssh -p 2222 ubuntu@127.0.0.1 'readlink -f /sys/bus/virtio/devices/virtio0/driver'
```

### Step B3: Shared-memory agent note (important)

For this default demo flow (`./scripts/run_qemu_phase2.sh`), `shm_agent` is **not required**.
Reason: this boot mode does not expose ivshmem transport, so shared-memory agent activity is not part of core smoke verification.

Use `shm_agent` only in ivshmem boot mode (`./scripts/run_qemu_phase3_ivshmem.sh`) when you specifically want to demonstrate host<->guest shared-memory transport behavior.

### Step B4: Run host-side full smoke (inflate + deflate) (Terminal B)

Temporarily disable pressure so inflate+deflate convergence is deterministic for smoke capture:

```bash
ssh -p 2222 ubuntu@127.0.0.1 '
echo 0 | sudo tee /sys/module/vballoon_lab/parameters/pressure_enable >/dev/null
echo 128 | sudo tee /sys/module/vballoon_lab/parameters/pressure_min_free_mb >/dev/null
'
```

```bash
cd ~/virtio-balloon
./scripts/smoke_phase2.sh 2>&1 | tee proofs/phase3_pressure_run.log
```

Expected markers in output:
- `----- BEGIN INFLATE LOG -----`
- `----- BEGIN DEFLATE LOG -----`
- `smoke_phase2: completed (real QMP path)`

What these lines mean:
- `BEGIN INFLATE LOG`: host requests lower guest memory target and guest starts reclaiming pages.
- `BEGIN DEFLATE LOG`: host requests higher guest memory target and guest returns pages.
- completion line: both directions ran in one smoke pass.

What to do if `BEGIN DEFLATE LOG` is missing:
- Pressure likely interfered with convergence.
- Keep pressure disabled as shown above, then rerun Step B4.

After smoke capture, restore pressure settings for pressure-behavior validation:

```bash
ssh -p 2222 ubuntu@127.0.0.1 '
echo 1 | sudo tee /sys/module/vballoon_lab/parameters/pressure_enable >/dev/null
echo 1536 | sudo tee /sys/module/vballoon_lab/parameters/pressure_min_free_mb >/dev/null
'
```

### Step B5: Optional shared-memory contract logs (ivshmem mode only)

Run this only if you booted with `./scripts/run_qemu_phase3_ivshmem.sh`.
If you are on `run_qemu_phase2.sh`, skip this step.

```bash
cd ~/virtio-balloon
timeout 12s ./host/balloond/balloond 2147483648 "$HOME/virtio-balloon/logs/qmp.sock" < /dev/null 2>&1 | tee proofs/phaseC_ivshmem_contract_run.log
timeout 12s ./host/balloond/balloond 3221225472 "$HOME/virtio-balloon/logs/qmp.sock" < /dev/null 2>&1 | tee proofs/phaseC_ivshmem_contract_run2.log
```

Expected patterns:
- first run: `target=2147483648`
- second run: `new target_bytes=3221225472`
- both: `ack_seq=` present

What this output means:
- `cmd_seq` represents command publish by host.
- `ack_seq` represents command acknowledgement by guest side.
- matching progression shows command lifecycle is healthy.

### Step B6: Memory-pressure behavior proof (Terminal B)

Set stronger pressure threshold and rerun:

```bash
ssh -p 2222 ubuntu@127.0.0.1 '
echo 1 | sudo tee /sys/module/vballoon_lab/parameters/pressure_enable >/dev/null
echo 1536 | sudo tee /sys/module/vballoon_lab/parameters/pressure_min_free_mb >/dev/null
'

cd ~/virtio-balloon
timeout 20s ./host/balloond/balloond 2147483648 "$HOME/virtio-balloon/logs/qmp.sock" < /dev/null 2>&1 | tee proofs/phaseD_pressure_run.log
ssh -p 2222 ubuntu@127.0.0.1 'sudo dmesg -T | grep -Ei "vballoon_lab|pressure deflate|BUG:|Oops|Call Trace"' \
  2>&1 | tee proofs/phaseD_pressure_dmesg_fresh.log
```

Expected:
- multiple `pressure deflate` lines in `proofs/phaseD_pressure_dmesg_fresh.log`

What this output means:
- guest free-memory threshold triggered pressure reaction;
- driver chooses safer path by deflating balloon under pressure.

What to do if no `pressure deflate` lines appear:
- confirm parameter values:

```bash
ssh -p 2222 ubuntu@127.0.0.1 '
echo -n "pressure_enable="; cat /sys/module/vballoon_lab/parameters/pressure_enable
echo -n "pressure_min_free_mb="; cat /sys/module/vballoon_lab/parameters/pressure_min_free_mb
'
```

## 6. Quick Validation Commands

```bash
cd ~/virtio-balloon

# Check key proof files are non-empty
for f in \
  proofs/phaseB_replay_guard.log \
  proofs/phaseC_ivshmem_contract_run.log \
  proofs/phaseC_ivshmem_contract_run2.log \
  proofs/phaseC_ivshmem_daemon_run.log \
  proofs/phaseD_pressure_run.log \
  proofs/phaseD_pressure_dmesg_fresh.log \
  proofs/phase3_pressure_run.log
 do
  ls -lh "$f"
done

# Check phase3 full smoke sections
grep -q "BEGIN INFLATE LOG" proofs/phase3_pressure_run.log && echo "inflate OK"
grep -q "BEGIN DEFLATE LOG" proofs/phase3_pressure_run.log && echo "deflate OK"
```

Interpretation:
- non-empty proof files mean logs were captured correctly;
- `inflate OK` + `deflate OK` means both transitions exist in the main smoke proof.

## 7. Troubleshooting

### SSH host key changed (`REMOTE HOST IDENTIFICATION HAS CHANGED`)
This happens when the guest VM image is recreated and old SSH fingerprints are still cached on host.

```bash
ssh-keygen -f "$HOME/.ssh/known_hosts" -R "[127.0.0.1]:2222"
ssh-keygen -f "$HOME/.ssh/known_hosts" -R "127.0.0.1"
ssh -p 2222 ubuntu@127.0.0.1
```

When prompted, type `yes` to trust the new key.

### QMP socket missing
If you see `QMP socket not found`:

```bash
cd ~/virtio-balloon
./scripts/run_qemu_phase2.sh
```

### smoke script stops after inflate
This means pressure settings are too aggressive for convergence. Use:

```bash
ssh -p 2222 ubuntu@127.0.0.1 '
echo 0 | sudo tee /sys/module/vballoon_lab/parameters/pressure_enable >/dev/null
echo 128 | sudo tee /sys/module/vballoon_lab/parameters/pressure_min_free_mb >/dev/null
'
```

Then rerun `./scripts/smoke_phase2.sh`.

### `ack_seq` stays old (for example, remains `12`)
If `ack_seq` does not advance in a run, usually one of these is true:
- You are in `run_qemu_phase2.sh` mode (no ivshmem transport), so `shm_agent` is not relevant.
- You are in ivshmem mode but `shm_agent` is not running.

For default smoke flow, ignore `shm_agent` and use temporary pressure disable/restore steps in **Step B4**.

### Guest SSH password rejected
Cloud-init default in this project is:
- user: `ubuntu`
- password: `ubuntu`

If changed, regenerate `images/seed.iso` from Section 4 and reboot QEMU.

### First-run package installation output is very long
On a fresh guest image, `build-essential` and dependencies may not be installed.
The package-check logic in Step B2 installs only missing packages, but first-time install can still print a long list of `Get:` and `Unpacking` lines. This is expected.

### Boot shows `/dev/disk/by-label/BOOT` or `/dev/disk/by-label/UEFI` timeout
If boot logs show repeated timeout/dependency failures for `by-label/BOOT` or `by-label/UEFI`, the guest image likely has stale `/etc/fstab` entries from another environment.

Preferred fix (best reproducibility):

```bash
cd ~/virtio-balloon/images
rm -f ubuntu-24.04-server-cloudimg-amd64.img seed.iso
wget -O ubuntu-24.04-server-cloudimg-amd64.img https://cloud-images.ubuntu.com/noble/current/noble-server-cloudimg-amd64.img
qemu-img resize ubuntu-24.04-server-cloudimg-amd64.img +20G
cloud-localds seed.iso user-data meta-data
```

Alternative repair on current guest:

```bash
ssh -p 2222 ubuntu@127.0.0.1 '
sudo cp /etc/fstab /etc/fstab.bak
sudo sed -i "/by-label\\/BOOT/s/^/# /; /by-label\\/UEFI/s/^/# /" /etc/fstab
'
```

Then reboot/restart VM and continue from Step A1.

## 8. Final Notes

- This submission demonstrates dynamic virtio balloon inflate/deflate with real QMP control.
- Shared-memory contract and ivshmem transport are demonstrated in the included proofs.
- Current architecture still uses guest userspace `shm_agent` bridge for shared-memory update path; kernel-direct SHM integration is documented as future extension.


