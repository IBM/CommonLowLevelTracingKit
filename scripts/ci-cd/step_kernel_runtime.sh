#!/usr/bin/env bash
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

# CI Step: kernel-module RUNTIME test on a STOCK distro kernel.
#
# The clltk kernel module registers tracing at load. Stock distro kernels are
# built WITHOUT CONFIG_CONSTRUCTORS, so the constructor-based path never runs -
# registration has to come from the module notifier. This boots the modules in
# QEMU on the distro kernel and checks that tracing actually registers (trace
# files created, no invalid-offset errors). It is the regression gate for the
# notifier init path.
#
# No kernel build: the distro kernel-core package ships a bootable image and
# kernel-devel the matching build tree. Runs under TCG, so no KVM is required.
# Exit: 0 = tracing works on the stock kernel, 1 = it does not.

set -euo pipefail
ROOT_PATH=$(git rev-parse --show-toplevel)
cd "${ROOT_PATH}"

echo "========================================"
echo "CI Step: Kernel Runtime (stock kernel)"
echo "========================================"

ARCH=$(uname -m)
case "$ARCH" in
    x86_64) QEMU_PKG=qemu-system-x86-core ;;
    aarch64) QEMU_PKG=qemu-system-aarch64-core ;;
    *) echo "unsupported arch $ARCH"; exit 1 ;;
esac
dnf -y install kernel-core kernel-devel kernel-modules-core cpio zstd busybox "$QEMU_PKG" >/dev/null

# Pick a version that has both a bootable image and a matching build tree.
KVER=""
for v in $(rpm -q --qf '%{version}-%{release}.%{arch}\n' kernel-core | sort -V -r); do
    [ -f "/lib/modules/$v/vmlinuz" ] && [ -d "/usr/src/kernels/$v" ] && { KVER=$v; break; }
done
[ -n "$KVER" ] || { echo "no kernel-core/kernel-devel pair found"; exit 1; }
KSRC="/usr/src/kernels/$KVER"
CTORS=$(grep -c '^CONFIG_CONSTRUCTORS=y' "$KSRC/.config" || true)
echo "kernel $KVER (CONFIG_CONSTRUCTORS=y count: $CTORS -- 0 means the stock, notifier-only case)"

# Kernel image. On x86 the distro vmlinuz is a bootable bzImage QEMU loads
# directly. On arm64 it is an EFI zboot (zstd) wrapper QEMU cannot load, so
# extract the raw Image from the zboot payload (offset/size from its header).
KIMG="/lib/modules/$KVER/vmlinuz"
if [ "$ARCH" != x86_64 ]; then
    python3 - "$KIMG" /tmp/clltk_Image.zst <<'PY'
import sys
d = open(sys.argv[1], "rb").read()
x = d.find(b"zimg")
if x < 0:
    raise SystemExit("not an EFI zboot image")
payload_offset = int.from_bytes(d[x + 4:x + 8], "little")
payload_size = int.from_bytes(d[x + 8:x + 12], "little")
open(sys.argv[2], "wb").write(d[payload_offset:payload_offset + payload_size])
PY
    zstd -qdf /tmp/clltk_Image.zst -o /tmp/clltk_Image
    KIMG=/tmp/clltk_Image
fi

echo "Building the kernel modules against $KVER..."
# cd instead of make -C: the module Makefile derives repo paths from $(PWD).
( cd kernel_tracing_library/src && make KERNEL_SRC="$KSRC" modules >/dev/null )
make -C "$KSRC" M="$ROOT_PATH/examples/simple_kernel_module" \
    KBUILD_EXTRA_SYMBOLS="$ROOT_PATH/kernel_tracing_library/src/Module.symvers" modules >/dev/null

# Minimal initramfs: busybox init loads the modules and reports the result.
IRD=$(mktemp -d)
mkdir -p "$IRD"/{bin,proc,sys,tmp}
cp "$(command -v busybox)" "$IRD/bin/busybox"
cp kernel_tracing_library/src/clltk_kernel_tracing.ko "$IRD/clltk.ko"
cp examples/simple_kernel_module/simple_tracing_test.ko "$IRD/example.ko"
cat > "$IRD/init" <<'INIT'
#!/bin/busybox sh
export PATH=/bin
busybox mount -t proc proc /proc
busybox mount -t sysfs sysfs /sys
busybox mount -t tmpfs tmpfs /tmp
busybox echo "GUEST_UP $(busybox uname -r)"
busybox insmod /clltk.ko tracing_path=/tmp/
busybox insmod /example.ko
busybox sleep 3
N=$(busybox ls /tmp/*.clltk_ktrace 2>/dev/null | busybox wc -l)
INV=$(busybox dmesg | busybox grep -c "invalid in_file_offset")
busybox echo "RESULT traces=$N invalid=$INV"
if [ "$N" -gt 0 ] && [ "$INV" -eq 0 ]; then
    busybox echo STOCK_KERNEL_TRACE_OK
else
    busybox echo STOCK_KERNEL_TRACE_FAIL
fi
busybox poweroff -f
INIT
chmod +x "$IRD/init"
( cd "$IRD" && find . | cpio -o -H newc 2>/dev/null | gzip ) > /tmp/clltk_initramfs.gz

if [ "$ARCH" = x86_64 ]; then
    QEMU=(qemu-system-x86_64 -M pc)
    CONSOLE=ttyS0
    [ -w /dev/kvm ] && QEMU+=(-enable-kvm -cpu host) || QEMU+=(-cpu max)
else
    QEMU=(qemu-system-aarch64 -M virt -cpu max)
    CONSOLE=ttyAMA0
fi

echo "Booting the stock kernel in QEMU..."
timeout 420 "${QEMU[@]}" -m 1G -smp 4 -kernel "$KIMG" -initrd /tmp/clltk_initramfs.gz \
    -append "console=$CONSOLE panic=1 rdinit=/init" -nographic -no-reboot > /tmp/clltk_boot.log 2>&1 || true

grep -aE "RESULT|STOCK_KERNEL_TRACE" /tmp/clltk_boot.log | tail -3 || true
if grep -aq STOCK_KERNEL_TRACE_OK /tmp/clltk_boot.log; then
    echo "PASSED: kernel tracing registers and traces on a stock kernel"
    exit 0
fi
echo "FAILED: no traces on the stock kernel"
tail -25 /tmp/clltk_boot.log
exit 1
