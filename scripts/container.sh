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
    CONTAINER_ARCH=${CONTAINER_ARCH:="$($CONTAINER_CMD info | grep -oP '(?<=OsArch: linux/)\S+')"}
    TAG="clltk_ci-$CONTAINER_TARGET-$MD5_SUM-$CONTAINER_ARCH"
else
    # docker
    TAG="clltk_ci-$CONTAINER_TARGET-$MD5_SUM"
fi

if ! $CONTAINER_CMD image exists "$TAG"; then 
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

container_cmd="${CONTAINER_CMD} run"
container_workdir="/clltk-workspace-base"
container_cmd+=" --workdir $container_workdir/"

if [[ "${READ_ONLY_CONTAINER:-false}" == "true" ]]; then
    # add source folder as read only
    container_cmd+=" --volume ${ROOT_PATH}/:$container_workdir/:roz"
    container_cmd+=" --read-only"
else
    # or add source folder as writable
    container_cmd+=" --volume ${ROOT_PATH}/:$container_workdir/:z"
fi

if [[ -d "${HOME}/.ccache/" ]]; then
    container_cmd+=" --volume ${HOME}/.ccache:/root/.ccache/"
    container_cmd+=" --env CCACHE_DIR=/root/.ccache/"
fi

catch_dir=./build/in_container/$CONTAINER_ARCH
mkdir -p $catch_dir

mkdir -p $catch_dir/build
container_cmd+=" --env BUILD_DIR=$container_workdir/build/"
container_cmd+=" --volume $catch_dir/build/:$container_workdir/build/:z"

mkdir -p $catch_dir/build/install
container_cmd+=" --env INSTALL_DIR=$container_workdir/build/install/"

if [ -n "${SONAR_TOKEN}" ]; then
    container_cmd+=" --env SONAR_TOKEN=${SONAR_TOKEN}"
fi

if [[ ! -z $PERSITENT_ARTIFACTS ]]; then
    container_cmd+=" --volume ${PERSITENT_ARTIFACTS}:$container_workdir/build/persistent/:z"
else
    mkdir -p $catch_dir/build/persistent
fi
container_cmd+=" --env PERSITENT_ARTIFACTS=$container_workdir/build/persistent/"

mkdir -p $catch_dir/traces/
container_cmd+=" --env CLLTK_TRACING_PATH=/tmp/traces/"
container_cmd+=" --volume $catch_dir/traces:/tmp/traces/:z"

touch $catch_dir/../.bash_history
container_cmd+=" --volume $catch_dir/../.bash_history:/root/.bash_history:z"

container_cmd+=" --security-opt label=disable"
container_cmd+=" --security-opt=seccomp=unconfined"
container_cmd+=" --user root"

# forward pull request info
container_cmd+=" --env CLLTK_CI_VERSION_STEP_REQUIRED=${CLLTK_CI_VERSION_STEP_REQUIRED:-"true"}"

# container run option
container_cmd+=" --entrypoint /bin/bash"
container_cmd+=" --network=host"
container_cmd+=" --hostname clltk-ci"
container_cmd+=" --rm"
container_cmd+=" --interactive"
container_cmd+=" --tty"

# last one
container_cmd+=" $TAG"

# forward into container 
if [ $# -ne 0 ]; then
    # Append -c and the quoted, escaped command to be executed inside the container
    container_cmd+=" --login -i -c \"$(printf '%q ' "$@")\""
else
    container_cmd+=" --login -i" # bash interactive
fi

echo run = "$container_cmd"
eval "$container_cmd" # run the command
