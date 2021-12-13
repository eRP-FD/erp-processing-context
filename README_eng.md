# erp-processing-context
TEE processing context for the ePrescription (**eR**eze**p**t, erp) service.

TEE = trusted execution environment or german VAU = vertrauenswürdige Ausführungsumgebung.

# Project Setup

See [here](doc/ProjectSetup.md)

# Building project

See [here](doc/Building.md) for details related to building the project and updating the necessary test resources including `TSL_valid.xml` and `BNA_valid.xml`.

# The Outside World
Communication with the outside world
- incoming HTTP requests
- PostgreSQL
- HSM
- registration service
- remote attestation service

# Implementation
A [guide](doc/GuideToImplementation.md) outlines the implementation.

# Notes
The test key/certificate pair in `resources/test/02_development.config.json.in` (erp/processing-context/server/certificate)
was generated on macOS Catalina 10.15.1 using OpenSSL 3.0.0-dev.

They are meant to be used exclusively for testing purposes on a server running locally.

```shell
 openssl req -newkey rsa:2048 -nodes -keyout key.pem \
              -x509 -days 3650 -out cert.pem \
              -subj "/C=DE/ST=HH/L=Hamburg/O=IBM/OU=Gesundheitsplattform" \
                    "/CN=ePA Backend Mock -- FdV-Modul Unit Testing" \
              -addext "subjectAltName = DNS:127.0.0.1"
 ```

# Build image
```$xslt
cd docker/build
docker build -t de.icr.io/erp_dev/erp-pc-ubuntu-build:0.0.3 .
docker push de.icr.io/erp_dev/erp-pc-ubuntu-build:0.0.3
```

# Tools
## JWT signing tool `jwt`

This tool uses the private key located in the source tree at `resources/test/jwt/idp_id` to sign a json-claim file
provided at the command line and prints it to _stdout_.  

```
Usage: jwt <claimfile>

<claimfile>   file containing claim to sign
```

## VAU Request encryption tool `vau_encrypt`
This tool uses the key from `vau/private-key` in `02_development.config.json` or environment variable `ERP_VAU_PRIVATE_KEY`
to create an encrypted request.

```
Usage: vau_encrypt <infile> <outfile>
<infile>      name of file with plain text request
<outfile>     target file for encrypted request
```

## Create PKCS7 bundles on command line:
in directory resources/test/EndpointHandlerTest
```
cat kbv_bundle.xml| openssl smime -sign -signer ../ssl/ec.crt -inkey ../ssl/ec.priv.pem -outform der -nodetach |base64 -w0  >kbv_bundle.xml.p7s
```
