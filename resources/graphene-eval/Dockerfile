################################################################################
# Begin
FROM ubuntu:18.04

################################################################################
# Setup base image
RUN apt update && \
apt install -y \
    build-essential cpio git wget unzip flex bison libelf-dev libssl-dev bc autoconf libtool cmake python \
    libcurl4-openssl-dev debhelper lsb-release ocaml-nox ocamlbuild protobuf-c-compiler libprotobuf-c-dev \
    protobuf-compiler libprotobuf-dev kmod linux-headers-generic joe initramfs-tools reprepro gawk

################################################################################
# Prepare work environment
SHELL ["/bin/bash", "-c"]
ENV KERNEL 5.4.82
ENV GRAPHENE_DIR /sgx/graphene
RUN git config --global user.email "root@localhost" # Setup git (required when patching the kernel)
RUN git config --global user.name "root"
RUN groupadd sgx_prv
RUN mkdir /sgx && cd /sgx
WORKDIR /sgx
# # Variant 1: clone / Download sources.
RUN git clone --depth 1 https://github.com/oscarlab/graphene.git
RUN git clone --depth 1 https://github.com/oscarlab/graphene-sgx-driver
RUN git clone --depth 1 https://github.com/intel/SGXDataCenterAttestationPrimitives.git
RUN git clone --depth 1 https://github.com/intel/linux-sgx.git
RUN git clone --depth 1 https://github.com/intel/linux-sgx-driver.git
RUN wget https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git/snapshot/linux-$KERNEL.tar.gz
# Variant 2: transfer sources from host.
# ADD ./SGXDataCenterAttestationPrimitives /sgx/SGXDataCenterAttestationPrimitives
# ADD ./graphene /sgx/graphene
# ADD ./graphene-sgx-driver /sgx/graphene-sgx-driver
# COPY ./linux-5.4.82.tar.gz /sgx
# ADD ./linux-sgx /sgx/linux-sgx
# ADD ./linux-sgx-driver /sgx/linux-sgx-driver

################################################################################
# Patch, configure, build and install kernel and its modules
RUN cd /sgx
WORKDIR /sgx
RUN tar xf linux-$KERNEL.tar.gz
WORKDIR /sgx/linux-$KERNEL
RUN git init && git add . && git commit -m 'initial import' . # In order to apply patches with git
RUN git am /sgx/graphene-sgx-driver/fsgsbase_patches/*.patch
COPY config-5.4.82 .config
RUN touch .scmversion # Get rid of the + at the end of the kernel version (marks "dirty" kernels).
RUN yes '' | make oldconfig
RUN make -j `getconf _NPROCESSORS_ONLN`
RUN make modules -j `getconf _NPROCESSORS_ONLN`
RUN make modules_install
RUN cp arch/x86/boot/bzImage /boot/vmlinuz-$KERNEL
RUN cp .config /boot/config-$KERNEL
RUN cp System.map /boot/System.map-$KERNEL
RUN update-initramfs -c -k $KERNEL
RUN mkdir -p /lib/modules/$KERNEL/kernel/drivers/intel/sgx

################################################################################
# Build and install the DCAP SGX driver and the plain SGX driver
RUN cd /sgx/SGXDataCenterAttestationPrimitives/driver/linux
WORKDIR /sgx/SGXDataCenterAttestationPrimitives/driver/linux
RUN make
RUN cp intel_sgx.ko /lib/modules/$KERNEL/kernel/drivers/intel/sgx
RUN cp 10-sgx.rules /etc/udev/rules.d
RUN cd /sgx/linux-sgx-driver
WORKDIR /sgx/linux-sgx-driver
RUN make
RUN cp /sgx/linux-sgx-driver/isgx.ko /lib/modules/$KERNEL/kernel/drivers/intel/sgx

################################################################################
# Build SGX SDK and PSW
ENV PATH /sgx/linux-sgx/external/toolset/ubuntu18.04:$PATH
RUN export PATH=/sgx/linux-sgx/external/toolset/ubuntu18.04:$PATH
RUN cd /sgx/linux-sgx
WORKDIR /sgx/linux-sgx
RUN make preparation
RUN make sdk
RUN make sdk_install_pkg
RUN ./linux/installer/bin/sgx_linux_x64_sdk_2.12.100.3.bin -prefix /opt/intel
RUN source /opt/intel/sgxsdk/environment
RUN make psw
RUN make deb_psw_pkg
RUN make deb_local_repo
RUN echo "deb [trusted=yes arch=amd64] file:/sgx/linux-sgx/linux/installer/deb/sgx_debian_local_repo bionic main" > /etc/apt/sources.list
RUN apt update
RUN apt install -y libsgx-launch libsgx-urts libsgx-epid libsgx-urts libsgx-quote-ex libsgx-urts libsgx-dcap-ql


################################################################################
# Build graphene
RUN cd /sgx/graphene-sgx-driver
WORKDIR /sgx/graphene-sgx-driver
ENV ISGX_DRIVER_PATH /sgx/linux-sgx-driver
RUN ./link-intel-driver.py
# # Pass the path to the proper driver, which is: ~/sgx/linux-sgx-driver
# # Now sgx.h and gsgx.h are generated.
RUN cp -v /sgx/graphene-sgx-driver/gsgx.h /sgx/graphene/Pal/src/host/Linux-SGX
RUN cp -v /sgx/graphene-sgx-driver/sgx.h /sgx/graphene/Pal/src/host/Linux-SGX
RUN cd /sgx/graphene
WORKDIR /sgx/graphene
RUN make SGX=1

################################################################################
# End
RUN cd /
