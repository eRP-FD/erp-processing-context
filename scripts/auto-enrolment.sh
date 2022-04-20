#!/bin/bash

# (C) Copyright IBM Deutschland GmbH 2022
# (C) Copyright IBM Corp. 2022

HERE=$(cd $(dirname $0) && pwd -P)
ENROLMENT_HELPER=$HERE/enrolment-helper.sh
MAX_ATTEMPTS=60
RETRY_SLEEP_SECONDS=10

sleep 30
EXPECTED_VERSION=$("$ENROLMENT_HELPER" version)

for PROCESSING_CONTEXT in {1..2}; do
    for ((ATTEMPT=1; ATTEMPT <= MAX_ATTEMPTS ; ++ATTEMPT)); do
        echo "Checking health for processing context $PROCESSING_CONTEXT version $EXPECTED_VERSION (attempt $ATTEMPT)"
        HEALTH=$("$ENROLMENT_HELPER" "health$PROCESSING_CONTEXT")
        RESULT=$?
        if [[ $RESULT == 0 && -n "$HEALTH" ]]; then
            VERSION=$(echo "$HEALTH" | jq -r ".version.build")
            if [[ "$VERSION" == "$EXPECTED_VERSION" ]]; then
                echo "$HEALTH"
                GENERAL_STATUS=$(echo "$HEALTH" | jq -r ".status")
                TEE_TOKEN_STATUS=$(echo "$HEALTH" | jq -r '.checks[] | select(.name | contains("TeeTokenUpdater")) | .status')
                if [[ $GENERAL_STATUS == "DOWN" && $TEE_TOKEN_STATUS == "DOWN" ]]; then
                    echo "Enrolling processing context $PROCESSING_CONTEXT"
                    "$ENROLMENT_HELPER" "pc$PROCESSING_CONTEXT" static
                    # Enrolling a quote requires an Attestation Key
                    "$ENROLMENT_HELPER" "pc$PROCESSING_CONTEXT" ak
                    if "$ENROLMENT_HELPER" "pc$PROCESSING_CONTEXT" quote; then
                        break
                    fi
                else
                    echo "Processing context $PROCESSING_CONTEXT is already enrolled"
                    break
                fi
            else
                echo "Got health status from unexpected version $VERSION"
            fi
        else
            echo "Health check failed for processing context $PROCESSING_CONTEXT"
        fi

        sleep $RETRY_SLEEP_SECONDS
    done
done

# keep container running
trap : TERM INT
sleep infinity &
wait
