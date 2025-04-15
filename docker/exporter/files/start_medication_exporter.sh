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

if [ $ERP_BUILD = "reldeb" ]; then
    echo "*** Launching RelWithDebInfo variant"
    cd /debug/erp-exporter/bin
else
    echo "*** Launching Release variant"
    cd /erp-exporter/bin
fi

exec ./erp-medication-exporter
