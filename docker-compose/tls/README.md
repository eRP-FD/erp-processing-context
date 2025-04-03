## Initial setup
Populate the current folder with files generated with the script located in `scripts/generate_certs.sh`
from `ibm:eRp/postgres-docker`.
The script generates a root and a client certificate which may be used by postgres, redis and any other component
which requires certificates. The certificates are for testing purposes and contain no confidential
information.

Usage:
```shell
cd $PROJECT_ROOT/erp-processing-context/docker-compose/tls
generate_certs.sh `pwd`
```

Result:
```shell
rootCA.crt
rootCA.key
rootCA.srl

server.crt
server.csr
server.key
```

It's assumed that the project directories `erp-processing-context` and `postgres-docker`
are located in $PROJECT_ROOT.
