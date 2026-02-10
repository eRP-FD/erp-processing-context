# syntax=docker/dockerfile:1.20
#
# (C) Copyright IBM Deutschland GmbH 2021, 2025
# (C) Copyright IBM Corp. 2021, 2025
#
# non-exclusively licensed to gematik GmbH
#

FROM de.icr.io/erp_dev/erp-pc-ubuntu-build:2.3.1 AS build

WORKDIR /erp
ARG ERP_BUILD_VERSION
ARG ERP_RELEASE_VERSION
COPY --parents \
        src \
        test \
        tools \
        cmake \
        resources \
        conanfile.py \
        CMakeLists.txt \
        scripts/ci-build.sh \
        scripts/CMakeLists.txt \
        scripts/generate_pki_test_data.sh \
        source

RUN --mount=type=secret,id=conan_credentials.json,target=/root/.conan2/credentials.json \
    source/scripts/ci-build.sh \
        --source_folder="source" \
        --output_folder="/" \
        --build_type="RelWithDebInfo" \
        --build_version="${ERP_BUILD_VERSION}" \
        --release_version="${ERP_RELEASE_VERSION}" \
        --skip-build

RUN cmake --build "/build/RelWithDebInfo" --target "resources;erp-fhir-ws" && \
    cmake --install "/build/RelWithDebInfo" --strip --prefix "/usr/local/erp-fhir-ws" --component erp-fhir-ws && \
    rm -rf /build

FROM ubuntu:jammy AS runtime
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update --fix-missing && \
    apt-get install -y tzdata && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

COPY --from=build /usr/local/erp-fhir-ws /usr/local/erp-fhir-ws
EXPOSE 8085/tcp
ENV ERP_SERVER_HOST=0.0.0.0
CMD [ "/usr/local/erp-fhir-ws/bin/erp-fhir-ws" ]
