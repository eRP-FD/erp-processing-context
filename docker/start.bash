#!/bin/bash

#
# (C) Copyright IBM Deutschland GmbH 2021, 2025
# (C) Copyright IBM Corp. 2021, 2025
#
# non-exclusively licensed to gematik GmbH
#

if [ -z $ERP_BUILD ]; then
     ERP_BUILD="release"
fi

BINARY=erp-processing-context
if [ -z $ERP_EXPORTER ] || [ $ERP_EXPORTER = 0 ]; then
	BINARY=erp-processing-context
else
	BINARY=erp-medication-exporter
fi

if [ $ERP_BUILD = "reldeb" ]; then
    echo "*** Launching $BINARY RelWithDebInfo variant"
    cd /debug/erp/bin
else
    echo "*** Launching $BINARY Release variant"
    cd /erp/bin
fi

if [ -n "$ERP_LAUNCHER" ]; then
    if ! command -v "$ERP_LAUNCHER" >/dev/null 2>&1; then
        echo "Error: ERP_LAUNCHER='$ERP_LAUNCHER' not found in PATH."
        exit 1
    fi
    exec "$ERP_LAUNCHER" $ERP_LAUNCHER_ARGS "$BINARY"
else
    if [ -z "$ERP_STANDALONE" ] || [ "$ERP_STANDALONE" = "0" ]; then
        exec gramine-direct "$BINARY"-kubernetes
    else
        exec "./$BINARY"
    fi
fi
