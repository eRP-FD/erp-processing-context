#
# (C) Copyright IBM Deutschland GmbH 2021
# (C) Copyright IBM Corp. 2021
#

FROM de.icr.io/erp_dev/ubuntu-noble:latest AS build

# Set up a c++ build environment
# libssl1.1 install openssl1.1.1 and related certificates
# libefiboot-dev and libefivar-dev are required by indirect dependencies from libtpmclient/libtss.
RUN apt-get update --fix-missing && \
    apt-get install --no-install-recommends -y software-properties-common wget gpg-agent && \
    /bin/bash -o pipefail -c 'wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add - ' && \
    add-apt-repository -y -S deb http://apt.llvm.org/noble/ llvm-toolchain-noble-19 main && \
    /bin/bash -o pipefail -c 'wget -O - http://apt.postgresql.org/pub/repos/apt/ACCC4CF8.asc | tee /etc/apt/keyrings/ACCC4CF8-postgres.asc' && \
    /bin/bash -o pipefail -c 'echo "deb [signed-by=/etc/apt/keyrings/ACCC4CF8-postgres.asc] https://apt.postgresql.org/pub/repos/apt noble-pgdg main" > /etc/apt/sources.list.d/pgdg.list' && \
    apt-get update --fix-missing && \
    DEBIAN_FRONTEND=noninteractive apt-get install --no-install-recommends -y  \
        openjdk-21-jre-headless \
        gcc-12 g++-12 \
        patch \
        jq \
        wget \
        libgtest-dev \
        git \
        make \
        ninja-build \
        pkg-config \
        uuid-runtime \
        xz-utils \
        curl \
        automake \
        libefiboot-dev \
        libefivar-dev \
        postgresql-client-16 \
        ca-certificates \
        xxd \
        xmlsec1 \
        xsltproc \
        clang-tidy-19 \
        tzdata \
        ccache \
        pipx && \
    rm -rf /var/lib/apt/lists/*

RUN wget https://github.com/Kitware/CMake/releases/download/v4.4.3/cmake-4.4.3-linux-x86_64.tar.gz && \
    wget https://github.com/CycloneDX/cyclonedx-cli/releases/download/v0.27.2/cyclonedx-linux-x64

# Tool for converting json sbom to xml
RUN mv /cyclonedx-linux-x64 /usr/bin/cyclonedx && \
    chmod 755 /usr/bin/cyclonedx

# Update certificates and install a recent version of cmake (required for fhir.cmake)
RUN update-ca-certificates && \
    tar -xzf /cmake-4.4.3-linux-x86_64.tar.gz \
       --strip-components=1 \
       -C /usr/local

# Update certificates
RUN update-ca-certificates && \
    pipx install 'conan<3.0'

ENV PATH=/root/.local/bin:$PATH

COPY ./files/known_hosts /root/.ssh/known_hosts
# work around fix for CVE-2022-24765:
RUN git config --global --add safe.directory '*'
