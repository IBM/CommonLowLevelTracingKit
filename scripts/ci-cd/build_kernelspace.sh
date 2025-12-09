#!/usr/bin/env bash
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
# Kernel module test script.
# 
# This script builds and tests kernel modules in an isolated QEMU environment.
# It downloads kernel source and builds it once (cached), then uses that for
# module compilation and testing.
#
# The kernel build is cached, so subsequent runs are fast (~1-2 min).
# First run requires full kernel build (~20-30 min).

set -e
ROOT_PATH=$(git rev-parse --show-toplevel)
cd "${ROOT_PATH}"

# ============================================================================
# Helper Functions
# ============================================================================

log() { echo "=> $*" >&2; }
err() { echo "EE $*" >&2; exit 1; }

help() {
    cat << 'EOF'
Usage: build_kernelspace.sh [OPTIONS]

Builds and tests kernel modules in an isolated QEMU environment.

OPTIONS:
    -h              Show this help
    -k VERSION      Kernel version (default: 6.6)
    -t DIR          Cache directory for kernel artifacts (persistent)
    -d DIR          Build directory (can be ephemeral)
    -s MODULE_SRC   Module source directory (can be repeated)
    -m MODULE       Module name to load (can be repeated)
    -b              Force rebuild modules
    -r              Run QEMU test
    -l              Use LLVM/Clang

EXAMPLES:
    # Build and test with default kernel
    ./build_kernelspace.sh -r \
        -s ./kernel_tracing_library/src \
        -s ./examples/simple_kernel_module \
        -m clltk_kernel_tracing \
        -m simple_kernel_module

    # Test with specific kernel version
    ./build_kernelspace.sh -k 6.1 -r -s ./kernel_tracing_library/src -m clltk_kernel_tracing

NOTES:
    - First run builds the kernel (~20-30 min), subsequent runs are fast (~1-2 min)
    - Use -t to specify a persistent cache directory to preserve kernel builds
    - The script requires: curl, tar, make, gcc, qemu-system-x86_64, busybox, cpio
EOF
}

# ============================================================================
# Kernel Download Functions (using pre-built kernels)
# ============================================================================

get_kernel_major() {
    echo "${1%%.*}"
}

download_kernel_artifacts() {
    local version="$1"
    local cache_dir="$2"
    local major
    major=$(get_kernel_major "$version")

    log "Downloading kernel $version artifacts"

    mkdir -p "$cache_dir"

    # Download kernel source
    local kernel_url="https://cdn.kernel.org/pub/linux/kernel/v${major}.x/linux-${version}.tar.xz"
    local kernel_archive="$cache_dir/linux-${version}.tar.xz"
    local kernel_dir="$cache_dir/linux-${version}"

    if [[ ! -d "$kernel_dir" ]]; then
        if [[ ! -f "$kernel_archive" ]]; then
            log "Downloading kernel source from $kernel_url"
            curl -L --progress-bar -o "$kernel_archive" "$kernel_url" || err "Failed to download kernel"
        fi
        log "Extracting kernel source"
        tar -xf "$kernel_archive" -C "$cache_dir"
        rm -f "$kernel_archive"
    else
        log "Reusing cached kernel source: $kernel_dir"
    fi

    echo "$kernel_dir"
}

prepare_kernel() {
    local kernel_dir="$1"

    log "Preparing kernel for module building"
    cd "$kernel_dir"

    # Check if kernel is already fully built (has Module.symvers)
    if [[ -f "Module.symvers" ]] && [[ -f "arch/x86/boot/bzImage" ]]; then
        log "Kernel already built, reusing cached build"
        cd - > /dev/null
        return
    fi

    if [[ ! -f ".config" ]]; then
        log "Creating kernel configuration"
        make defconfig

        # Required for CLLTK kernel module
        ./scripts/config --set-val CONFIG_MODULES y
        ./scripts/config --set-val CONFIG_MODULE_UNLOAD y
        ./scripts/config --set-val CONFIG_KALLSYMS y
        ./scripts/config --set-val CONFIG_KALLSYMS_ALL y
        ./scripts/config --set-val CONFIG_PROC_FS y
        ./scripts/config --set-val CONFIG_TMPFS y
        ./scripts/config --set-val CONFIG_VFAT_FS y
        ./scripts/config --set-val CONFIG_FAT_FS y
        ./scripts/config --set-val CONFIG_NLS y
        ./scripts/config --set-val CONFIG_NLS_CODEPAGE_437 y
        ./scripts/config --set-val CONFIG_NLS_ISO8859_1 y

        # Required for QEMU boot
        ./scripts/config --set-val CONFIG_SERIAL_8250 y
        ./scripts/config --set-val CONFIG_SERIAL_8250_CONSOLE y
        ./scripts/config --set-val CONFIG_VIRTIO y
        ./scripts/config --set-val CONFIG_VIRTIO_PCI y
        ./scripts/config --set-val CONFIG_VIRTIO_BLK y
        ./scripts/config --set-val CONFIG_ATA y
        ./scripts/config --set-val CONFIG_ATA_PIIX y
        ./scripts/config --set-val CONFIG_BLK_DEV_SD y

        # Disable unnecessary features for faster build
        ./scripts/config --set-val CONFIG_DEBUG_INFO n
        ./scripts/config --set-val CONFIG_NET n
        ./scripts/config --set-val CONFIG_SOUND n
        ./scripts/config --set-val CONFIG_USB_SUPPORT n
        ./scripts/config --set-val CONFIG_DRM n
        ./scripts/config --set-val CONFIG_WIRELESS n
        ./scripts/config --set-val CONFIG_WLAN n

        make olddefconfig
    fi

    # Build the kernel - this generates Module.symvers needed for external modules
    # Note: modules_prepare does NOT generate Module.symvers, a full build is required
    log "Building kernel (first run only, will be cached)..."
    log "This may take 20-30 minutes depending on your system"
    make -j"$(nproc)"

    cd - > /dev/null
}

# ============================================================================
# Module Build Functions
# ============================================================================

generate_version_header() {
    log "Generating version.gen.h"
    
    local version_script="$ROOT_PATH/cmake/gen_version_header.sh"
    local version_template="$ROOT_PATH/cmake/version.h.template"
    local version_output="$ROOT_PATH/tracing_library/include/CommonLowLevelTracingKit/version.gen.h"
    
    if [[ -f "$version_script" ]]; then
        "$version_script" -t "$version_template" -o "$version_output"
    else
        err "Version script not found: $version_script"
    fi
}

build_modules() {
    local kernel_dir="$1"
    local force_rebuild="$2"
    shift 2
    local module_sources=("$@")

    # Generate version header before building
    generate_version_header

    local extra_symbols=""

    for module_path in "${module_sources[@]}"; do
        local abs_path
        abs_path=$(realpath "$module_path")
        log "Building module: $abs_path"

        cd "$abs_path"

        if [[ "$force_rebuild" == "true" ]]; then
            make -C "$kernel_dir" M="$PWD" clean
        fi

        make -C "$kernel_dir" M="$PWD" ${extra_symbols:+KBUILD_EXTRA_SYMBOLS="$extra_symbols"} -j"$(nproc)"

        # Accumulate symbols for dependent modules
        if [[ -f "$abs_path/Module.symvers" ]]; then
            extra_symbols="${extra_symbols:+$extra_symbols }$abs_path/Module.symvers"
        fi

        cd - > /dev/null
    done
}

# ============================================================================
# QEMU Test Functions
# ============================================================================

create_initramfs() {
    local initrd_dir="$1"
    local traces_dir="$2"
    shift 2
    local modules=("$@")

    log "Creating minimal initramfs"

    mkdir -p "$initrd_dir"/{bin,lib/modules,proc,sys,tmp,traces}

    # Copy busybox
    local busybox
    busybox=$(command -v busybox) || err "busybox not found"
    cp "$busybox" "$initrd_dir/bin/"

    # Copy kernel modules
    for mod in "${modules[@]}"; do
        if [[ -f "$mod" ]]; then
            cp "$mod" "$initrd_dir/lib/modules/"
        fi
    done

    # Create init script
    cat > "$initrd_dir/init" << 'INIT_EOF'
#!/bin/busybox sh
export PATH=/bin

busybox echo "==> Mounting filesystems"
busybox mount -t proc proc /proc
busybox mount -t sysfs sysfs /sys
busybox mount -t tmpfs tmpfs /tmp

# Mount the FAT drive for trace output (shared with host)
busybox echo "==> Mounting trace output drive"
busybox mount -t vfat /dev/sda1 /traces 2>/dev/null || \
busybox mount -t vfat /dev/sda /traces 2>/dev/null || \
busybox echo "Warning: Could not mount trace drive, using tmpfs"
busybox mount -t tmpfs tmpfs /traces 2>/dev/null || true

busybox echo "==> System info"
busybox cat /proc/version
busybox echo "CPUs: $(busybox nproc)"

busybox echo "==> Loading modules"
INIT_EOF

    # Add module loading commands
    for mod_path in "$initrd_dir"/lib/modules/*.ko; do
        local mod_name
        mod_name=$(basename "$mod_path" .ko)
        if [[ "$mod_name" == "clltk_kernel_tracing" ]]; then
            echo "busybox insmod /lib/modules/${mod_name}.ko tracing_path=/tmp/" >> "$initrd_dir/init"
        else
            echo "busybox insmod /lib/modules/${mod_name}.ko" >> "$initrd_dir/init"
        fi
    done

    cat >> "$initrd_dir/init" << 'INIT_EOF'

busybox echo "==> Waiting for tracing to complete"
busybox sleep 3

busybox echo "==> Trace files in /tmp:"
busybox ls -la /tmp/*.clltk_ktrace 2>/dev/null || busybox echo "No trace files found"

busybox echo "==> Copying traces to output drive"
busybox cp /tmp/*.clltk_ktrace /traces/ 2>/dev/null || true
busybox sync

busybox echo "==> Unloading modules"
INIT_EOF

    # Add module unloading in reverse order
    for mod_path in "$initrd_dir"/lib/modules/*.ko; do
        local mod_name
        mod_name=$(basename "$mod_path" .ko)
        if [[ "$mod_name" != "clltk_kernel_tracing" ]]; then
            echo "busybox rmmod $mod_name 2>/dev/null || true" >> "$initrd_dir/init"
        fi
    done
    echo "busybox rmmod clltk_kernel_tracing 2>/dev/null || true" >> "$initrd_dir/init"

    cat >> "$initrd_dir/init" << 'INIT_EOF'

busybox echo "==> Listing final trace files"
busybox ls -la /traces/

# Count trace files and report
TRACE_COUNT=$(busybox ls /traces/*.clltk_ktrace 2>/dev/null | busybox wc -l)
if [ "$TRACE_COUNT" -gt 0 ]; then
    busybox echo "KERNEL_TEST_PASSED: $TRACE_COUNT trace file(s) created"
else
    busybox echo "KERNEL_TEST_FAILED: No trace files created"
fi

busybox echo "==> Shutdown"
busybox poweroff -f
INIT_EOF

    chmod +x "$initrd_dir/init"

    # Create cpio archive
    (cd "$initrd_dir" && find . | cpio -o -H newc 2>/dev/null | gzip) > "$initrd_dir.gz"
    echo "$initrd_dir.gz"
}

run_qemu_test() {
    local bzimage="$1"
    local initrd="$2"
    local traces_dir="$3"
    local timeout_sec="${4:-120}"

    log "Running QEMU test (timeout: ${timeout_sec}s)"

    local qemu_log="$traces_dir/qemu.log"

    # Use QEMU's FAT drive feature to share traces directory with guest
    # This allows the guest to write directly to the host filesystem
    # The traces_dir must exist and be writable
    mkdir -p "$traces_dir/guest_traces"

    # Run QEMU with FAT drive for trace output (same approach as original script)
    timeout "$timeout_sec" qemu-system-x86_64 \
        -kernel "$bzimage" \
        -initrd "$initrd" \
        -append "console=ttyS0 panic=1" \
        -nographic \
        -no-reboot \
        -m 1G \
        -smp "$(nproc)" \
        -drive file=fat:rw:"$traces_dir/guest_traces",format=raw \
        ${KVM_AVAILABLE:+-enable-kvm} \
        2>&1 | tee "$qemu_log" || true

    # Move traces from guest_traces to traces_dir
    if ls "$traces_dir/guest_traces"/*.clltk_ktrace &>/dev/null 2>&1; then
        mv "$traces_dir/guest_traces"/*.clltk_ktrace "$traces_dir/" 2>/dev/null || true
    fi

    # Check results
    if grep -q "KERNEL_TEST_PASSED" "$qemu_log"; then
        log "QEMU test PASSED"
        return 0
    else
        log "QEMU test FAILED - check $qemu_log for details"
        return 1
    fi
}

# ============================================================================
# Main
# ============================================================================

# Check for KVM support
if [[ -w /dev/kvm ]]; then
    KVM_AVAILABLE=1
    log "KVM acceleration available"
else
    log "KVM not available, using emulation (slower)"
fi

# Default values
KVERSION="6.6"
CACHE_DIR="./build_kernel/cache"
BUILD_DIR="./build_kernel/build"
MODULE_SOURCES=()
MODULES=()
FORCE_REBUILD=false
RUN_QEMU=false

# Parse arguments
while getopts "hlbrk:t:d:s:m:" opt; do
    case $opt in
        h) help; exit 0 ;;
        l) export LLVM=1 ;;
        b) FORCE_REBUILD=true ;;
        r) RUN_QEMU=true ;;
        k) KVERSION="$OPTARG" ;;
        t) CACHE_DIR="$OPTARG" ;;
        d) BUILD_DIR="$OPTARG" ;;
        s) MODULE_SOURCES+=("$OPTARG") ;;
        m) MODULES+=("$OPTARG") ;;
        *) help; exit 1 ;;
    esac
done

# Create directories
mkdir -p "$CACHE_DIR" "$BUILD_DIR"
CACHE_DIR=$(realpath "$CACHE_DIR")
BUILD_DIR=$(realpath "$BUILD_DIR")
TRACES_DIR="$BUILD_DIR/traces"
mkdir -p "$TRACES_DIR"
rm -rf "$TRACES_DIR"/*

log "Kernel version: $KVERSION"
log "Cache directory: $CACHE_DIR"
log "Build directory: $BUILD_DIR"

# Step 1: Get kernel source and build kernel (generates Module.symvers)
KERNEL_DIR=$(download_kernel_artifacts "$KVERSION" "$CACHE_DIR")
prepare_kernel "$KERNEL_DIR"

# Step 2: Build modules
if [[ ${#MODULE_SOURCES[@]} -gt 0 ]]; then
    build_modules "$KERNEL_DIR" "$FORCE_REBUILD" "${MODULE_SOURCES[@]}"
fi

# Step 3: Run QEMU test
if [[ "$RUN_QEMU" == "true" ]]; then
    # Use kernel image from our build (prepare_kernel already built it)
    BZIMAGE="$KERNEL_DIR/arch/x86/boot/bzImage"
    if [[ ! -f "$BZIMAGE" ]]; then
        err "Kernel image not found: $BZIMAGE"
    fi

    # Collect .ko files
    KO_FILES=()
    for src in "${MODULE_SOURCES[@]}"; do
        for ko in "$src"/*.ko; do
            [[ -f "$ko" ]] && KO_FILES+=("$ko")
        done
    done

    if [[ ${#KO_FILES[@]} -eq 0 ]]; then
        err "No .ko files found to test"
    fi

    # Create initramfs and run test
    INITRD_DIR="$BUILD_DIR/initramfs"
    rm -rf "$INITRD_DIR"
    INITRD=$(create_initramfs "$INITRD_DIR" "$TRACES_DIR" "${KO_FILES[@]}")

    if run_qemu_test "$BZIMAGE" "$INITRD" "$TRACES_DIR"; then
        # Decode traces
        DECODER="$ROOT_PATH/decoder_tool/python/clltk_decoder.py"
        if [[ -f "$DECODER" ]]; then
            log "Decoding traces"
            # Find trace files from QEMU log (they're printed there)
            # For now, try to decode any .clltk_ktrace files we can find
            if ls "$TRACES_DIR"/*.clltk_ktrace &>/dev/null; then
                python3 "$DECODER" "$TRACES_DIR" -o "$TRACES_DIR/decoded.txt" || log "Decoder warning (non-fatal)"
                log "Decoded output: $TRACES_DIR/decoded.txt"
            else
                log "No trace files to decode (traces are inside QEMU)"
            fi
        fi
        log "SUCCESS: Kernel test completed"
    else
        err "FAILED: Kernel test failed"
    fi
fi

log "Done"
