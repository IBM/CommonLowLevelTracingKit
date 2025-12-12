FROM quay.io/fedora/fedora-minimal:43 as ci
RUN echo "build base-layers"

USER root

RUN dnf -y update && \
    dnf -y upgrade

RUN dnf -y install \
    git \
    git-lfs

RUN dnf -y install \
    cmake \
    make \
    gcc \
    g++ \
    clang-tools-extra \
    valgrind \
    lcov \
    gcovr \
    libasan \
    rpmdevtools

# Static analysis tools
RUN dnf -y install \
    cppcheck \
    jq

RUN dnf -y install \
    file \
    rsync \
    gettext \
    wget \
    unzip \
    openssl

RUN dnf -y install \
    python3 \
    python3-pip

RUN dnf -y install \
    zlib-ng-compat-devel \
    libtar-devel \
    cli11-devel \
    rapidjson-devel \
    libffi-devel \
    libarchive-devel \
    boost-devel \
    java-21-openjdk-devel

# clang-format specific version may need to be installed via LLVM repo or built from source.
# Assuming clang-tools-extra provides clang-format; adjust if specific version is needed.
RUN update-alternatives --install /usr/bin/clang-format clang-format /usr/bin/clang-format 100

RUN pip3 install \
    numpy \
    pandas

RUN \
    echo "[safe]" >> ~/.gitconfig &&\
    echo "    directory = *" >> ~/.gitconfig &&\
    echo ""

FROM ci as kernel
RUN echo "now build kernel-layers"

RUN dnf -y install \
    curl \
    rpm-build \
    elfutils \
    clang \
    lld \
    llvm \
    elfutils-libelf-devel \
    ccache \
    @development-tools \
    flex \
    bison \
    bc \
    busybox \
    strace \
    cpio \
    qemu-system-x86 \
    ncurses-devel \
    openssl-devel \
    openssl-devel-engine \
    kmod

FROM kernel as dev
RUN echo "now build dev-layers"

RUN dnf -y install \
    ninja-build \
    gdb \
    lldb \
    perf \
    npm \
    bash-completion

RUN pip3 install \
    scipy \
    matplotlib
