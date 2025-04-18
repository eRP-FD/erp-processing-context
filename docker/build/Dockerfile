#
# (C) Copyright IBM Deutschland GmbH 2021
# (C) Copyright IBM Corp. 2021
#

FROM ubuntu:jammy AS build

# Set up a c++ build environment
# libssl1.1 install openssl1.1.1 and related certificates
# libefiboot-dev and libefivar-dev are required by indirect dependencies from libtpmclient/libtss.
RUN apt-get update --fix-missing && \
    apt-get install -y software-properties-common wget && \
    wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add -  && \
    add-apt-repository -y -S deb http://apt.llvm.org/jammy/ llvm-toolchain-jammy-16 main && \
    add-apt-repository -y -S deb http://apt.llvm.org/jammy/ llvm-toolchain-jammy-19 main
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y  \
    openjdk-11-jre-headless \
    gcc-12 g++-12 \
    jq \
    wget \
    libgtest-dev \
    git \
    ninja-build \
    python3 \
    python3-pip \
    pkg-config \
    uuid-runtime \
    xz-utils \
    curl \
    automake \
    libefiboot-dev \
    libefivar-dev \
    ca-certificates \
    xxd \
    xmlsec1 \
    xsltproc \
    clang-tidy-16 \
    clang-tidy-19 \
    tzdata \
    ccache \
    openjdk-17-jdk \
    && rm -rf /var/lib/apt/lists/*

# Update certificates and install latest version of cmake
RUN update-ca-certificates && \
    pip install --upgrade cmake 'conan'

# Tool for converting json sbom to xml
RUN wget https://github.com/CycloneDX/cyclonedx-cli/releases/download/v0.27.2/cyclonedx-linux-x64 -O /usr/bin/cyclonedx && \
    chmod 755 /usr/bin/cyclonedx

COPY known_hosts /root/.ssh/known_hosts
# work around fix for CVE-2022-24765:
RUN git config --global --add safe.directory '*'
