#
# (C) Copyright IBM Deutschland GmbH 2021, 2024
# (C) Copyright IBM Corp. 2021, 2024
#
# non-exclusively licensed to gematik GmbH
#

# Build a simple runtime environment based on ubuntu:jammy with gramine-direct installed.
# This environment will be used to run the erp-processing-context with gramine-direct.

FROM ubuntu:jammy AS runtime

# binutils provides 'readelf' which is required for gramine-sgx-sign
RUN apt-get update --fix-missing && \
    apt-get install -y software-properties-common \
    binutils gdb gdbserver screen \
    gnupg && \
    rm -rf /var/lib/apt/lists/*

COPY files/gramine-keyring.gpg /usr/share/keyrings

RUN echo 'deb [arch=amd64 signed-by=/usr/share/keyrings/gramine-keyring.gpg] https://packages.gramineproject.io/ 1.6 main' \
    |  tee /etc/apt/sources.list.d/gramine.list \
    && apt-get update \
    && apt-get install -y gramine

RUN apt-get update --fix-missing

RUN mkdir -p /var/log/erp
