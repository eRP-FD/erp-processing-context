#
# (C) Copyright IBM Deutschland GmbH 2021, 2025
# (C) Copyright IBM Corp. 2021, 2025
#
# non-exclusively licensed to gematik GmbH
#

set(updated_files
    CodeSystem-immunization-recommendation-status.json
    CodeSystem-insurance-plan-type.json
    CodeSystem-synthesis-type.json
    CodeSystem-v2-0560.json
    CodeSystem-v2-tables.json
    CodeSystem-v3-CodeSystem.json
    CodeSystem-v3-RoleClass.json
)

# Movecodes used in legacy validation to separate folder
set(source_dir "${FHIR_CURRENT_PACKAGE_DIR}/package")
set(target_dir "${FHIR_CURRENT_PACKAGE_DIR}/package-6.3.0-updated")
file(MAKE_DIRECTORY "${target_dir}")
foreach(file ${updated_files})
    file(RENAME "${source_dir}/${file}" "${target_dir}/${file}")
endforeach()
