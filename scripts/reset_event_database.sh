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
elif [ "$#" = 10 ]; then
    DB_HOST=$1
    DB_PORT=$2
    DB_USER=$4
    DB_PASSWORD=$5
    DB_NAME_MAIN=$6
    DB_HOST_MAIN=$7
    DB_PORT_MAIN=$8
    DB_USER_MAIN=$9
    DB_PASSWORD_MAIN=${10}
else
    DB_HOST=localhost
    DB_PORT=9006
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
    find "${HERE}/sql-event-db" -maxdepth 1 -type f -name 'migrate_*' -print0 \
    | sort --zero-terminated --version-sort \
    | while IFS= read -d '' migrationFile ; do
        local name="$(basename "${migrationFile}")"
        echo -e "\nApplying: ${name} \n"
        run_psql -f "${migrationFile}" || die "Migration failed: ${name}"
    done
}

echo "Creating database."
run_psql -f "${HERE}/sql-event-db/create_database_and_roles.sql" || die "Database creation failed."

# after now use the newly created database
if [ "$#" = 5 ]; then
    DB_NAME=$3
elif [ "$#" = 10 ]; then
    DB_NAME=$3
else
    DB_NAME="erp_event_db"
fi

echo -e "\nCreating tables."
run_psql -f "${HERE}/sql-event-db/erp_event.sql" || die "Table creation failed."
echo -e "\nSetting additional grants."
run_psql -f "${HERE}/sql-event-db/additional_grants.sql" || die "Failed to setup additional grants."

apply_migrations

if [ "$#" = 10 ]; then
    echo -e "\nSetting subscription."
    CONNECTION="dbname=${DB_NAME_MAIN} host=${DB_HOST_MAIN} port=${DB_PORT_MAIN} user=${DB_USER_MAIN} password=${DB_PASSWORD_MAIN} sslmode=require"
    run_psql -f "${HERE}/sql-event-db/erp_event_subscription.sql" -v connection="${CONNECTION}" || die "Failed to setup subscription."
else
     echo -e "\Skip setting subscription."
fi

echo -e "\nDatabase setup complete.\n"