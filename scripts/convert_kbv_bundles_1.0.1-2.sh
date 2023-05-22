#!/bin/bash -e

# (C) Copyright IBM Deutschland GmbH 2021, 2023
# (C) Copyright IBM Corp. 2021, 2023
#
# non-exclusively licensed to gematik GmbH

#########################################################
#
#  Replace Version Numbers in KBV-Bundles
#
#
#########################################################

usage() {
cat <<EOF
Usage:
 convert_kbv_bundles_1.0.1-2.sh <dir>

 <dir> directory with KBV-Bundles in XML
EOF
}

if [ "$#" -ne 1 ] ; then
    usage >&2
    exit 1
fi

case "$1" in
    --help|-h)
        usage
        exit 0
        ;;
esac

FILEDIR="$1"

if [ ! -d "$FILEDIR" ] ; then
    echo "no such directory: $FILEDIR" >&2
    usage >&2
    exit 1
fi

PROFILES=(
    "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Bundle"
    "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Composition"
    "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Medication_PZN"
    "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Prescription"
    "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Medication_FreeText"
    "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Medication_Ingredient"
    "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Medication_Compounding"
)

COMMAND=(sed -i)

for p in "${PROFILES[@]}" ; do
    COMMAND=("${COMMAND[@]}" -e "s;$p|1.0.1;$p|1.0.2;g")
done

find "${FILEDIR}" -iname '*.xml' -type f -print0 |
while IFS='' read -d '' bundle_file; do
    "${COMMAND[@]}" "$bundle_file"
done
