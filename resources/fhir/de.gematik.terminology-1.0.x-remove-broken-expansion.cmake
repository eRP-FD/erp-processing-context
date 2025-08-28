#
# (C) Copyright IBM Deutschland GmbH 2021, 2025
# (C) Copyright IBM Corp. 2021, 2025
#
# non-exclusively licensed to gematik GmbH
#

set(files
    package/ValueSet-ti-medication-dispense-status-vs.json
    package/ValueSet-ti-medication-request-status-vs.json
)
list(TRANSFORM files PREPEND "${FHIR_CURRENT_PACKAGE_DIR}/")

foreach(file ${files})
    file(REMOVE "${file}.orig")
    file(RENAME "${file}" "${file}.orig")
    file(READ "${file}.orig" content NEWLINE_CONSUME)
    string(JSON fixed_content REMOVE "${content}" "expansion")
    file(WRITE "${file}" "${fixed_content}")
endforeach()
