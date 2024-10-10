FROM docker.io/library/ubuntu:mantic-20240216 as ci
RUN echo "build base-layers"

USER root

RUN apt-get update --fix-missing;
RUN apt-get upgrade -y ;

RUN apt-get install -y \
    git\
    git-lfs\
    file \
    clang-format-17 \
    cmake \
    make \
    gcc \
    libtar-dev \
    libcli11-dev \
    nlohmann-json3-dev \
    rsync \
    gettext \
    valgrind \
    openjdk-17-jdk \
    wget \
    unzip \
    openssl \
    lcov \
    gcovr \
    ;
    
RUN update-alternatives --install /usr/bin/clang-format clang-format /usr/bin/clang-format-17 100;

RUN apt-get install -y \
    python3.6 \
    python3-pip\
    ;

RUN pip3 install --break-system-packages\
    robotframework \
    numpy \
    pandas \
    ;

RUN \
    echo "[safe]" >> ~/.gitconfig &&\
    echo "    directory = *" >> ~/.gitconfig &&\
    echo "";

FROM ci as kernel
RUN echo "now build kernel-layers"

RUN apt-get install -y\
    curl\
    rpm \
    elfutils \
    clang \
    lld \
    llvm \
    libelf-dev \
    ccache \
    build-essential\
    flex\
    bison\
    bc\
    busybox-static\
    strace\
    cpio\
    qemu-system\
    libncurses-dev\
    libssl-dev\
    kmod\
    ;

FROM kernel as dev
RUN echo "now build dev-layers"

RUN apt-get install -y\
    ninja-build \
    gdb \
    lldb \
    bash-completion \
    ;


RUN pip3 install --break-system-packages\
    scipy \
    matplotlib \
    ;