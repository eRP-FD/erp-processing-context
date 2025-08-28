set(files
    package/StructureDefinition-epa-op-cancel-dispensation-erp-input-parameters.json
    package/StructureDefinition-epa-op-cancel-prescription-erp-input-parameters.json
    package/StructureDefinition-epa-op-provide-dispensation-erp-input-parameters.json
    package/StructureDefinition-epa-op-provide-prescription-erp-input-parameters.json
    package/StructureDefinition-epa-op-rx-dispensation-erp-output-parameters.json
    package/StructureDefinition-epa-op-rx-prescription-erp-output-parameters.json
)

set(patch_file "${CURRENT_SOURCE_DIR}/fhir/de.gematik.epa.medication-${FHIR_PACKAGE_VERSION}-fix-part-type.patch")

list(TRANSFORM files PREPEND "${FHIR_CURRENT_PACKAGE_DIR}/")

foreach(file ${files})
    file(REMOVE "${file}.orig")
    file(RENAME "${file}" "${file}.orig")
    file(READ "${file}.orig" content NEWLINE_CONSUME)
    string(REPLACE "}," "},\n" with_newlines "${content}")
    file(WRITE "${file}.nl" "${with_newlines}")
    file(WRITE "${file}" "${with_newlines}")
    #file(REMOVE "${file}.nl")
endforeach()

execute_process(
    COMMAND "${Patch_EXECUTABLE}" "--quiet" "-d" "${FHIR_CURRENT_PACKAGE_DIR}/package" "-i" "${patch_file}"
    RESULT_VARIABLE result
)
if (result GREATER 0)
    message(FATAL_ERROR "patching failed")
endif()


