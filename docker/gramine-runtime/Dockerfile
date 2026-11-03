#
# (C) Copyright IBM Deutschland GmbH 2021, 2026
# (C) Copyright IBM Corp. 2021, 2026
#
# non-exclusively licensed to gematik GmbH
#

# Build a simple runtime environment based on ubuntu:jammy with gramine-direct installed.
# This environment will be used to run the erp-processing-context with gramine-direct.

FROM de.icr.io/erp_dev/ubuntu-noble:latest AS runtime

# binutils provides 'readelf' which is required for gramine-sgx-sign
RUN apt-get update --fix-missing && \
    apt-get install --no-install-recommends -y software-properties-common \
    binutils gdb gdbserver screen \
    gnupg && \
    rm -rf /var/lib/apt/lists/*

COPY files/gramine-keyring.gpg /usr/share/keyrings

RUN /bin/bash -o pipefail -c 'echo "deb [arch=amd64 signed-by=/usr/share/keyrings/gramine-keyring.gpg] https://packages.gramineproject.io/ 1.6 main" | tee /etc/apt/sources.list.d/gramine.list' \
    && apt-get update \
    && apt-get install --no-install-recommends -y gramine && \
    rm -rf /var/lib/apt/lists/*

RUN apt-get update --fix-missing && \
    rm -rf /var/lib/apt/lists/*

RUN mkdir -p /var/log/erp
