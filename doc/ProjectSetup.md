# Project Setup

We are using
- C++17
- target system is Ubuntu 18.10<br>
  That was a choice forced by the use of Fortanix. Graphene may have different restrictions.
- GCC 9 
- libraries (so far)
  - boost :: beast (HTTPS server and client)
  - boost :: asio
  - pqxx (PostgreSQL)
  - rapidjson
  - libxml2
  - openSSL
  - GLog
- Conan as package manager
- CMake
- git(hub) with master branch
- Builds on Jenkins
- static code analysis with Sonarcube
- dependency analysis warns about vulnerabilities in libraries
- testing with Google Test

The erp pc will be a regular application, not a docker image. More than one application will be started on a single machine.
Docker and Kubernetes may be used during development.

Graphene adds a Linux-as-a-library and may impose restrictions.

Packages you must have installed on Linux or WSL:
- autoconf
- libtool
