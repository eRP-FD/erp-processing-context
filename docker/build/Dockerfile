FROM ubuntu:focal AS build

# Set up a c++ build environment
# libssl1.1 install openssl1.1.1 and related certificates
# libefiboot-dev and libefivar-dev are required by indirect dependencies from libtpmclient/libtss.
RUN apt-get update --fix-missing && \
    apt-get install -y software-properties-common && \
    add-apt-repository ppa:ubuntu-toolchain-r/test && \
    apt-get install -y  \
    openjdk-11-jdk \
    gcc-9 g++-9 \
    wget \
    libgtest-dev \
    libssl1.1 \
    git \
    python3 \
    python3-pip \
    pkg-config \
    xz-utils \
    curl \
    cmake \
    automake \
    clang-tidy \
    libefiboot-dev \
    libefivar-dev \
    && rm -rf /var/lib/apt/lists/*

# build gtest
WORKDIR /usr/src/googletest
RUN cmake .
RUN cmake --build . --target install

