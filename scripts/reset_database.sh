#!/usr/bin/env bash

#
# (C) Copyright IBM Deutschland GmbH 2021, 2025
# (C) Copyright IBM Corp. 2021, 2025
#
# non-exclusively licensed to gematik GmbH
#

set -e

HERE="$(cd $(dirname $0); pwd -P)"

PSQL_BIN="/usr/bin/psql"

DB_NAME=postgres
if [ "$#" = 5 ]; then
    DB_HOST=$1
    DB_PORT=$2
    DB_USER=$4
    DB_PASSWORD=$5
else
    DB_HOST=localhost
    DB_PORT=9005
    DB_USER=erp_admin
    read -p "Password ${DB_USER}@${DB_HOST}:" -s DB_PASSWORD
fi

echo

PSQL_ARGS=("--no-password" "--no-readline" "--echo-errors" "--quiet" "--set=ON_ERROR_STOP=1")

unset passfile

delpassfile() {
    if [ -n "${passfile}" ] ; then
        rm --preserve-root -f "${passfile}"
    fi
    passfile=""
}

die() {
    echo "$@" >&2
    exit 1
}

run_psql() {
    test -z "$passfile"
    trap delpassfile QUIT
    set +x
    passfile="$(umask 077 && mktemp -t psql_pass_XXXXXXXXXX)"
    echo "${DB_HOST}:${DB_PORT}:${DB_NAME}:${DB_USER}:$DB_PASSWORD" > "${passfile}"
    DB_CONNECT="host='${DB_HOST}' port='${DB_PORT}' user='${DB_USER}' passfile='${passfile}' dbname='${DB_NAME}'"
    set +e
    "${PSQL_BIN}" "${PSQL_ARGS[@]}" "${DB_CONNECT}" "$@"
    local result="$?"
    set -e
    delpassfile
    trap - QUIT
    return $result
}

apply_migrations() {
    local -a migrations
    find "${HERE}/sql" -maxdepth 1 -type f -name 'migrate_*' -print0 \
    | sort --zero-terminated --version-sort \
    | while IFS= read -d '' migrationFile ; do
        local name="$(basename "${migrationFile}")"
        echo -e "\nApplying: ${name} \n"
        run_psql -f "${migrationFile}" || die "Migration failed: ${name}"
    done
}

echo "Creating database."
run_psql -f "${HERE}/sql/create_database_and_roles.sql" || die "Database creation failed."

# after now use the newly created database
if [ "$#" = 5 ]; then
    DB_NAME=$3
else
    DB_NAME="erp_processing_context"
fi

echo -e "\nCreating tables."
run_psql -f "${HERE}/sql/erp_partitioned.sql" || die "Table creation failed."
echo -e "\nSetting additional grants."
run_psql -f "${HERE}/sql/additional_grants.sql" || die "Failed to setup additional grants."

apply_migrations

echo -e "\nEnable logical replication for events."
run_psql -f "${HERE}/sql/enable_event_replication.sql" || die "Enable logical replication failed."

echo -e "\nEnable the creation of events."
run_psql -f "${HERE}/sql/enable_event_creation.sql" || die "Enable event creation failed."

echo -e "\nDatabase setup complete.\n"
