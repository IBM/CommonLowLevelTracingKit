FROM quay.io/fedora/fedora-minimal:43 as ci

LABEL org.opencontainers.image.source="https://github.com/IBM/CommonLowLevelTracingKit"
LABEL org.opencontainers.image.description="CI container for CommonLowLevelTracingKit"
LABEL org.opencontainers.image.licenses="BSD-2-Clause-Patent"

RUN echo "build base-layers"

USER root

RUN dnf -y update && \
    dnf -y upgrade

RUN dnf -y install \
    git \
    git-lfs  \
    cmake \
    make \
    gcc \
    g++ \
    clang-tools-extra \
    valgrind \
    lcov \
    gcovr \
    libasan \
    rpmdevtools \
    cppcheck \
    jq \
    file \
    rsync \
    gettext \
    wget \
    unzip \
    openssl \
    python3 \
    python3-pip \
    zlib-ng-compat-devel \
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
    pandas \
    pytz

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
