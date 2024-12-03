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
COPY . erp-processing-context
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

WORKDIR /build/erp-processing-context/release
RUN CC=gcc-${GCC_VERSION} CXX=g++-${GCC_VERSION} cmake \
    -GNinja \
    -DCMAKE_BUILD_TYPE=Release \
    -DERP_WITH_HSM_MOCK=OFF \
    -DERP_BUILD_VERSION=${ERP_BUILD_VERSION} \
    -DERP_RELEASE_VERSION=${ERP_RELEASE_VERSION} \
    -DERP_WARNING_AS_ERROR=ON \
    -DCMAKE_INSTALL_PREFIX=/erp .. && \
    ninja -l$(nproc) production && \
    cmake --install . --strip --component production &&\
    cp install_manifest_production.txt install_manifest.txt

COPY docker/manifest.erp.release.template /erp/bin/erp-processing-context.manifest
COPY docker/manifest.erp.release.kubernetes.template /erp/bin/erp-processing-context-kubernetes.manifest

COPY docker/manifest.exporter.release.template /erp/bin/exporter.manifest
COPY docker/manifest.exporter.release.kubernetes.template /erp/bin/exporter-kubernetes.manifest

COPY docker/create_gramine_manifest.bash /build/erp-processing-context/release

# Create signed manifest files for erp-processing-context.
RUN chmod 755 /build/erp-processing-context/release/create_gramine_manifest.bash && \
    /build/erp-processing-context/release/create_gramine_manifest.bash \
    /build/erp-processing-context/release/install_manifest.txt \
    /erp/bin/erp-processing-context.manifest
RUN /build/erp-processing-context/release/create_gramine_manifest.bash \
    /build/erp-processing-context/release/install_manifest.txt \
    /erp/bin/erp-processing-context-kubernetes.manifest

# Create signed manifest files for medication-exporter.
RUN /build/erp-processing-context/release/create_gramine_manifest.bash \
    /build/erp-processing-context/release/install_manifest.txt \
    /erp/bin/exporter.manifest
RUN /build/erp-processing-context/release/create_gramine_manifest.bash \
    /build/erp-processing-context/release/install_manifest.txt \
    /erp/bin/exporter-kubernetes.manifest

RUN tar -czf ./erp-processing-context.tar.gz /erp

# Build RelWithDebInfo variant:
FROM build AS reldeb_build

WORKDIR /build/erp-processing-context/reldeb
RUN CC=gcc-12 CXX=g++-12 cmake \
    -GNinja \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DERP_WITH_HSM_MOCK=OFF \
    -DERP_BUILD_VERSION=${ERP_BUILD_VERSION} \
    -DERP_RELEASE_VERSION=${ERP_RELEASE_VERSION} \
    -DERP_WARNING_AS_ERROR=ON \
    -DCMAKE_INSTALL_PREFIX=/debug/erp .. && \
    ninja -l$(nproc) production && \
    cmake --install . --strip --component production &&\
    cp install_manifest_production.txt install_manifest.txt

COPY docker/manifest.erp.reldeb.template /debug/erp/bin/erp-processing-context.manifest
COPY docker/manifest.erp.reldeb.kubernetes.template /debug/erp/bin/erp-processing-context-kubernetes.manifest

COPY docker/manifest.exporter.reldeb.template /debug/erp/bin/exporter.manifest
COPY docker/manifest.exporter.reldeb.kubernetes.template /debug/erp/bin/exporter-kubernetes.manifest

COPY docker/create_gramine_manifest.bash /build/erp-processing-context/reldeb

# Create signed manifest files for erp-processing-context.
RUN chmod 755 /build/erp-processing-context/reldeb/create_gramine_manifest.bash && \
    /build/erp-processing-context/reldeb/create_gramine_manifest.bash \
    /build/erp-processing-context/reldeb/install_manifest.txt \
    /debug/erp/bin/erp-processing-context.manifest
RUN /build/erp-processing-context/reldeb/create_gramine_manifest.bash \
    /build/erp-processing-context/reldeb/install_manifest.txt \
    /debug/erp/bin/erp-processing-context-kubernetes.manifest

# Create signed manifest files for medication-exporter.
RUN /build/erp-processing-context/reldeb/create_gramine_manifest.bash \
    /build/erp-processing-context/reldeb/install_manifest.txt \
    /debug/erp/bin/exporter.manifest
RUN /build/erp-processing-context/reldeb/create_gramine_manifest.bash \
    /build/erp-processing-context/reldeb/install_manifest.txt \
    /debug/erp/bin/exporter-kubernetes.manifest

RUN tar -czf ./erp-processing-context-debug.tar.gz /debug/erp



# Build runtime image here:
FROM de.icr.io/erp_dev/rt-gramine-direct:0.0.7 AS runtime

# Dummy key required for the signing steps.
RUN openssl genrsa -3 -out /enclave-dummy-key.pem 3072

WORKDIR /debug/erp
COPY --from=reldeb_build /build/erp-processing-context/reldeb/erp-processing-context-debug.tar.gz .
COPY --from=reldeb_build /debug/erp .

# Make sure that the signing step performs well.
RUN cd /debug/erp/bin && gramine-sgx-sign \
    --manifest /debug/erp/bin/erp-processing-context-kubernetes.manifest \
    --output   /debug/erp/bin/erp-processing-context-kubernetes.manifest.sgx \
    --libpal   /usr/lib/x86_64-linux-gnu/gramine/sgx/libpal.so \
    --key      /enclave-dummy-key.pem
RUN cd /debug/erp/bin && gramine-sgx-sign \
    --manifest /debug/erp/bin/exporter-kubernetes.manifest \
    --output   /debug/erp/bin/exporter-kubernetes.manifest.sgx \
    --libpal   /usr/lib/x86_64-linux-gnu/gramine/sgx/libpal.so \
    --key      /enclave-dummy-key.pem

WORKDIR /erp
COPY --from=release_build /build/erp-processing-context/release/erp-processing-context.tar.gz .
COPY --from=release_build /erp .

# Make sure that the signing step performs well.
RUN cd /erp/bin && gramine-sgx-sign \
    --manifest /erp/bin/erp-processing-context-kubernetes.manifest \
    --output   /erp/bin/erp-processing-context-kubernetes.manifest.sgx \
    --libpal   /usr/lib/x86_64-linux-gnu/gramine/sgx/libpal.so \
    --key      /enclave-dummy-key.pem
RUN cd /erp/bin && gramine-sgx-sign \
    --manifest /erp/bin/exporter-kubernetes.manifest \
    --output   /erp/bin/exporter-kubernetes.manifest.sgx \
    --libpal   /usr/lib/x86_64-linux-gnu/gramine/sgx/libpal.so \
    --key      /enclave-dummy-key.pem

WORKDIR /
COPY docker/start.bash /start.bash
RUN chmod 755 /start.bash

ENTRYPOINT ["/start.bash"]
