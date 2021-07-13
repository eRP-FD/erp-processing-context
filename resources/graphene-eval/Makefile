# Build erp-processing-context as follows:
#
# - make               -- create non-SGX no-debug-log manifest
# - make SGX=1         -- create SGX no-debug-log manifest
# - make SGX=1 DEBUG=1 -- create SGX debug-log manifest
#
# Use `make clean` to remove Graphene-generated files.

$(info HOME: $(HOME))
GRAPHENEDIR ?= $(HOME)/graphene
$(info GRAPHENEDIR: $(GRAPHENEDIR))
SGX_SIGNER_KEY ?= $(GRAPHENEDIR)/Pal/src/host/Linux-SGX/signer/enclave-key.pem
BINDIR = cmake-build-debug/bin

ifeq ($(DEBUG),1)
GRAPHENEDEBUG = inline
else
GRAPHENEDEBUG = none
endif

DISTRIB_ID ?= $(shell lsb_release --short --id)
DISTRIB_RELEASE ?= $(shell lsb_release --short --release)

ifeq ($(SGX), 1)
ifneq ($(DISTRIB_ID),Ubuntu)
$(error This example requires Ubuntu when building for SGX.)
endif
endif

UBUNTU_VERSION = $(DISTRIB_ID)$(DISTRIB_RELEASE)
$(info UBUNTU_VERSION: $(UBUNTU_VERSION))

.PHONY: all
all: erp-processing-context configuration.json erp-processing-context.manifest | pal_loader
ifeq ($(SGX),1)
all: erp-processing-context.token
endif

include $(GRAPHENEDIR)/Scripts/Makefile.configs

$(info ARCH_LIBDIR: $(ARCH_LIBDIR))

############################ erp-processing-context EXECUTABLE ##################

# The template file contains almost all necessary information to run the exe
# under Graphene / Graphene-SGX. We create the manifest (to be run under
# non-SGX Graphene) by simply replacing variables in the template file via sed.
erp-processing-context.manifest: erp-processing-context.manifest.template
	sed -e 's|$$(GRAPHENEDIR)|'"$(GRAPHENEDIR)"'|g' \
		-e 's|$$(GRAPHENEDEBUG)|'"$(GRAPHENEDEBUG)"'|g' \
		-e 's|$$(ARCH_LIBDIR)|'"$(ARCH_LIBDIR)"'|g' \
		-e 's|# '"$(UBUNTU_VERSION)"' ||g' \
		-e 's|$$(HOME)|'"$(HOME)"'|g' \
		$< > $@

# Manifest for Graphene-SGX requires special "pal-sgx-sign" procedure. This
# procedure measures all Memcached dependencies (shared libraries and trusted
# files), measures Memcached code/data pages, adds measurements in the
# resulting manifest.sgx file (among other, less important SGX options) and
# creates memcached.sig (SIGSTRUCT object).
#
# Graphene-SGX requires EINITTOKEN and SIGSTRUCT objects (see SGX hardware ABI,
# in particular EINIT instruction). The "pal-sgx-get-token" script generates
# EINITTOKEN based on a SIGSTRUCT and puts it in .token file. Note that
# filenames must be the same as the executable/manifest name (i.e., "memcached").
# EINITTOKEN must be generated on the machine where the application will run, not
# where it was built.

erp-processing-context.manifest.sgx: erp-processing-context.manifest erp-processing-context
	$(GRAPHENEDIR)/Pal/src/host/Linux-SGX/signer/pal-sgx-sign \
		-libpal $(GRAPHENEDIR)/Runtime/libpal-Linux-SGX.so \
		-key $(SGX_SIGNER_KEY) \
		-manifest $< -output $@ \
		-exec erp-processing-context

erp-processing-context.sig: erp-processing-context.manifest.sgx

erp-processing-context.token: erp-processing-context.sig
	$(GRAPHENEDIR)/Pal/src/host/Linux-SGX/signer/pal-sgx-get-token \
		-output erp-processing-context.token -sig erp-processing-context.sig

########################### COPIES OF EXECUTABLES #############################

erp-processing-context: $(BINDIR)/erp-processing-context
	cp $< $@

configuration.json: $(BINDIR)/configuration.json
	cp $< $@

# Create a link to pal_loader for simplicity.

pal_loader:
	ln -s $(GRAPHENEDIR)/Runtime/pal_loader $@

################################## CLEANUP ####################################

.PHONY: clean
clean:
	$(RM) *.token *.sig *.manifest.sgx *.manifest erp-processing-context pal_loader
