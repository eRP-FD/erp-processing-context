## Initial setup
Populate this folder with files generated with the script located in `scripts/generecate_certs.sh` from `ibm:eRp/postgres-docker`.

Example:
```shell

cd $PROJECT_ROOT/erp-processing-context/docker-compose/tls
generate_certs.sh `pwd`
```

It's assumed that the directories `erp-processing-context` and `postgres-docker` are located in $PROJECT_ROOT.
