#
# (C) Copyright IBM Deutschland GmbH 2021, 2024
# (C) Copyright IBM Corp. 2021, 2024
#
# non-exclusively licensed to gematik GmbH
#

FROM de.icr.io/erp_dev/erp-pc-ubuntu-build:2.1.4 as build

# ensure strip command is available
RUN apt-get update --fix-missing && \
    apt-get install -y binutils

WORKDIR /build
COPY . erp-medication-exporter
ARG GITHUB_USERNAME
ARG GITHUB_OAUTH_TOKEN
ENV GITHUB_USERNAME=${GITHUB_USERNAME:-''} \
    GITHUB_OAUTH_TOKEN=${GITHUB_OAUTH_TOKEN:-''}
RUN git config --global url.https://${GITHUB_USERNAME:-}:${GITHUB_OAUTH_TOKEN:-}@github.ibmgcloud.net/.insteadOf git@github.ibmgcloud.net:
RUN pip3 install "conan<2.0" --upgrade
# set username and password in environment variables for IBM internal nexus "erp"
# (as added below) so that the login to this remote happens automatically.
ARG CONAN_LOGIN_USERNAME
ARG CONAN_PASSWORD
ARG ERP_BUILD_VERSION
ARG ERP_RELEASE_VERSION
ENV CONAN_LOGIN_USERNAME=${CONAN_LOGIN_USERNAME:-''} \
    CONAN_PASSWORD=${CONAN_PASSWORD:-''} \
    ERP_BUILD_VERSION=${ERP_BUILD_VERSION:-'0.0.0'} \
    ERP_RELEASE_VERSION=${ERP_RELEASE_VERSION:-'0.0.0'} \
    GCC_VERSION=12

RUN conan remote clean &&\
    conan remote add conan-center-binaries  https://nexus.epa-dev.net/repository/conan-center-binaries --force &&\
    conan user -r conan-center-binaries -p "${CONAN_PASSWORD}" "${CONAN_LOGIN_USERNAME}" &&\
    conan remote add erp https://nexus.epa-dev.net/repository/erp-conan-internal true --force &&\
    conan user -r erp -p "${CONAN_PASSWORD}" "${CONAN_LOGIN_USERNAME}" &&\
    conan remote add nexus https://nexus.epa-dev.net/repository/conan-center-proxy true --force &&\
    conan profile new --detect default && \
    conan profile update settings.compiler.version=${GCC_VERSION} default && \
    conan profile update env.CC=gcc-${GCC_VERSION} default && \
    conan profile update env.CXX=g++-${GCC_VERSION} default


# Build Release variant:
FROM build AS release_build

WORKDIR /build/erp-medication-exporter/release
RUN CC=gcc-${GCC_VERSION} CXX=g++-${GCC_VERSION} cmake \
    -GNinja \
    -DCMAKE_BUILD_TYPE=Release \
    -DERP_WITH_HSM_MOCK=OFF \
    -DERP_BUILD_VERSION=${ERP_BUILD_VERSION} \
    -DERP_RELEASE_VERSION=${ERP_RELEASE_VERSION} \
    -DERP_WARNING_AS_ERROR=ON \
    -DCMAKE_INSTALL_PREFIX=/erp-exporter .. && \
    ninja -l$(nproc) production && \
    cmake --install . --strip --component production && \
    cp install_manifest_production.txt install_manifest.txt

RUN tar -czf ./erp-medication-exporter.tar.gz /erp-exporter

# Build RelWithDebInfo variant:
FROM build AS reldeb_build

WORKDIR /build/erp-medication-exporter/reldeb
RUN CC=gcc-12 CXX=g++-12 cmake \
    -GNinja \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DERP_WITH_HSM_MOCK=OFF \
    -DERP_BUILD_VERSION=${ERP_BUILD_VERSION} \
    -DERP_RELEASE_VERSION=${ERP_RELEASE_VERSION} \
    -DERP_WARNING_AS_ERROR=ON \
    -DCMAKE_INSTALL_PREFIX=/debug/erp-exporter .. && \
    ninja -l$(nproc) production && \
    cmake --install . --strip --component production && \
    cp install_manifest_production.txt install_manifest.txt

RUN tar -czf ./erp-medication-exporter-debug.tar.gz /debug/erp-exporter

# Build runtime image here:
FROM de.icr.io/erp_dev/rt-gramine-direct:0.0.7 AS runtime

WORKDIR /debug/erp-exporter
COPY --from=reldeb_build /build/erp-medication-exporter/reldeb/erp-medication-exporter-debug.tar.gz .
COPY --from=reldeb_build /debug/erp-exporter .

WORKDIR /erp-exporter
COPY --from=release_build /build/erp-medication-exporter/release/erp-medication-exporter.tar.gz .
COPY --from=release_build /erp-exporter .

WORKDIR /
COPY docker/exporter/files/start_medication_exporter.sh /start_medication_exporter.sh
RUN chmod 755 /start_medication_exporter.sh

ENTRYPOINT ["/start_medication_exporter.sh"]
