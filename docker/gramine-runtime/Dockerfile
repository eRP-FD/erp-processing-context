#
# (C) Copyright IBM Deutschland GmbH 2022
# (C) Copyright IBM Corp. 2022
#

# Build a simple runtime environment based on ubuntu:focal with gramine-direct installed.
# This environment will be used to run the erp-processing-context with gramine-direct.

FROM ubuntu:focal AS runtime

RUN apt-get update --fix-missing \
    && apt-get install -y software-properties-common gnupg \
    && rm -rf /var/lib/apt/lists/*

COPY docker/files/gramine-keyring.gpg /usr/share/keyrings

RUN echo 'deb [arch=amd64 signed-by=/usr/share/keyrings/gramine-keyring.gpg] https://packages.gramineproject.io/ 1.2 main' \
    |  tee /etc/apt/sources.list.d/gramine.list \
    && apt-get update \
    && apt-get install -y gramine

RUN apt-get update --fix-missing

RUN mkdir -p /var/log/erp
