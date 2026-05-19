#
# (C) Copyright IBM Deutschland GmbH 2021, 2025
# (C) Copyright IBM Corp. 2021, 2025
#
# non-exclusively licensed to gematik GmbH
#

function(fix_parameters_part_type file)
    execute_process(
        COMMAND "${cmake_EXECUTABLE}" "-E" "rename" "${file}" "${file}.orig"
        RESULT_VARIABLE result
    )
    if (result GREATER 0)
        message(FATAL_ERROR "rename failed: ${file}")
    endif()
    execute_process(
        COMMAND "${jq_EXECUTABLE}" "-f" "${CURRENT_SOURCE_DIR}/fhir/fix-Parameters.parameter.part-type.jq" "${file}.orig"
        OUTPUT_FILE "${file}"
        RESULT_VARIABLE result
    )
    if (result GREATER 0)
        message(FATAL_ERROR "patching failed: ${file}")
    endif()
endfunction()

if (DEFINED ERP_FIX_PARAMETERS_PART_TYPE_FILES)
    list(TRANSFORM ERP_FIX_PARAMETERS_PART_TYPE_FILES PREPEND "${FHIR_CURRENT_PACKAGE_DIR}/")
    foreach(file ${ERP_FIX_PARAMETERS_PART_TYPE_FILES})
        fix_parameters_part_type("${file}")
    endforeach()
endif()
