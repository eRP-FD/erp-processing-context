# Instructions
This folder provides Dockerfiles and helper scripts to build and execute the erp-processing-context (erp-pc) server.

## Dockerfile
The main Dockerfile for cloning, building, packaging and executing the erp-pc server. You have to provide build arguments in order to access the conan and github repositories.

#### Build instructions:
```shell
docker build                                            \
    --build-arg GITHUB_USERNAME="<username>"            \
    --build-arg GITHUB_OAUTH_TOKEN="<auth_token>"       \
    --build-arg CONAN_LOGIN_USERNAME="<username_conan>" \
    --build-arg CONAN_PASSWORD="<password_conan>"       \
    -t erp-processing-context:local                     \
    -f docker/Dockerfile .
```
This Dockerfile expects a build image and a runtime image. These are explained below.

In order to run erp-pc with gramine, additional steps are performed in the Dockerfile with the helper script create_graphene_manifest.sh and the manifest templates (one for the Release and one for the ReleaseWithDebugInfo symbols build.)
The script creates a manifest file for graphene which is copied from the template files and extended. The manifest file is required during runtime with gramine.

The start_erp.sh shell script is a wrapper for providing a docker entrypoint.

### Build image
The build image is used during the build and provides a fixed and constant build environment. The Dockerfille for creating the build image is placed in the docker/build folder.

#### Build instructions:
```shell
docker build -t de.icr.io/erp_dev/erp-pc-ubuntu-build:1.0.0 -f docker/build/Dockerfile .
```
The name of the image (in this case: erp-pc:build) must be adopted in the main Dockerfile.

### Runtime image
The runtime image provides a basic environment, based on Ubuntu 20.04 LTS. A sample runtime environment is provided for running erp-pc with gramine-direct. Ths Dockerfile for this runtime image is placed in the docker/gramine-runtime folder.
The files/ folder contains the downloaded keyring for the gramine package. The keyring can be downloaded / updated from: [gramine-keyring.gpg](https://packages.gramineproject.io/gramine-keyring.gpg "gramine-keyring.gpg").

#### Build instructions:
```shell
docker build -t de.icr.io/erp_dev/rt-gramine-direct:0.0.1 -f docker/gramine-runtime/Dockerfile .
```
The name of the image (in this case: erp-pc:rt-gramine-direct) must be adopted in the main Dockerfile.
