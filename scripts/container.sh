#! /bin/bash
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

### pure functions ###

is_podman()
{
    local version_str=$($1 --version  2> /dev/null)
    echo "$version_str" | grep -qi "^podman"
}

### start ###
REAL_CWD=$(pwd)
ROOT_PATH=$(git rev-parse --show-toplevel)
cd ${ROOT_PATH}

set -e
if [ -n "$CI" ]; then
    set -v
    set -x
fi

USER_CURRENT="$(id -u):$(id -g)"
CONTAINER_USER=${CONTAINER_USER:-$USER_CURRENT} # override if not set
CONTAINER_CMD=${CONTAINER_CMD:-podman}
CONTAINER_TARGET=${CONTAINER_TARGET:=dev}

MD5_SUM=($(md5sum ./scripts/container.Dockerfile))

if is_podman $CONTAINER_CMD ; then
    # podman
    CONTAINER_ARCH=${CONTAINER_ARCH:="$(uname -m)"}
    TAG="clltk_ci-$CONTAINER_TARGET-$MD5_SUM-$CONTAINER_ARCH"
else
    # docker
    TAG="clltk_ci-$CONTAINER_TARGET-$MD5_SUM"
fi

image_exists() {
    if is_podman $CONTAINER_CMD; then
        $CONTAINER_CMD image exists "$1"
    else
        $CONTAINER_CMD image inspect "$1" > /dev/null 2>&1
    fi
}

if ! image_exists "$TAG"; then 
    # build container container
    echo "TAG = $TAG"

    if is_podman $CONTAINER_CMD ; then
        # podman
        ${CONTAINER_CMD} build --file ./scripts/container.Dockerfile --no-cache --target $CONTAINER_TARGET --tag $TAG --arch $CONTAINER_ARCH .
    else
        # docker
        ${CONTAINER_CMD} build --file ./scripts/container.Dockerfile --no-cache --target $CONTAINER_TARGET --tag $TAG .
    fi

    container_build_status=$?
    if [ $container_build_status -ne 0 ]; then
        echo container build failed
        exit 1
    fi
fi

# build the container invocation as an argument array: no eval, so paths with
# spaces and forwarded commands with arbitrary quoting/newlines stay intact
container_args=(run)
container_workdir="/clltk-workspace-base"
container_args+=(--workdir "$container_workdir/")

if [[ "${READ_ONLY_CONTAINER:-false}" == "true" ]]; then
    # add source folder as read only
    container_args+=(--volume "${ROOT_PATH}/:$container_workdir/:roz")
    container_args+=(--read-only)
else
    # or add source folder as writable
    container_args+=(--volume "${ROOT_PATH}/:$container_workdir/:z")
fi

if [[ -d "${HOME}/.ccache/" ]]; then
    container_args+=(--volume "${HOME}/.ccache:/root/.ccache/")
    container_args+=(--env CCACHE_DIR=/root/.ccache/)
fi

catch_dir=./build/in_container/$CONTAINER_ARCH
mkdir -p "$catch_dir"

mkdir -p "$catch_dir/build"
container_args+=(--env "BUILD_DIR=$container_workdir/build/")
container_args+=(--volume "$catch_dir/build/:$container_workdir/build/:z")

mkdir -p "$catch_dir/build/install"
container_args+=(--env "INSTALL_DIR=$container_workdir/build/install/")

if [ -n "${SONAR_TOKEN}" ]; then
    container_args+=(--env "SONAR_TOKEN=${SONAR_TOKEN}")
fi

if [[ ! -z $PERSITENT_ARTIFACTS ]]; then
    container_args+=(--volume "${PERSITENT_ARTIFACTS}:$container_workdir/build_kernel/persistent/:z")
else
    mkdir -p "$catch_dir/build_kernel/persistent"
fi
container_args+=(--env "PERSITENT_ARTIFACTS=$container_workdir/build_kernel/persistent/")

mkdir -p "$catch_dir/traces/"
container_args+=(--env CLLTK_TRACING_PATH=/tmp/traces/)
container_args+=(--volume "$catch_dir/traces:/tmp/traces/:z")

touch "$catch_dir/../.bash_history"
container_args+=(--volume "$catch_dir/../.bash_history:/root/.bash_history:z")

container_args+=(--security-opt label=disable)
container_args+=(--security-opt=seccomp=unconfined)
container_args+=(--user root)

# forward pull request info
container_args+=(--env "CLLTK_CI_VERSION_STEP_REQUIRED=${CLLTK_CI_VERSION_STEP_REQUIRED:-true}")

# container run option
container_args+=(--entrypoint /bin/bash)
container_args+=(--network=host)
container_args+=(--hostname clltk-ci)
container_args+=(--rm)

# Add interactive flags unless running in non-interactive mode (e.g. CI)
# Auto-detect: skip --interactive --tty when stdin is not a terminal
if [[ "${CONTAINER_NON_INTERACTIVE:-false}" != "true" ]] && [ -t 0 ]; then
    container_args+=(--interactive --tty)
fi

container_args+=("$TAG")

# forward into container
if [ $# -ne 0 ]; then
    # Run the provided command: %q-quote every argument into one string that
    # the container's bash -c unquotes again, preserving the exact arguments
    container_args+=(-c "$(printf '%q ' "$@")")
else
    # Interactive shell
    container_args+=(--login -i)
fi

echo run = "$CONTAINER_CMD ${container_args[*]}"
exec "$CONTAINER_CMD" "${container_args[@]}"
