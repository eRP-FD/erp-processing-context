#!/bin/bash

#
# (C) Copyright IBM Deutschland GmbH 2021
# (C) Copyright IBM Corp. 2021
#

if [ -z $ERP_BUILD ]; then
     ERP_BUILD="release"
fi

if [ $ERP_BUILD = "reldeb" ]; then
    echo "*** Launching RelWithDebInfo variant"
    cd /debug/erp/bin
else
    echo "*** Launching Release variant"
    cd /erp/bin
fi

# Replace launching shell with erp-pc by calling with exec:
if [ -z "$ERP_STANDALONE" ] || [ "$ERP_STANDALONE" = 0 ]; then
    exec gramine-direct erp-processing-context-kubernetes
else
    exec ./erp-processing-context
fi
