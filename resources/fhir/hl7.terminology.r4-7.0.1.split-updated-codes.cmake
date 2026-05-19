#
# (C) Copyright IBM Deutschland GmbH 2021, 2025
# (C) Copyright IBM Corp. 2021, 2025
#
# non-exclusively licensed to gematik GmbH
#

set(updated_files
    CodeSystem-insurance-plan-type.json
    CodeSystem-service-category.json
    ValueSet-v2-0003.json
    ValueSet-v2-0338.json
    ValueSet-v2-0725.json
    CodeSystem-v3-ActRelationshipSubset.json
    CodeSystem-v3-styleType.json
    CodeSystem-v3-HL7DocumentFormatCodes.json
)

# Movecodes used in legacy validation to separate folder
set(source_dir "${FHIR_CURRENT_PACKAGE_DIR}/package")
set(target_dir "${FHIR_CURRENT_PACKAGE_DIR}/package-6.3.0-updated")
file(MAKE_DIRECTORY "${target_dir}")
foreach(file ${updated_files})
    file(RENAME "${source_dir}/${file}" "${target_dir}/${file}")
endforeach()
