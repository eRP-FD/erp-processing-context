# docker-compose.yml
Orchestration script for working with all components required for the erp-processing-context.

## Initial setup
First step: see `tls/README.md`. You may use the file `03_docker-compose.config.json` which uses adjusted connection settings for
postgres and redis. Place the file in your folder where erp-processing-context resides and adjust the `certificatePath` for redis
to point to the generated rootCA.crt file.

## Further Info
For simplicity, the redis server container is using the same certificates as postgres.
The `docker-compose/redis/db` and `docker-compose/redis/log` paths are mapped into the container. They contain the redis database
and redis log. The same applies to the configuration file `redis.conf`. It should not need any adjustments.

The yml script references the source folder of the erp-processing-context sources as `erp-processing-context`.

Almost all simulators and servers export a port in case you want to connect to anything for introspection, starting from 9000.

The TPM server is launched in `socsim` (simulation mode.)
