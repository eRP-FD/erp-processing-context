#!/usr/bin/env bash

set -e

HERE="$(cd $(dirname $0); pwd -P)"

DB_HOST=localhost
DB_PORT=5432
DB_NAME=erp
DB_USER=erp_admin
PSQL_BIN="/usr/bin/psql"

read -p "Password ${DB_USER}@${DB_HOST}:" -s DB_PASSWORD
echo

PSQL_ARGS=("--no-password" "--no-readline")

unset passfile

delpassfile() {
    if [ -n "${passfile}" ] ; then
        rm --preserve-root -f "${passfile}"
    fi
    passfile=""
}

run_psql() {
    test -z "$passfile"
    trap delpassfile QUIT
    set +x
    passfile="$(umask 077 && mktemp -t psql_pass_XXXXXXXXXX)"
    echo "${DB_HOST}:${DB_PORT}:${DB_NAME}:${DB_USER}:$DB_PASSWORD" > "${passfile}"
    set -x
    DB_CONNECT="host='${DB_HOST}' port='${DB_PORT}' user='${DB_USER}' passfile='${passfile}' dbname='${DB_NAME}'"
    "${PSQL_BIN}" "${PSQL_ARGS[@]}" "${DB_CONNECT}"
    delpassfile
    trap - QUIT
}

bytea() {
    local fileName="$1";
    echo -n "'\x";
    perl -ne 'print unpack "H*", $_' "$fileName"
    echo -n "'::bytea";
}

print_transaction() {
    local resource="$HERE/../resources/test/EndpointHandlerTest";

    echo -n "BEGIN;"

    cat "${HERE}/sql/erp_partitioned.sql";

    echo -n "COMMIT;"
}

print_transaction | run_psql


