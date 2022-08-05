#!/bin/bash -ex
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
    while [ $# -gt 0 ] ; do
        case "$1" in
            --build_version=*)
                erp_build_version="${1#*=}"
                ;;
            --release_version=*)
                erp_release_version="${1#*=}"
                ;;
            *)
                die "Unknown option: $1"
                ;;
        esac
        shift
    done
}


readargs "$@"

test -n "${erp_build_version}" || die "missing argument --build_version="
test -n "${erp_release_version}" || die "missing argument --release_version="

mkdir -p jenkins-build-debug
cd jenkins-build-debug
pip3 install conan --upgrade
pip3 install --user packageurl-python
conan --version
conan remote clean
conan remote add conan-center-binaries  https://nexus.epa-dev.net/repository/conan-center-binaries --force
conan remote add nexus https://nexus.epa-dev.net/repository/conan-center-proxy true --force
conan remote add erp https://nexus.epa-dev.net/repository/erp-conan-internal true --force
set -x
# Add credentials for IBM internal nexus
conan user -r erp -p "${NEXUS_PASSWORD}" "${NEXUS_USERNAME}"
conan user -r conan-center-binaries -p "${NEXUS_PASSWORD}" "${NEXUS_USERNAME}"
conan profile new --detect default
echo -e '[build_requires]\nautomake/1.16.5' >> ~/.conan/profiles/default
set +x
export CC=gcc-11
export CXX=g++-11
cmake -GNinja \
      -DCMAKE_BUILD_TYPE=Debug \
      -DERP_BUILD_VERSION=${erp_build_version} \
      -DERP_RELEASE_VERSION=${erp_release_version} \
      -DERP_WITH_HSM_MOCK=ON \
      -DERP_WARNING_AS_ERROR=ON \
      -DERP_CONAN_ARGS="-o with_sbom=True" \
      ..

ninja -l$(nproc) clean

"${BUILD_WRAPPER_HOME}/build-wrapper-linux-x86-64" --out-dir sonar-reports \
        ninja -l$(nproc) test production

