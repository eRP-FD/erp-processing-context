#
# (C) Copyright IBM Deutschland GmbH 2021, 2024
# (C) Copyright IBM Corp. 2021, 2024
#
# non-exclusively licensed to gematik GmbH
#

FROM de.icr.io/erp_dev/erp-pc-ubuntu-build:2.2.2 as build

# ensure strip command is available
RUN apt-get update --fix-missing && \
    apt-get install -y binutils

WORKDIR /erp
COPY . source
ARG GITHUB_USERNAME
ARG GITHUB_OAUTH_TOKEN
ENV GITHUB_USERNAME=${GITHUB_USERNAME:-''} \
    GITHUB_OAUTH_TOKEN=${GITHUB_OAUTH_TOKEN:-''}
RUN git config --global url.https://${GITHUB_USERNAME:-}:${GITHUB_OAUTH_TOKEN:-}@github.ibmgcloud.net/.insteadOf git@github.ibmgcloud.net:
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

COPY scripts/ci-build.sh scripts/ci-build.sh


RUN scripts/ci-build.sh \
        --source_folder="source" \
        --output_folder="/" \
        --build_type="Release" \
        --build_version="${ERP_BUILD_VERSION}" \
        --release_version="${ERP_RELEASE_VERSION}" \
        --skip-build

RUN scripts/ci-build.sh \
        --source_folder="source" \
        --output_folder="/" \
        --build_type="RelWithDebInfo" \
        --build_version="${ERP_BUILD_VERSION}" \
        --release_version="${ERP_RELEASE_VERSION}" \
        --skip-build

# Build Release variant:
FROM build AS release_build

WORKDIR /build/Release
RUN cmake --build . --target production -- -l$(nproc) && \
    cmake --install . --strip --component production --prefix /erp-exporter &&\
    cp install_manifest_production.txt install_manifest.txt

RUN tar -czf ./erp-medication-exporter.tar.gz /erp-exporter

# Build RelWithDebInfo variant:
FROM build AS reldeb_build

WORKDIR /build/RelWithDebInfo

RUN cmake --build . --target production -- -l$(nproc) && \
    cmake --install . --component production --prefix /debug/erp-exporter &&\
    cp install_manifest_production.txt install_manifest.txt

RUN tar -czf ./erp-medication-exporter-debug.tar.gz /debug/erp-exporter

# Build runtime image here:
FROM de.icr.io/erp_dev/rt-gramine-direct:0.0.8 AS runtime

WORKDIR /debug/erp-exporter
COPY --from=reldeb_build /build/RelWithDebInfo/erp-medication-exporter-debug.tar.gz .
COPY --from=reldeb_build /debug/erp-exporter .

WORKDIR /erp-exporter
COPY --from=release_build /build/Release/erp-medication-exporter.tar.gz .
COPY --from=release_build /erp-exporter .

WORKDIR /
COPY docker/exporter/files/start_medication_exporter.sh /start_medication_exporter.sh
RUN chmod 755 /start_medication_exporter.sh

ENTRYPOINT ["/start_medication_exporter.sh"]
