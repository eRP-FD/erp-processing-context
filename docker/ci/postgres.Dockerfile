FROM de.icr.io/erp_dev/erp-postgres-db:master

# COPY docker-compose/scripts/postgres-entrypoint.sh /postgres-entrypoint.sh
# COPY tls/server.key tls/server.crt /tls/

HEALTHCHECK --timeout=1s --start-interval=0.5s --start-period=30s --retries=3 --interval=3s  \
    CMD psql -p 5432 -U erp_admin -d "erp_processing_context" -c '\q' || exit 1

# RUN install -o postgres -m 600 /tls/server.key /erp/secrets/server.key && \
#     install -o postgres -m 644 /tls/server.crt /erp/secrets/server.crt

USER postgres
SHELL [ "/bin/bash", "-c" ]
RUN echo "erp0815" > /tmp/dbpasswd && \
    initdb --auth=trust \
           -U 'erp_admin' \
           --pwfile=/tmp/dbpasswd \
           -c ssl=on \
           -c ssl_cert_file=/erp/secrets/server.crt \
           -c ssl_key_file=/erp/secrets/server.key \
           -c wal_level=logical \
           -c log_statement=none && \
    source /usr/local/bin/docker-entrypoint.sh && \
    POSTGRES_HOST_AUTH_METHOD=trust pg_setup_hba_conf

COPY scripts/reset_database.sh /scripts/
COPY scripts/sql /scripts/sql/
RUN pg_ctl -D /var/lib/postgresql/data start && \
    scripts/reset_database.sh localhost 5432 erp_processing_context erp_admin erp0815 && \
    pg_ctl -D /var/lib/postgresql/data stop
