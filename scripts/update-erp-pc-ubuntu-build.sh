#!/usr/bin/env bash

#
# (C) Copyright IBM Deutschland GmbH 2021, 2026
# (C) Copyright IBM Corp. 2021, 2026
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
if [ -f ../docker/build-image/Dockerfile ]; then
    cp ../docker/build-image/Dockerfile "$tmpdir/Dockerfile"
    cp -rf ../docker/build-image/files "$tmpdir/"
elif [ -f docker/build-image/Dockerfile ]; then
    cp docker/build-image/Dockerfile "$tmpdir/Dockerfile"
    cp -rf docker/build-image/files "$tmpdir/"
else
    echo "please call this script from the cmake build dir or from the project root dir"
    exit
fi

echo "fetch latest version of de.icr.io/erp_dev/ubuntu-noble"
docker pull de.icr.io/erp_dev/ubuntu-noble:latest

echo "running docker build"
docker build --pull -t erp-pc-ubuntu-build:$version "$tmpdir"
docker image ls | grep erp-pc-ubuntu-build
docker tag erp-pc-ubuntu-build:$version de.icr.io/erp_dev/erp-pc-ubuntu-build:$version

ibmcloud cr login
docker push de.icr.io/erp_dev/erp-pc-ubuntu-build:$version
