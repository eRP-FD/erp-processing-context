Installing FHIR-Validation Web-Service
======================================

Follow the build instructions to set up your build tree as described in [Building.md](../../doc/Building.md)

Then build the Web-Service using:
```sh
ninja erp-fhir-ws resources
```

Finally install with:

```sh
cmake --install <build-folder> --prefix <target-folder> --component erp-fhir-ws
```
