#
# (C) Copyright IBM Deutschland GmbH 2021, 2025
# (C) Copyright IBM Corp. 2021, 2025
#
# non-exclusively licensed to gematik GmbH
#
set(legacy_files
    CodeSystem-v2-0074.json
    CodeSystem-v2-0116.json
    CodeSystem-v2-0131.json
    CodeSystem-v2-0203.json
    CodeSystem-v2-0276.json
    CodeSystem-v2-0373.json
    CodeSystem-v2-0493.json
    CodeSystem-v2-0916.json
    ValueSet-v2-0116.json
    ValueSet-v2-0136.json
    ValueSet-v2-0276.json
    ValueSet-v2-0493.json
    ValueSet-v2-0916.json
)
set(updated_files
    CodeSystem-diagnosis-role.json
    CodeSystem-operation-outcome.json
)

# Movecodes used in legacy validation to separate folder
set(source_dir "${FHIR_CURRENT_PACKAGE_DIR}/package")
set(target_dir "${FHIR_CURRENT_PACKAGE_DIR}/package-legacy-validation")
file(MAKE_DIRECTORY "${target_dir}")
foreach(file ${legacy_files} ${updated_files})
    file(RENAME "${source_dir}/${file}" "${target_dir}/${file}")
endforeach()
