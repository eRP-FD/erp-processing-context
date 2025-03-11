#
# (C) Copyright IBM Deutschland GmbH 2021, 2024
# (C) Copyright IBM Corp. 2021, 2024
#
# non-exclusively licensed to gematik GmbH
#

FROM de.icr.io/erp_dev/erp-pc-ubuntu-build:2.2.2 as build

# ensure strip command is available
RUN apt-get update --fix-missing && \
    apt-get install -y binutils rsync

# Build the erp processing context.
WORKDIR /build
COPY . source

ARG GITHUB_USERNAME
ARG GITHUB_OAUTH_TOKEN
ENV GITHUB_USERNAME=${GITHUB_USERNAME:-''} \
    GITHUB_OAUTH_TOKEN=${GITHUB_OAUTH_TOKEN:-''}
RUN git config --global url.https://${GITHUB_USERNAME:-}:${GITHUB_OAUTH_TOKEN:-}@github.ibmgcloud.net/.insteadOf git@github.ibmgcloud.net:
RUN pip3 install "conan<3" --upgrade
# set username and password in environment variables for IBM internal nexus "erp"
# (as added below) so that the login to this remote happens automatically.
ARG CONAN_LOGIN_USERNAME
ARG CONAN_PASSWORD
ARG ERP_BUILD_VERSION
ARG ERP_RELEASE_VERSION
ENV CONAN_LOGIN_USERNAME=${CONAN_LOGIN_USERNAME:-''} \
    CONAN_PASSWORD=${CONAN_PASSWORD:-''} \
    ERP_BUILD_VERSION=${ERP_BUILD_VERSION:-'0.0.0'} \
    ERP_RELEASE_VERSION=${ERP_RELEASE_VERSION:-'0.0.0'}


RUN source/scripts/ci-build.sh \
        --source_folder="source" \
        --output_folder="/" \
        --build_type="RelWithDebInfo" \
        --build_version="${ERP_BUILD_VERSION}" \
        --release_version="${ERP_RELEASE_VERSION}" \
        --skip-build \
        --with_hsm_mock

RUN cmake --build "RelWithDebInfo" --target blob-db-initialization -- -l$(nproc)
RUN cmake --install "RelWithDebInfo" --prefix /erp --component blob-db-initialization

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
