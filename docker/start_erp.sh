#!/bin/bash

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
exec ./erp-processing-context
