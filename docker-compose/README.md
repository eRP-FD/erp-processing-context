# docker-compose.yml
Orchestration script for working with all components required for the erp-processing-context and the medication-exporter.

## Initial setup
First step: see `tls/README.md`. You may use the file `03_docker-compose.config.json` which uses adjusted connection settings for
postgres and redis. Place the file in your folder where the erp-processing-context binary resides and adjust the `certificatePath` for postgres and redis
to point to the generated rootCA.crt file.

## Further Info
For simplicity, the redis server container is using the same certificates as postgres.
The `docker-compose/redis/db` and `docker-compose/redis/log` paths are mapped into the container. They contain the redis database
and redis log. The same applies to the configuration file `redis.conf`. It should not need any adjustments.

The yml script references the source folder of the erp-processing-context sources as `erp-processing-context`.

Almost all simulators and servers export a port in case you want to connect to anything for introspection, starting from 9000.

## TPM server
The TPM server is launched in `socsim` (simulation mode.)

Following environment variables must be set in order to launch processes using TPM simulator:
```sh
TPM_INTERFACE_TYPE="socsim"
TPM_COMMAND_PORT="9002"
TPM_SERVER_TYPE="mssim"
TPM_SERVER_NAME="localhost"
TPM_PLATFORM_PORT="9003"
```

### Start all containers
```sh
docker-compose create
docker-compose start
```
Then observe database creation:
```sh
docker-compose logs -f db-schema
```

### Reset all containers
```sh
docker-compose down -v
```

### Enrollment:
Check out the latest Version of [`vau-hsm`](https://github.ibmgcloud.net/eRp/vau-hsm).


Set the following environment Variables:
```sh
cd <build_folder>/bin
export ERP_SERVER_HOST="localhost"
export TPM_INTERFACE_TYPE="socsim"
export TPM_COMMAND_PORT="9002"
export TPM_SERVER_TYPE="mssim"
export TPM_SERVER_NAME="localhost"
export TPM_PLATFORM_PORT="9003"
export DEBUG_ENABLE_HSM_MOCK=false
export DEBUG_DISABLE_REGISTRATION=true
export DEBUG_ENABLE_MOCK_TSL_MANAGER=true
```
Start `erp-processing-context` and wait until Enrollment-Server is ready.
Then, in another shell instance, start `blob-db-initialization` *with the same variables set*:
```sh
./blob-db-initialization -s <vau-hsm-checkout-folder>/client/test/resources/saved -h "$ERP_SERVER_HOST" all
```
Wait until `erp-processing-context` stops reporting `TeeTokenUpdater` and `C.FD.SIG-eRP` as DOWN.
```
WARNING ErpMain.cxx:395] main/: health status is DOWN (TeeTokenUpdater, C.FD.SIG-eRP), waiting for it to go up
```

### Connect to redis container:
```shell
$COMPOSE_YAML="<source_folder>/docker-compose"
redis-cli --tls \
    --cert "$COMPOSE_YAML/tls/server.crt" --key "$COMPOSE_YAML/tls/server.key" \
    -p 9004 --cacert "$COMPOSE_YAML/tls/rootCA.crt" -a welcome
```

### Connect to postgres container:

```shell
# erp-processing-context database
psql --username=erp_admin --dbname=erp_processing_context --host=<your host ip> --port=9005

# medication-exporter database
psql --username=erp_admin --dbname=erp_event_db --host=<your host ip> --port=9006

# when psql is not available:
docker run -ti --rm postgres:16 psql --username=erp_admin --dbname=erp_processing_context --host=<your host ip> --port=9005
docker run -ti --rm postgres:16 psql --username=erp_admin --dbname=erp_event_db --host=<your host ip> --port=9006
```
