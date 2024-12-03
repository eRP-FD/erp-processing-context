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
    while [ $# -gt 0 ] ; do
        case "$1" in
            --build_version=*)
                erp_build_version="${1#*=}"
                ;;
            --release_version=*)
                erp_release_version="${1#*=}"
                ;;
            --skip-build)
                skip_build=yes
                ;;
            *)
                die "Unknown option: $1"
                ;;
        esac
        shift
    done
}

readargs "$@"

if [ "$skip_build" == "no" ]; then
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
fi

test -n "${erp_build_version}" || die "missing argument --build_version="
test -n "${erp_release_version}" || die "missing argument --release_version="

mkdir -p jenkins-build-debug
cd jenkins-build-debug
pip3 install "conan<2.0" --upgrade
pip3 install --user packageurl-python
export CONAN_TRACE_FILE=$(pwd)/conan_trace.log
conan --version
conan remote clean
conan remote add nexus https://nexus.epa-dev.net/repository/conan-center-proxy true --force
conan remote add conan-center-binaries  https://nexus.epa-dev.net/repository/conan-center-binaries --force
conan remote add erp https://nexus.epa-dev.net/repository/erp-conan-internal true --force
set -x

export GCC_VERSION=12
export CC=gcc-${GCC_VERSION}
export CXX=g++-${GCC_VERSION}
# Add credentials for IBM internal nexus
conan user -r erp -p "${NEXUS_PASSWORD}" "${NEXUS_USERNAME}"
conan user -r conan-center-binaries -p "${NEXUS_PASSWORD}" "${NEXUS_USERNAME}"
conan profile new --detect default
conan profile update env.CC=${CC} default
conan profile update env.CXX=${CXX} default
conan profile update settings.compiler.version=${GCC_VERSION} default
set +x
repeat_without_binary_repo=0
cmake -GNinja \
      -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DERP_BUILD_VERSION=${erp_build_version} \
      -DERP_RELEASE_VERSION=${erp_release_version} \
      -DERP_WITH_HSM_MOCK=ON \
      -DERP_WARNING_AS_ERROR=ON \
      -DERP_CONAN_ARGS="-o with_sbom=True" \
      -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
      .. || repeat_without_binary_repo=1

if [ $repeat_without_binary_repo == 1 ]; then
  conan remote remove conan-center-binaries
  conan remove -f '*'
  cmake -GNinja \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DERP_BUILD_VERSION=${erp_build_version} \
        -DERP_RELEASE_VERSION=${erp_release_version} \
        -DERP_WITH_HSM_MOCK=ON \
        -DERP_WARNING_AS_ERROR=ON \
        -DERP_CONAN_ARGS="-o with_sbom=True" \
        -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
        ..
  conan remote add conan-center-binaries  https://nexus.epa-dev.net/repository/conan-center-binaries --force
  conan user -r conan-center-binaries -p "${NEXUS_PASSWORD}" "${NEXUS_USERNAME}"
fi

if [ "$skip_build" == "yes" ]; then
    ninja generated_source
else
    ninja -l$(nproc) test production fhirtools-test exporter-test
fi

if [ $repeat_without_binary_repo == 1 ]; then
  exit 2
fi
