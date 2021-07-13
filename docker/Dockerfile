FROM de.icr.io/erp_dev/erp-pc-ubuntu-build:0.0.9 as build

# ensure strip command is available
RUN apt-get update --fix-missing && \
    apt-get install -y binutils

WORKDIR /build
COPY . erp-processing-context
RUN mkdir release reldeb
WORKDIR /build/erp-processing-context/release
ARG GITHUB_USERNAME
ARG GITHUB_OAUTH_TOKEN
ENV GITHUB_USERNAME=${GITHUB_USERNAME:-''} \
    GITHUB_OAUTH_TOKEN=${GITHUB_OAUTH_TOKEN:-''}
RUN git config --global url.https://${GITHUB_USERNAME:-}:${GITHUB_OAUTH_TOKEN:-}@github.ibmgcloud.net/.insteadOf git@github.ibmgcloud.net:
RUN pip3 install conan --upgrade
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

RUN conan remote clean &&\
    conan remote add conan-center-binaries  https://nexus.epa-dev.net/repository/conan-center-binaries --force &&\
    conan user -r conan-center-binaries -p "${CONAN_PASSWORD}" "${CONAN_LOGIN_USERNAME}" &&\
    conan remote add erp https://nexus.epa-dev.net/repository/erp-conan-internal true --force &&\
    conan user -r erp -p "${CONAN_PASSWORD}" "${CONAN_LOGIN_USERNAME}" &&\
    conan remote add nexus https://nexus.epa-dev.net/repository/conan-center-proxy true --force &&\
    conan profile new default --detect &&\
    conan profile update settings.compiler.libcxx=libstdc++11 default &&\
    conan profile update settings.compiler.cppstd=17 default &&\
    conan profile update settings.compiler.version=9 default &&\
    conan profile update env.CXX=g++-9 default &&\
    conan profile update env.CC=gcc-9 default

# Build Release variant:
FROM build AS release_build

RUN conan install .. --build missing
RUN cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DERP_WITH_HSM_MOCK=OFF \
    -DERP_BUILD_VERSION=${ERP_BUILD_VERSION} \
    -DERP_RELEASE_VERSION=${ERP_RELEASE_VERSION} \
    -DCMAKE_INSTALL_PREFIX=/erp .. && \
    cmake --build . -j 6 --target production && \
    cmake --install . --strip --component production &&\
    cp install_manifest_production.txt install_manifest.txt

COPY docker/manifest.template /erp/bin/erp-processing-context.manifest
COPY docker/create_graphene_manifest.sh /build/erp-processing-context/release

RUN chmod 755 /build/erp-processing-context/release/create_graphene_manifest.sh && \
    /build/erp-processing-context/release/create_graphene_manifest.sh \
    /build/erp-processing-context/release/install_manifest.txt \
    erp-processing-context \
    /graphene/graphene \
    /erp/bin/erp-processing-context.manifest

RUN tar -czf ./erp-processing-context.tar.gz /erp

# Build RelWithDebInfo variant:
FROM build AS reldeb_build

WORKDIR /build/erp-processing-context/reldeb
RUN conan install .. --build missing
RUN cmake \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DERP_WITH_HSM_MOCK=OFF \
    -DERP_BUILD_VERSION=${ERP_BUILD_VERSION} \
    -DERP_RELEASE_VERSION=${ERP_RELEASE_VERSION} \
    -DCMAKE_INSTALL_PREFIX=/debug/erp .. && \
    cmake --build . -j 6 --target production && \
    cmake --install . --strip --component production &&\
    cp install_manifest_production.txt install_manifest.txt

COPY docker/manifest.reldeb.template /debug/erp/bin/erp-processing-context.manifest
COPY docker/create_graphene_manifest.sh /build/erp-processing-context/reldeb

RUN chmod 755 /build/erp-processing-context/reldeb/create_graphene_manifest.sh && \
    /build/erp-processing-context/reldeb/create_graphene_manifest.sh \
    /build/erp-processing-context/reldeb/install_manifest.txt \
    erp-processing-context \
    /graphene/graphene \
    /debug/erp/bin/erp-processing-context.manifest

RUN tar -czf ./erp-processing-context-debug.tar.gz /debug/erp

# Build runtime image here:
FROM ubuntu:focal AS runtime

RUN apt-get update --fix-missing

WORKDIR /debug/erp
COPY --from=reldeb_build /build/erp-processing-context/reldeb/erp-processing-context-debug.tar.gz .
COPY --from=reldeb_build /debug/erp .

WORKDIR /erp
COPY --from=release_build /build/erp-processing-context/release/erp-processing-context.tar.gz .
COPY --from=release_build /erp .

WORKDIR /
COPY docker/start_erp.sh /start_erp.sh
RUN chmod 755 /start_erp.sh

ENTRYPOINT ["/start_erp.sh"]
