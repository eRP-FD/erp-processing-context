#
# (C) Copyright IBM Deutschland GmbH 2021, 2025
# (C) Copyright IBM Corp. 2021, 2025
#
# non-exclusively licensed to gematik GmbH
#
set(files
    package/StructureDefinition-GEM-ERPEU-PR-PAR-Close-Operation-Input.json
    package/StructureDefinition-GEM-ERPEU-PR-PAR-EU-GET-Prescription-Input.json
)

list(TRANSFORM files PREPEND "${FHIR_CURRENT_PACKAGE_DIR}/")

include("${CURRENT_SOURCE_DIR}/fhir/fix-Parameters.parameter.part-type.cmake")

foreach(file ${files})
    fix_parameters_part_type(${file})
endforeach()
