#!/usr/bin/env bash

#
# (C) Copyright IBM Deutschland GmbH 2021
# (C) Copyright IBM Corp. 2021
#

COMMAND="$(basename "$0")"

LOCAL_VERSION=latest
VERSION=d-0.0.1
REGISTRY=de.icr.io/erp_dev
IMAGE_REGISTRY=de.icr.io
IMAGE_NAME_PC=erp-processing-context
IMAGE_NAME_ENROLMENTHELPER=blob-db-initialization
TAG=$IMAGE_NAME_PC
DOCKERFILE=docker/Dockerfile
NAMESPACE=erp-system
NEXUS_USERNAME="<nexus-user>"
NEXUS_PASSWORD="<nexus-password>"
GIT_USERNAME="<git-user>"
GIT_OAUTH_TOKEN="<git-oauth-token>"
RELEASE_VERSION=1.4.0
BUILD_VERSION=v-0.0.1

if [ $# -ne 2 ] && [ $# -ne 3 ]; then
    echo "usage: $COMMAND <version> <command> [<image>]"
    echo "Example: $COMMAND d-0.0.1 build"
    echo "Commands:"
    echo "  build:    build the service in a local Docker environment and prepare a Docker runtime image"
    echo "  tag:      tag the built docker image"
    echo "  push:     push the tagged Docker image to the ibmcloud registry"
    echo "image types:"
    echo "  pc:              build the erp-processing-context image"
    echo "  enrolmenthelper: build the enrolment helper image"
    echo
    echo "these environment variables have to be set in order to checkout files from nexus and github"
    echo "    NEXUS_USERNAME"
    echo "    NEXUS_PASSWORD"
    echo "    GIT_USERNAME"
    echo "    GIT_OAUTH_TOKEN"
    exit 1
fi
VERSION=$1

if [ -z $NEXUS_USERNAME ]; then
    echo "NEXUS_USERNAME is not set"
    exit 1
fi
if [ -z $NEXUS_PASSWORD ]; then
    echo "NEXUS_PASSWORD is not set"
    exit 1
fi
if [ -z $GIT_USERNAME ]; then
    echo "GIT_USERNAME is not set"
    exit 1
fi
if [ -z $GIT_OAUTH_TOKEN ]; then
    echo "GIT_OAUTH_TOKEN is not set"
    exit 1
fi


if [ ! -f scripts/build-docker-local.sh ]; then
    echo "Please run this script from the top level path of erp-processing-context"
    exit 1
fi

# create the docker image in 2 step docker build:
#   1. compile the service on ubuntu with necessary dependencies
#   2. copy the built service binary + dependencies in a new fresh ubuntu
function build_local_docker {
    echo "# Build the service and create a local docker image ..."
    local tmp_dir="$(mktemp -d -t erp-docker-XXXXXXXXXX)"
    cp -a                   \
       cmake                \
       $DOCKERFILE          \
       docker               \
       CMakeLists.txt       \
       conanfile.txt        \
       resources            \
       scripts              \
       src                  \
       test                 \
       tools                \
       "$tmp_dir/"
    echo "did set up docker environment in '$tmp_dir'"
    (cd "$tmp_dir" ; docker build -t ${TAG} . --build-arg CONAN_LOGIN_USERNAME="${NEXUS_USERNAME}" --build-arg CONAN_PASSWORD="${NEXUS_PASSWORD}" \
        --build-arg GITHUB_USERNAME="${GIT_USERNAME}" --build-arg GITHUB_OAUTH_TOKEN="${GIT_OAUTH_TOKEN}" \
        --build-arg ERP_BUILD_VERSION="${VERSION}" --build-arg ERP_RELEASE_VERSION="${RELEASE_VERSION}")
    local status=$?
    if [ "$status" != "0" ]; then
        echo "building the docker image failed with $status"
        exit $status
    fi
}


# tag the docker image
function tag_local_docker {
    echo "# Tagging the local docker image as ${REGISTRY}/${TAG}:${VERSION} ..."
    docker tag ${TAG}:${LOCAL_VERSION} ${REGISTRY}/${TAG}:${VERSION}
}

# push the tagged docker image to the cluster registry
function push_to_cluster_registry {
    echo "# Pushing ${REGISTRY}/${TAG}:${VERSION} to the cluster registry ..."
    ibmcloud cr login
    docker push ${REGISTRY}/${TAG}:${VERSION}
}

if [ $# -eq 3 ]
then
  case $3 in
    pc)
        TAG=$IMAGE_NAME_PC
        DOCKERFILE=docker/Dockerfile
        ;;
    enrolmenthelper)
        TAG=$IMAGE_NAME_ENROLMENTHELPER
        DOCKERFILE=docker/enrolment/Dockerfile
        ;;
    *)
        echo "ERROR: unknown image type $3"
        exit 1
        ;;
  esac
fi

echo "Build local image $TAG:${VERSION} ..."

case $2 in
    build)
        build_local_docker
        ;;
    tag)
        tag_local_docker
        ;;
    push)
        push_to_cluster_registry
        ;;
    *)
        echo "ERROR: unknown command $2"
        exit 1
        ;;
esac
