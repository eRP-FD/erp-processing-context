#!/bin/bash

#
# (C) Copyright IBM Deutschland GmbH 2021
# (C) Copyright IBM Corp. 2021
#

usage() {
  echo -e "\nUsage: $0 pc1|pc2|health1|health2 [<blob_type>]\n       <blob_type> only required for pc1/pc2"
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
healthpc1="curl https://erp-processing-context-1.${NAMESPACE}.svc.cluster.local:9085/health -k | jq"
healthpc2="curl https://erp-processing-context-2.${NAMESPACE}.svc.cluster.local:9086/health -k | jq"

case $PC in

  pc1)
    echo "Running enrolment for erp-processing-context-1\n"
    export ERP_SERVER_HOST=erp-processing-context-1.${NAMESPACE}.svc.cluster.local
    export TPM_SERVER_NAME=tpm-simulator-1.${NAMESPACE}.svc.cluster.local
    echo "$command"
    eval $command
    ;;

  pc2)
    echo "Running enrolment for erp-processing-context-2\n"
    export ERP_SERVER_HOST=erp-processing-context-2.${NAMESPACE}.svc.cluster.local
    export TPM_SERVER_NAME=tpm-simulator-2.${NAMESPACE}.svc.cluster.local
    echo "$command"
    eval $command
    ;;

  health1)
    echo "Running health check against erp-processing-context-1\n"
    echo "$healthpc1"
    eval $healthpc1
    ;;

  health2)
    echo "Running health check against erp-processing-context-2\n"
    echo "$healthpc2"
    eval $healthpc2
    ;;

  *)
    echo "ERROR: unknown command"
    exit 1
    ;;
esac

exit 0 
