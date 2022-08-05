# erp-processing-context
TEE processing context für den Dienst **eR**eze**p**t (erp).

TEE = trusted execution environment oder deutsch VAU = vertrauenswürdige Ausführungsumgebung.

# Project Setup

Siehe [hier](doc/ProjectSetup.md)

# Bau des Projectes

Siehe [hier](doc/Building.md) für Einzelheiten zur Erstellung des Projekts und zur Aktualisierung der erforderlichen Testressourcen einschließlich `TSL_valid.xml` und `BNA_valid.xml`.

# Verbindungen zu anderen Komponenten

- eingehende HTTP-Anfragen
- PostgreSQL
- HSM
- registration service (Redis)
- remote attestation service

# Implementation
Eine [Anleitung](doc/GuideToImplementation.md) beschreibt die Implementierung.

# Hinweise
Das test key/certificate-Paar in `resources/test/02_development.config.json.in` (erp/processing-context/server/certificate)
wurde auf macOS Catalina 10.15.1 mit OpenSSL 3.0.0-dev erzeugt.

Sie sind ausschließlich für Testzwecke auf einem lokal betriebenen Server gedacht.

```shell
 openssl req -newkey rsa:2048 -nodes -keyout key.pem \
              -x509 -days 3650 -out cert.pem \
              -subj "/C=DE/ST=HH/L=Hamburg/O=IBM/OU=Gesundheitsplattform" \
                    "/CN=ePA Backend Mock -- FdV-Modul Unit Testing" \
              -addext "subjectAltName = DNS:127.0.0.1"
 ```

# Build Environment im Docker-Image
```shell
cd docker/build
docker build -t de.icr.io/erp_dev/erp-pc-ubuntu-build:2.0.1 .
docker push de.icr.io/erp_dev/erp-pc-ubuntu-build:2.0.1
```

# Tools
## JWT-Signatur-Tool `jwt`

Dieses Werkzeug verwendet den Private Key, der sich im Quellbaum unter `resources/test/jwt/idp_id` befindet, um eine json-claim-Datei zu signieren, welche
 in der Befehlszeile angegeben wird, und gibt sie auf _stdout_ aus.  

```
Usage: jwt <claimfile>

<claimfile>   Datei mit claims die signiert werden sollen
```

## VAU-Anfrage-Verschlüsselungs-Tool `vau_encrypt`
Dieses Werkzeug verwendet den Key aus `vau/private-key` in `02_development.config.json` oder die Umgebungsvariable `ERP_VAU_PRIVATE_KEY`
um eine verschlüsselte Anfrage zu erstellen.

```
Usage: vau_encrypt <infile> <outfile>
<infile>      Name einer Datei mit Klartext-Anfrage (Request)
<outfile>     Ausgabedatei für die verschlüsselte Anfrage
```

## Erstellung eines PKCS7 bundles auf der Kommandozeile:
In Verzeichnis resources/test/EndpointHandlerTest
```
cat kbv_bundle.xml| openssl smime -sign -signer ../ssl/ec.crt -inkey ../ssl/ec.priv.pem -outform der -nodetach |base64 -w0  >kbv_bundle.xml.p7s
```
