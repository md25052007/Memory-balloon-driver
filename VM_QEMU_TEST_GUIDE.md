# VM/QEMU + Guest + Virtio Balloon Driver: Full 11-Step Guide (With Explanations)

This document captures the complete workflow used for your project: host setup, guest bring-up, driver build/bind, QMP tests, validation, and git save.

---

## 1. Install Host Requirements

### Why
You need QEMU and supporting tools on the Ubuntu host VM to run the guest, control ballooning through QMP, and build/test kernel code.

### Commands
```bash
sudo apt update
sudo apt install -y \
  qemu-system-x86 qemu-utils ovmf socat cloud-image-utils \
  build-essential git make flex bison bc \
  libelf-dev libssl-dev libncurses-dev dwarves pahole cpio rsync
mkdir -p ~/virtio-balloon/{images,logs,guest,host,scripts}
```

---

## 2. Create Guest Image and Cloud-Init Seed (One-Time)

### Why
- Cloud image gives a ready Ubuntu guest disk.
- Cloud-init seed provides initial user/password for quick login.

### Commands
```bash
cd ~/virtio-balloon/images
wget -O ubuntu-24.04-server-cloudimg-amd64.img \
  https://cloud-images.ubuntu.com/noble/current/noble-server-cloudimg-amd64.img
qemu-img resize ubuntu-24.04-server-cloudimg-amd64.img +20G

cat > user-data << 'EOC'
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
package_update: true
EOC

cat > meta-data << 'EOC'
instance-id: balloon-tcg
local-hostname: balloon-guest
EOC

cloud-localds seed.iso user-data meta-data
```

---

## 3. Create Driver Source Files (`Makefile` and `vballoon_lab.c`)

### Why
This is your custom balloon-driver source directory. Module build/load depends on these files.

### Commands
```bash
mkdir -p ~/virtio-balloon/guest/vballoon_lab
cd ~/virtio-balloon/guest/vballoon_lab

cat > Makefile << 'EOM'
obj-m += vballoon_lab.o
KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
EOM
```

For `vballoon_lab.c`, keep your already tested final source (recommended).

---

## 4. Start QEMU VM

### Why
This starts the guest VM and exposes:
- SSH: `127.0.0.1:2222`
- QMP socket: `~/virtio-balloon/logs/qmp.sock`
- Virtio balloon device for your driver

### Commands
```bash
rm -f ~/virtio-balloon/logs/qmp.sock
qemu-system-x86_64 \
  -name balloon-tcg \
  -accel tcg,thread=multi \
  -machine q35 \
  -cpu max \
  -smp 2 \
  -m 3072 \
  -drive if=virtio,file=$HOME/virtio-balloon/images/ubuntu-24.04-server-cloudimg-amd64.img,format=qcow2 \
  -drive if=virtio,file=$HOME/virtio-balloon/images/seed.iso,format=raw \
  -nic user,hostfwd=tcp::2222-:22 \
  -device virtio-balloon-pci,id=balloon0,deflate-on-oom=on,free-page-reporting=on \
  -qmp unix:$HOME/virtio-balloon/logs/qmp.sock,server,nowait \
  -nographic
```

Keep this terminal running.

---

## 5. SSH into Guest

### Why
Module compilation/loading must run inside the guest kernel environment.

### Commands
```bash
ssh ubuntu@127.0.0.1 -p 2222
# password: ubuntu
```

---

## 6. Install Guest Build Requirements

### Why
Out-of-tree module build needs compiler + matching kernel headers.

### Commands
```bash
sudo apt update
sudo apt install -y build-essential linux-headers-$(uname -r)
```

---

## 7. Build and Load `vballoon_lab` Module

### Why
Compiles your custom driver and inserts it into kernel.

### Commands
```bash
cd ~/virtio-balloon/guest/vballoon_lab
make clean
make -j"$(nproc)"
sudo rmmod vballoon_lab 2>/dev/null || true
sudo insmod ./vballoon_lab.ko
```

---

## 8. Bind virtio Balloon Device to `vballoon_lab`

### Why
If stock `virtio_balloon` owns the device, your module won't receive events. Bind ensures `virtio0` uses your driver.

### Commands
```bash
DEV=virtio0
CUR=$(basename "$(readlink -f /sys/bus/virtio/devices/$DEV/driver)")
echo "current_driver=$CUR"

if [ "$CUR" = "virtio_balloon" ]; then
  echo $DEV | sudo tee /sys/bus/virtio/drivers/virtio_balloon/unbind
  echo $DEV | sudo tee /sys/bus/virtio/drivers/vballoon_lab/bind
fi

readlink -f /sys/bus/virtio/devices/$DEV/driver
```

Expected:
`/sys/bus/virtio/drivers/vballoon_lab`

---

## 9. Run QMP Balloon Tests (Host)

### Why
QMP is host-side control plane for target memory changes.

### Commands
```bash
# Query current actual
printf '%s\n' '{"execute":"qmp_capabilities"}' '{"execute":"query-balloon"}' \
| socat - UNIX-CONNECT:$HOME/virtio-balloon/logs/qmp.sock

# Inflate target = 2 GiB
printf '%s\n' '{"execute":"qmp_capabilities"}' \
'{"execute":"balloon","arguments":{"value":2147483648}}' \
| socat - UNIX-CONNECT:$HOME/virtio-balloon/logs/qmp.sock

# Poll actual repeatedly
for i in $(seq 1 24); do
  printf '%s\n' '{"execute":"qmp_capabilities"}' '{"execute":"query-balloon"}' \
  | socat - UNIX-CONNECT:$HOME/virtio-balloon/logs/qmp.sock | tail -n 1
  sleep 5
done

# Deflate target = 3 GiB
printf '%s\n' '{"execute":"qmp_capabilities"}' \
'{"execute":"balloon","arguments":{"value":3221225472}}' \
| socat - UNIX-CONNECT:$HOME/virtio-balloon/logs/qmp.sock

# Poll again
for i in $(seq 1 24); do
  printf '%s\n' '{"execute":"qmp_capabilities"}' '{"execute":"query-balloon"}' \
  | socat - UNIX-CONNECT:$HOME/virtio-balloon/logs/qmp.sock | tail -n 1
  sleep 5
done
```

Expected:
- Decrease to `2147483648` on inflate
- Increase to `3221225472` on deflate

---

## 10. Validate Guest Kernel Logs

### Why
Confirms no kernel crash / queue corruption during tests.

### Commands
```bash
sudo dmesg -T | grep -Ei "vballoon_lab|BUG:|Oops|Call Trace|inflate:id|deflate:id" | tail -n 200
```

Expected:
- `vballoon_lab: probe ok`
- No fresh `BUG/Oops/Call Trace`
- No fresh `inflate:id ... is not a head` / `deflate:id ... is not a head`

---

## 11. Stop VM, Recover if Needed, and Push to Git

### Why
Clean shutdown and preserve docs/results.

### Commands
```bash
# Stop VM from QEMU console: Ctrl+a then x
# or from host:
pkill -f qemu-system-x86_64

# Recovery if stuck/crashed/socket stale:
pkill -f qemu-system-x86_64 || true
rm -f ~/virtio-balloon/logs/qmp.sock

# Save guide to git
cd ~/virtio-balloon
git add VM_QEMU_TEST_GUIDE.md
git commit -m "Add full 11-step QEMU+guest+balloon guide with explanations"
git push
```

---

## Verified Baseline (Your Actual Result)

- Inflated from 3 GiB toward 2 GiB and reached exact target: `2147483648`
- Deflated back to exact 3 GiB target: `3221225472`
- Driver bound to: `/sys/bus/virtio/drivers/vballoon_lab`
- Final stable cycles completed without fresh kernel Oops.
