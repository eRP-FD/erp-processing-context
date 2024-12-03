#!/bin/bash

#
# (C) Copyright IBM Deutschland GmbH 2021, 2024
# (C) Copyright IBM Corp. 2021, 2024
#
# non-exclusively licensed to gematik GmbH
#

usage() {
  echo -e "\nUsage: $0 version|pc1|pc2|pc3|health1|health2|health3  [<blob_type>]\n       <blob_type> only required for pc1/pc2/pc3"
}

if [  $# -lt 1 ]
then
  usage
  exit 1
fi

if [[ ( $1 == "--help") ||  $1 == "-h" ]]
then
  usage
  exit 0
fi

PC=$1
shift

# extract namespace from host variable
IFS='.'
read -ra PARTS <<< "$ERP_SERVER_HOST"
IFS=' '
NAMESPACE=${PARTS[1]}

command="./blob-db-initialization -c /erp/vau-hsm/client/test/resources/saved/cacertecc.crt -s /erp/vau-hsm/client/test/resources/saved $*"
healthpc1="curl https://erp-processing-context-1.${NAMESPACE}.svc.cluster.local:9085/health -k --silent | jq"
healthpc2="curl https://erp-processing-context-2.${NAMESPACE}.svc.cluster.local:9086/health -k --silent | jq"
healthpc3="curl https://erp-medication-exporter.${NAMESPACE}.svc.cluster.local:9999/health -k --silent | jq"

case $PC in

  pc1)
    echo "Running enrolment for erp-processing-context-1\n" >&2
    export ERP_SERVER_HOST=erp-processing-context-1.${NAMESPACE}.svc.cluster.local
    export TPM_SERVER_NAME=tpm-simulator-1.${NAMESPACE}.svc.cluster.local
    echo "$command" >&2
    eval $command
    ;;

  pc2)
    echo "Running enrolment for erp-processing-context-2\n" >&2
    export ERP_SERVER_HOST=erp-processing-context-2.${NAMESPACE}.svc.cluster.local
    export TPM_SERVER_NAME=tpm-simulator-2.${NAMESPACE}.svc.cluster.local
    echo "$command" >&2
    eval $command
    ;;

 pc3)
    echo "Running enrolment for erp-medication-exporter\n" >&2
    export ERP_SERVER_HOST=erp-medication-exporter.${NAMESPACE}.svc.cluster.local
    export TPM_SERVER_NAME=tpm-simulator-2.${NAMESPACE}.svc.cluster.local
    echo "$command" >&2
    eval $command
    ;;

  health1)
    echo "Running health check against erp-processing-context-1\n" >&2
    echo "$healthpc1" >&2
    eval $healthpc1
    ;;

  health2)
    echo "Running health check against erp-processing-context-2\n" >&2
    echo "$healthpc2" >&2
    eval $healthpc2
    ;;

  health3)
    echo "Running health check against erp-medication-exporter\n" >&2
    echo "$healthpc3" >&2
    eval $healthpc3
    ;;

  version)
      ./blob-db-initialization --version
      ;;

  *)
    echo "ERROR: unknown command" >&2
    exit 1
    ;;
esac

exit 0
