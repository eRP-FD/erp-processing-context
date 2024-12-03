#!/bin/bash

#
# (C) Copyright IBM Deutschland GmbH 2021, 2024
# (C) Copyright IBM Corp. 2021, 2024
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

# Replace launching shell with erp-pc by calling with exec:
if [ -z "$ERP_STANDALONE" ] || [ "$ERP_STANDALONE" = 0 ]; then
    exec gramine-direct $BINARY-kubernetes
else
    exec ./$BINARY
fi
