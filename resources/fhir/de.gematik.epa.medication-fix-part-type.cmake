#
# (C) Copyright IBM Deutschland GmbH 2021, 2025
# (C) Copyright IBM Corp. 2021, 2025
#
# non-exclusively licensed to gematik GmbH
#

set(files
    package/StructureDefinition-epa-op-cancel-dispensation-erp-input-parameters.json
    package/StructureDefinition-epa-op-cancel-prescription-erp-input-parameters.json
    package/StructureDefinition-epa-op-provide-dispensation-erp-input-parameters.json
    package/StructureDefinition-epa-op-provide-prescription-erp-input-parameters.json
    package/StructureDefinition-epa-op-rx-dispensation-erp-output-parameters.json
    package/StructureDefinition-epa-op-rx-prescription-erp-output-parameters.json
)

if ("${FHIR_PACKAGE_VERSION}" VERSION_GREATER_EQUAL "1.2.0")
    list(APPEND files package/StructureDefinition-epa-op-add-emp-entry-input-parameters.json)
endif()

list(TRANSFORM files PREPEND "${FHIR_CURRENT_PACKAGE_DIR}/")

include("${CURRENT_SOURCE_DIR}/fhir/fix-Parameters.parameter.part-type.cmake")

foreach(file ${files})
    fix_parameters_part_type(${file})
endforeach()


