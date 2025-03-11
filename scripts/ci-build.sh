#!/bin/bash -ex

# (C) Copyright IBM Deutschland GmbH 2021, 2024
# (C) Copyright IBM Corp. 2021, 2024
#
# non-exclusively licensed to gematik GmbH

# Do not stop on address sanitizer errors.
export halt_on_error=0

die()
{
    echo "$@"
    exit 1
}

readargs()
{
    erp_build_version=
    erp_release_version=
    skip_build=no
    conan_args=()
    build_type=RelWithDebInfo
    with_ccache=false
    output_folder="."
    source_folder="."
    with_hsm_mock="False"
    while [ $# -gt 0 ] ; do
        case "$1" in
            --build_version=*)
                erp_build_version="${1#*=}"
                ;;
            --release_version=*)
                erp_release_version="${1#*=}"
                ;;
            --build_type=*)
                build_type="${1#*=}"
                ;;
            --source_folder=*)
                source_folder="${1#*=}"
                ;;
            --output_folder=*)
                output_folder="${1#*=}"
                ;;
            --with_ccache)
                with_ccache=true
                ;;
            --skip-build)
                skip_build=yes
                ;;
            --with_hsm_mock)
                with_hsm_mock="True"
                ;;
            *)
                die "Unknown option: $1"
                ;;
        esac
        shift
    done
}

readargs "$@"

if "${with_ccache}" ; then
    CCACHE_DIR=$(mount | awk '/ccache/ {print $3}')
    if [[ -n ${CCACHE_DIR} ]]; then
        ls -l ${CCACHE_DIR}
        df -h ${CCACHE_DIR}
        ccache --version
        ccache -o cache_dir=${CCACHE_DIR}
        ccache -M 40000M
        ccache -p
        ccache -s
    else
        echo "No ccache dir found."
    fi
    conan_args+=(--options "&:with_ccache=True")
fi

test -n "${erp_build_version}" || die "missing argument --build_version="
test -n "${erp_release_version}" || die "missing argument --release_version="

pip3 install "conan<3" --upgrade
pip3 install --user packageurl-python
export CONAN_TRACE_FILE=$(pwd)/conan_trace.log
conan --version

conan_remote_add() {
    if ! ( conan remote list | grep -qE "^$1" ; ) ; then
        conan remote add "$1" "$2"
    fi
}

conan_remote_add erp-conan-2 https://artifactory-cpp-ce.ihc-devops.net/artifactory/api/conan/erp-conan-2
conan_remote_add erp-conan-internal https://artifactory-cpp-ce.ihc-devops.net/artifactory/api/conan/erp-conan-internal

set +x
conan_auth() {
    set +x
    CONAN_LOGIN_USERNAME="$1" CONAN_PASSWORD="$2" conan remote auth erp-conan-2
    CONAN_LOGIN_USERNAME="$1" CONAN_PASSWORD="$2" conan remote auth erp-conan-internal
}
conan_auth "${NEXUS_USERNAME:-${CONAN_LOGIN_USERNAME}}" "${NEXUS_PASSWORD:-${CONAN_PASSWORD}}"
set -x

export GCC_VERSION=12
export CC=gcc-${GCC_VERSION}
export CXX=g++-${GCC_VERSION}

cmake_version="$(cmake --version | grep -m1 -oE '[^ ]*$')"

mkdir -p "$(conan config home)/profiles"

tee "$(conan config home)/profiles/default" <<EOF
[settings]
arch=x86_64
build_type=Release
compiler=gcc
compiler.cppstd=20
compiler.libcxx=libstdc++11
compiler.version=${GCC_VERSION}
os=Linux
[buildenv]
CC=${CC}
CXX=${CXX}
[platform_tool_requires]
cmake/${cmake_version}
EOF

lib_build_type=RelWithDebInfo

conan_args+=(
    -s "&:build_type=${build_type}"
    -s "build_type=${lib_build_type}"
    --build missing
    --output-folder "${output_folder}"
    --version "${erp_build_version}"
    --options "&:release_version=${erp_release_version}"
    --options "&:with_hsm_mock=${with_hsm_mock}"
    --options "&:with_hsm_tpm_production=True"
    --options "&:with_sbom=True"
    --options "&:with_warning_as_error=True"
    -c tools.cmake.cmaketoolchain:generator=Ninja
    -cc core:non_interactive=True
)

conan install "${source_folder}" "${conan_args[@]}"
cmake "${source_folder}" --preset "conan-${build_type,,}"

if [ "$skip_build" == "yes" ]; then
    cmake --build "${output_folder}/build/${build_type}" --target generated_source
else
    cmake --build "${output_folder}/build/${build_type}" --target test --target production -- -l$(nproc)
fi
