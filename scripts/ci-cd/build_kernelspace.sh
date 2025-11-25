#!/usr/bin/env bash
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

set -e
ROOT_PATH=$(git rev-parse --show-toplevel)
cd ${ROOT_PATH}

kernel_latest_query_version () {
    echo "=> query linux kernel version" >&2
    local version=$(curl https://www.kernel.org/ 2>/dev/null | grep -A 1 "latest_link" 2>/dev/null )
    version=${version##*.tar.xz\">}
    version=${version%</a>}
    echo "$version"
} 

kernel_query_server_prefix () {
    version=$1
    prefix=""
    case $version in 
        5*)
            prefix="https://kernel.org/pub/linux/kernel/v5.x/"
        ;;
        6*)
            prefix="https://kernel.org/pub/linux/kernel/v6.x/"
        ;;
        *)
            echo "EE kernel version $version unsupported" >&2
            exit 1
        ;;
    esac
    echo "$prefix"
}

kernel_latest_query_link () {
    version=$1
    echo "=> query linux kernel download link" >&2
    prefix=$(kernel_query_server_prefix $version)
    local download_link=$(curl -L $prefix 2>/dev/null | grep -A 1 "linux-$version.tar.xz")
    if [ -z "$download_link" ]; then
        echo "no download link for linux-$version" >&2
        exit 1
    fi
    echo "$prefix/linux-$version.tar.xz"
} 


download_link () {
    local url=$1
    local target=$2
    echo "=> download $url > $target" >&2
    curl -L -aqn -C - $url -o $target
}


create_rootfs () {
    local prefix=$1
    cd $prefix
    echo "=> create rootfs > $prefix" >&2
    mkdir -p ./{dev,proc,bin,etc,lib,lib/modules,usr,tmp,sbin,sys,usr/bin,usr/sbin,traces,tests}
    
    ln -s lib usr/lib
    ln -s lib lib64
    ln -s lib usr/lib64
    
    BUSYBOX=$(whereis busybox | cut -d" " -f2)
    cp $BUSYBOX ./sbin/busybox
    cd -
}


extract () {
    local archive=$1
    local target=$2

    echo "=> extracting $archive > $target" >&2
    tar xfk $archive -C $target
}


config_kernel() {
    local KERNEL_DIR=$1
    echo "=> config kernel > $KERNEL_DIR" >&2
    cd $KERNEL_DIR
    time make clean
    time make mrproper
    time make defconfig
    ./scripts/config --set-val CONFIG_STRICT_DEVMEM n
    ./scripts/config --set-val CONFIG_HARDENED_USERCOPY n
    ./scripts/config --set-val CONFIG_HAVE_HARDENED_USERCOPY_ALLOCATOR n
    ./scripts/config --set-val CONFIG_USB_SUPPORT n
    ./scripts/config --set-val CONFIG_NETDEVICES n
    ./scripts/config --set-val CONFIG_DRM n
    ./scripts/config --set-val CONFIG_VIDEO_DEV n
    ./scripts/config --set-val CONFIG_EXT2_FS n
    ./scripts/config --set-val CONFIG_EXT3_FS n
    ./scripts/config --set-val CONFIG_EXT4_FS n
    ./scripts/config --set-val CONFIG_NET n
    ./scripts/config --set-val CONFIG_HW_RANDOM n
    ./scripts/config --set-val CONFIG_SPI n
    ./scripts/config --set-val CONFIG_DEBUG_FS n
    ./scripts/config --set-val CONFIG_DEBUG_INFO_REDUCED n
    ./scripts/config --set-val CONFIG_X86_PKG_TEMP_THERMAL n
    ./scripts/config --set-val CONFIG_EFIVAR_FS n
    ./scripts/config --set-val CONFIG_DEBUG_INFO y
    ./scripts/config --set-val CONFIG_DEBUG_INFO_DWARF4 y
    ./scripts/config --set-val CONFIG_KALLSYMS y
    ./scripts/config --set-val CONFIG_KALLSYMS_ALL y
    ./scripts/config --set-val CONFIG_MODULES y
    ./scripts/config --set-val CONFIG_VFAT_FS y
    ./scripts/config --set-val CONFIG_PROC_FS y
    ./scripts/config --set-val CONFIG_PCI y
    ./scripts/config --set-val CONFIG_GDB_SCRIPTS y
    ./scripts/config --set-val CONFIG_I2C y
    ./scripts/config --set-val CONFIG_I2C_CHARDEV y
    ./scripts/config --set-val CONFIG_I2C_MUX y
    ./scripts/config --set-val CONFIG_I2C_DEBUG_CORE y
    ./scripts/config --set-val CONFIG_I2C_DEBUG_ALGO y
    ./scripts/config --set-val CONFIG_I2C_DEBUG_BUS y
    ./scripts/config --set-val CONFIG_GCOV_KERNEL y
    ./scripts/config --set-val CONFIG_FRAME_POINTER y
    ./scripts/config --set-val CONFIG_FRAME_POINTER y
    
    time make olddefconfig
    time make prepare    
    time make scripts_gdb
    cd -
}

build_kernel() {
    local KERNEL_DIR=$1
    echo "=> build kernel > $KERNEL_DIR" >&2
    cd $KERNEL_DIR

    make -j $(($(nproc)-1))
    cd -
}

install_kernel() {
    local KERNEL_DIR=$1
    local INSTALL_PATH=$2
    local INSTALL_MOD_PATH=$2
    cd $KERNEL_DIR
    time make INSTALL_PATH=$2 INSTALL_MOD_PATH=$2 install
    time make INSTALL_PATH=$2 INSTALL_MOD_PATH=$2 modules_install
    cd -
}

help() {
echo -e "Usage: $0 [OPTION]
Options:

    GENERAL:
    -h          help
    -t [TMPDIR] cache all TMP files in this <dir>
    -d [DIR]    put build and test in this <dir>
    -l          use LLVM

    KERNEL:
    -k [KVER]   force set kernel to KVER(default latest)

    MODULE
    -b          force rebuild module
    -s [MODULE_SRC]  module source is here
    
    TEST
    -m [MODULENAME] prepare test for MODULE
    -r          run qemu test

n\
Example: $1 -t /tmp/frank_sinatra/ -m sample -m foo -k 5.10.215 -r -s ./sample -s ../other_module/ 
\tcache all data in /tmp/frank_sinatra/ prepare modprobe for sample and foo run qemu\n"
}


flag_force_module_rebuild=false
flag_force_kernel_update=false
flag_exec_qemu=false

MODULES=()
MODULES_SRC=()
CC=${CC:-"gcc"}
# is is needed for older kernel if compiled with gcc newer then 12
export EXTRA_CFLAGS="-Werror=use-after-free -Werror=format -Werror=array-bounds"

unset LLVM

while getopts "lhbrt:k:m:s:d:" opt
do
   case $opt in
       h) help; exit 0;;
       l) LLVM=1;;
       b) flag_force_module_rebuild=true ;;
       r) flag_exec_qemu=true ;;
       s) MODULE_SRC+=($OPTARG);;
       m) MODULES+=($OPTARG);;
       t) PERSITENT_ARTIFACTS=$OPTARG;;
       k) KVERSION=$OPTARG;;
       d) BUILDDIR=$OPTARG;;
       *) help; exit 1;;
   esac
done

PERSITENT_ARTIFACTS="${PERSITENT_ARTIFACTS:-"./build_kernel/persistent"}"
CACHED_PREFIX="$PERSITENT_ARTIFACTS/kernel"
DOWNLOAD_FOLDER="$CACHED_PREFIX"

BUILDDIR="${BUILDDIR:-"./build_kernel/kernel"}"
mkdir -p $BUILDDIR
RUNTIME_PREFIX="$(realpath -m "$BUILDDIR")/runtime"
ROOTFS_FOLDER="$RUNTIME_PREFIX/rootfs"
TRACES_FOLDER="$RUNTIME_PREFIX/traces"
TESTS_FOLDER="$RUNTIME_PREFIX/tests"

# if no version is set looku newest version
if [ -z $KVERSION ]; then
    KVERSION=$(kernel_latest_query_version)
fi

KDIR="$CACHED_PREFIX/linux-$KVERSION"

# check if we have the right kernel version
# if we dont have it, delete all and rebuild the kernel
if [ ! -d "$KDIR" ]; then
    rm -rf $DOWNLOAD_FOLDER/linux*
    rm -rf $KDIR
    rm -rf $ROOTFS_FOLDER
    flag_force_kernel_update=true;
fi

# create env
mkdir -p $PERSITENT_ARTIFACTS
mkdir -p $DOWNLOAD_FOLDER
mkdir -p $ROOTFS_FOLDER
mkdir -p $TRACES_FOLDER
mkdir -p $TESTS_FOLDER
rm -rf $TRACES_FOLDER/*

# download kernel
if ($flag_force_kernel_update); then
    # query server 
    link=$(kernel_latest_query_link $KVERSION)
    download_link "$link" "$DOWNLOAD_FOLDER/linux.tar.xz"
fi

# extract kernel
if ($flag_force_kernel_update); then
    rm -rf $KDIR
    extract $DOWNLOAD_FOLDER/linux.tar.xz $CACHED_PREFIX/
    rm -rf $DOWNLOAD_FOLDER/linux.tar.xz
else
    echo "=> reuse kernel source > $KDIR" >&2
fi

# build kernel
if ($flag_force_kernel_update); then
    config_kernel "$KDIR"
    build_kernel "$KDIR"
else
    echo "=> reuse kernel > $KDIR/vmlinux" >&2
fi

# create rootfs
if [ ! -f "$ROOTFS_FOLDER/sbin/busybox" ]; then
    create_rootfs "$ROOTFS_FOLDER"
    install_kernel "$KDIR" "$ROOTFS_FOLDER" "$ROOTFS_FOLDER"
else
    echo "=> reuse rootfs > $ROOTFS_FOLDER" >&2
fi
 


for module_path in "${MODULE_SRC[@]}"; do
    cd "$module_path"

    # clean module
    if ($flag_force_module_rebuild); then
        echo "make -C $KDIR M=$PWD clean"
        make -C $KDIR M=$PWD clean
    fi

    make -C $KDIR M=$PWD 
    make -C $KDIR M=$PWD INSTALL_MOD_PATH=$ROOTFS_FOLDER modules_install
    cd -
    export KBUILD_EXTRA_SYMBOLS="$KBUILD_EXTRA_SYMBOLS $(realpath $module_path/Module.symvers)"
done


cat > $ROOTFS_FOLDER/init << EOF
#!/sbin/busybox sh
/sbin/busybox echo "mounting /proc"
/sbin/busybox mount -t proc proc /proc
/sbin/busybox echo "mounting /sys"
/sbin/busybox mount -t sysfs sysfs /sys
/sbin/busybox mount -t tmpfs tmpfs /tmp
/sbin/busybox mount /dev/sdc1 /tests
/sbin/busybox mount /dev/sdb1 /traces
/sbin/busybox echo "SYSTEM BOOTED"
/sbin/busybox echo 8 > /proc/sys/kernel/printk
/sbin/busybox cat /proc/cpuinfo
EOF


# execute qemu
if [ "${#MODULES[@]}" -ne "0" ]; then
cat >> $ROOTFS_FOLDER/init << EOF
modules="${MODULES[@]}"
EOF
fi

cat >> $ROOTFS_FOLDER/init << EOF
for mod in \$modules; do
    /sbin/busybox echo "=================="
    /sbin/busybox echo "== LOADING MODULE: \$mod"
    /sbin/busybox echo "=================="
    /sbin/busybox modprobe \$mod
    /sbin/busybox echo "=================="
done

/sbin/busybox cp -Rf /tmp/* /traces

/sbin/busybox echo "SYSTEM SHUTDOWN"
/sbin/busybox reboot -f

EOF

# remove symlinks into kernel sources so qemu can handle everything
rm -rf $ROOTFS_FOLDER/lib/modules/$KVERSION/source
rm -rf $ROOTFS_FOLDER/lib/modules/$KVERSION/build

if ($flag_exec_qemu); then
    qemu-system-x86_64 \
    -smp $(nproc) \
    -nographic \
    -no-reboot \
    -m 1G  \
    -drive file=fat:rw:$ROOTFS_FOLDER \
    -drive file=fat:rw:$TRACES_FOLDER,cache=none \
    -drive file=fat:rw:$TESTS_FOLDER,cache=none \
    -append " init=/init panic=1 console=ttyS0 root=/dev/sda1 " \
    -kernel $KDIR/arch/x86_64/boot/bzImage

    DECODER="$ROOT_PATH/decoder_tool/python/clltk_decoder.py"
    cmd="$DECODER $TRACES_FOLDER"
    echo format traces with: $cmd
    eval $cmd

fi

