#
# (C) Copyright IBM Deutschland GmbH 2021, 2024
# (C) Copyright IBM Corp. 2021, 2024
#
# non-exclusively licensed to gematik GmbH
#

FROM de.icr.io/erp_dev/erp-pc-ubuntu-build:2.1.4 as build

# ensure strip command is available
RUN apt-get update --fix-missing && \
    apt-get install -y binutils rsync

# Build the erp processing context.
WORKDIR /build
COPY . erp-processing-context
RUN mkdir release
WORKDIR /build/erp-processing-context/release
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
    conan remote add nexus https://nexus.epa-dev.net/repository/conan-center-proxy true --force &&\
    conan remote add erp https://nexus.epa-dev.net/repository/erp-conan-internal true --force &&\
    conan profile new --detect default && \
    conan profile update settings.compiler.version=${GCC_VERSION} default && \
    conan profile update env.CC=gcc-${GCC_VERSION} default && \
    conan profile update env.CXX=g++-${GCC_VERSION} default


RUN CC=gcc-${GCC_VERSION} \
    CXX=g++-${GCC_VERSION} \
    cmake \
        -GNinja \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DERP_WITH_HSM_MOCK=ON \
        -DERP_BUILD_VERSION=${ERP_BUILD_VERSION} \
        -DERP_RELEASE_VERSION=${ERP_RELEASE_VERSION} \
        -DCMAKE_INSTALL_PREFIX=/erp ..
RUN ninja -l$(nproc) blob-db-initialization
RUN cmake --install . --component blob-db-initialization
RUN cp ../scripts/enrolment-helper.sh /erp/bin
RUN cp ../scripts/auto-enrolment.sh /erp/bin

WORKDIR /build/vau-hsm
RUN git clone --depth 1 git@github.ibmgcloud.net:eRp/vau-hsm.git .
RUN mkdir -p /erp/vau-hsm/client/test/resources
RUN rsync -a client/test/resources/saved /erp/vau-hsm/client/test/resources/

FROM ubuntu:jammy AS runtime

RUN apt-get update --fix-missing && \
    apt-get install -y curl jq
WORKDIR /erp
COPY --from=build /erp .
WORKDIR /erp/bin
CMD ./auto-enrolment.sh
