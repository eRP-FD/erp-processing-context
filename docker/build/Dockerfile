#
# (C) Copyright IBM Deutschland GmbH 2021
# (C) Copyright IBM Corp. 2021
#

FROM ubuntu:focal AS build

# Set up a c++ build environment
# libssl1.1 install openssl1.1.1 and related certificates
# libefiboot-dev and libefivar-dev are required by indirect dependencies from libtpmclient/libtss.
RUN apt-get update --fix-missing && \
    apt-get install -y software-properties-common && \
    add-apt-repository ppa:ubuntu-toolchain-r/test && \
    apt-get install -y  \
    openjdk-11-jre-headless \
    gcc-9 g++-9 \
    jq \
    wget \
    libgtest-dev \
    libssl1.1 \
    git \
    ninja-build \
    python3 \
    python3-pip \
    pkg-config \
    uuid-runtime \
    xz-utils \
    curl \
    automake \
    clang-tidy-12 \
    libefiboot-dev \
    libefivar-dev \
    ca-certificates \
    xxd \
    xmlsec1 \
    && rm -rf /var/lib/apt/lists/*

# Update certificates and install latest version of cmake
RUN update-ca-certificates && \
    pip install --upgrade cmake

# build gtest
WORKDIR /usr/src/googletest
RUN cmake .
RUN cmake --build . --target install
WORKDIR /root
RUN mkdir -p ~/.ssh && ssh-keyscan github.ibmgcloud.net >> ~/.ssh/known_hosts
# work around fix for CVE-2022-24765:
RUN git config --global --add safe.directory '*'
