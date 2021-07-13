# Dockerfile for Graphene / SGX-enabled Ubuntu
The graphene software stack is composed of various components which
require specific instructions to build. The Dockerfile is based on
Ubuntu 18.04 but probably will need to be adopted for Redhat
distributions. For now, the Dockerfile enables the user to quickly
build an Ubuntu image with SGX enabled. SGX enabled means, the patched
GNU/Linux kernel together with the SGX Tools and graphene itself.

The sources are downloaded from their official git repositories. The
kernel is based on 5.4.82 because the kernel patches seem to work only
with the 5.4.y branch.

A complete build takes about 20-30 minutes (varying because several
packages need to be downloaded and how many cores were available for work)
on a 16-core/32-thread machine.

