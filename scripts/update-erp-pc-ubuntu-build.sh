#!/usr/bin/env bash

#
# (C) Copyright IBM Deutschland GmbH 2021, 2025
# (C) Copyright IBM Corp. 2021, 2025
#
# non-exclusively licensed to gematik GmbH
#

repo=`pwd`
tmpdir=$(mktemp -d)
version=$1

if [ "x$version" == "x" ]; then
    echo "please provide a version"
    exit
fi

echo $tmpdir
if [ -f ../docker/build/Dockerfile ]; then
    cp ../docker/build/Dockerfile "$tmpdir/Dockerfile"
elif [ -f docker/build/Dockerfile ]; then
    cp docker/build/Dockerfile "$tmpdir/Dockerfile"
else
    echo "please call this script from the cmake build dir or from the project root dir"
    exit
fi

if [ -f ../conanfile.txt ]; then
    cp ../conanfile.txt "$tmpdir/conanfile.txt"
elif [ -f conanfile.txt ]; then
    cp conanfile.txt "$tmpdir/conanfile.txt"
else
    echo "please call this script from the cmake build dir or from the project root dir"
    exit
fi



echo "fetch latest version of ubuntu:jammy"
docker pull ubuntu:jammy

echo "running docker build"
docker build -t erp-pc-ubuntu-build:$version "$tmpdir"
docker image ls | grep erp-pc-ubuntu-build
docker tag erp-pc-ubuntu-build:$version de.icr.io/erp_dev/erp-pc-ubuntu-build:$version

ibmcloud cr login
docker push de.icr.io/erp_dev/erp-pc-ubuntu-build:$version
