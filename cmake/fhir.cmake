
if (NOT TARGET resources)
    add_custom_target(resources)
endif()

if (NOT TARGET configuration)
    add_custom_target(configuration)
endif()

if (NOT TARGET fhirinstall)
    add_executable(fhirinstall EXCLUDE_FROM_ALL)
endif()

macro(fhir_substitute substitution)
    list(APPEND ERP_FHIR_SUBSTITUTION_ARGUMENTS "-s" "${substitution}")
endmacro()

set(ERP_FHIR_PATCH_ID 0)

find_program(jq_EXECUTABLE jq REQUIRED)
find_program(bash_EXECUTABLE bash REQUIRED)

function(fhir_patch package version)
    set(targetBase "${package}-${version}")
    set(patch_type "${ARGV2}")
    set(output_dir "${ERP_FHIR_RESOURCE_DIR}/profiles/${targetBase}")
    math(EXPR patchid "${ERP_FHIR_PATCH_ID} + 1")
    set(patchedmarker "${output_dir}/.patched-${patchid}")
    set(package_dir "${ERP_FHIR_REPOSITORY_DIR}/${package}#${version}")
    set(package_reference "${package}@${version}")


    if ("${patch_type}" STREQUAL "SCRIPT")
        set(package_fix_arguments)
        cmake_parse_arguments(PARSE_ARGV 2 arg "" "" "SCRIPT;DEPENDS")
        list(POP_FRONT arg_SCRIPT script)
        add_custom_command(
            COMMENT "Apply ${script} to package ${package_reference}"
            OUTPUT "${patchedmarker}"
            COMMAND "${CMAKE_COMMAND}"
                    "-Dcmake_EXECUTABLE=${CMAKE_COMMAND}"
                    "-DSOURCE_DIR=${CMAKE_SOURCE_DIR}"
                    "-DBINARY_DIR=${CMAKE_BINARY_DIR}"
                    "-DCURRENT_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
                    "-DCURRENT_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}"
                    "-DFHIR_CURRENT_PACKAGE_DIR=${package_dir}"
                    "-DFHIR_PACKAGE_VERSION=${version}"
                    ${arg_SCRIPT}
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/${script}"
            COMMAND "${CMAKE_COMMAND}" -E touch "${patchedmarker}"
            DEPENDS "${targetBase}-extract"
            DEPENDS ${script} ${arg_DEPENDS}
            VERBATIM
        )
    elseif("${patch_type}" STREQUAL "JQ")
        cmake_parse_arguments(PARSE_ARGV 2 arg "" "" "JQ;DEPENDS")
        list(POP_FRONT arg_JQ expression)
        list(POP_FRONT arg_JQ file)
        add_custom_command(
            COMMENT "Apply ${expression} to ${file} in package ${package_reference}"
            OUTPUT "${patchedmarker}"
            COMMAND "${CMAKE_COMMAND}" -E rename "${package_dir}/${file}" "${package_dir}/${file}.orig"
            COMMAND "${bash_EXECUTABLE}" -c "${jq_EXECUTABLE} '${expression}' '${package_dir}/${file}.orig' > '${package_dir}/${file}'"
            COMMAND "${CMAKE_COMMAND}" -E touch "${patchedmarker}"
            DEPENDS "${targetBase}-extract"
            DEPENDS ${arg_DEPENDS}
            VERBATIM
        )
    elseif("${patch_type}" STREQUAL "FIX_PARAMETERS_PART_TYPE")
        cmake_parse_arguments(PARSE_ARGV 2 arg "" "" "FIX_PARAMETERS_PART_TYPE;DEPENDS")
        add_custom_command(
            COMMENT "Fixing parameters.part.type in package ${package_reference}"
            OUTPUT "${patchedmarker}"
            COMMAND "${CMAKE_COMMAND}"
                    "-Dcmake_EXECUTABLE=${CMAKE_COMMAND}"
                    "-DSOURCE_DIR=${CMAKE_SOURCE_DIR}"
                    "-DBINARY_DIR=${CMAKE_BINARY_DIR}"
                    "-DCURRENT_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
                    "-DCURRENT_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}"
                    "-DFHIR_CURRENT_PACKAGE_DIR=${package_dir}"
                    "-DFHIR_PACKAGE_VERSION=${version}"
                    "-Djq_EXECUTABLE=${jq_EXECUTABLE}"
                    "-DERP_FIX_PARAMETERS_PART_TYPE_FILES=${arg_FIX_PARAMETERS_PART_TYPE}"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/fhir/fix-Parameters.parameter.part-type.cmake"
            COMMAND "${CMAKE_COMMAND}" -E touch "${patchedmarker}"
            DEPENDS "${targetBase}-extract"
            DEPENDS "fhir/fix-Parameters.parameter.part-type.jq"
            DEPENDS ${arg_DEPENDS}
            VERBATIM
        )
    else()
        message(FATAL_ERROR "unknown patch type: ${patch_type}")
    endif()
    add_custom_target("${targetBase}-patch${patchid}" DEPENDS ${patchedmarker})
    add_dependencies("${targetBase}-patched" "${targetBase}-patch${patchid}")
    if (TARGET "${targetBase}-patch${ERP_FHIR_PATCH_ID}")
        add_dependencies("${targetBase}-patch${patchid}" "${targetBase}-patch${ERP_FHIR_PATCH_ID}")
    endif()
    set(ERP_FHIR_PATCH_ID "${patchid}" PARENT_SCOPE)
endfunction()

function(fhir_install package version)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs DEPENDS)
    cmake_parse_arguments(PARSE_ARGV 0 arg "${options}" "${oneValueArgs}" "${multiValueArgs}")


    set(archive_filename "${package}-${version}.tgz")
    set(archive "${CMAKE_CURRENT_SOURCE_DIR}/fhir/${archive_filename}")
    set(package_reference "${package}@${version}")
    set(package_dir "${ERP_FHIR_REPOSITORY_DIR}/${package}#${version}")
    set(target "${package}-${version}")
    set(exclude_file_dir "${CMAKE_CURRENT_SOURCE_DIR}/fhir")
    set(output_dir "${ERP_FHIR_RESOURCE_DIR}/profiles/${target}")

    set_property(GLOBAL APPEND PROPERTY CONFIGURE_DEPENDS "${archive_filename}")

    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E tar tf "${archive}"
        OUTPUT_VARIABLE files
    )
    string(REGEX REPLACE "\n" ";" files "${files}")
    list(FILTER files INCLUDE REGEX "^package/")
    list(REMOVE_DUPLICATES files)
    list(TRANSFORM files PREPEND "${package_dir}/")


    add_custom_command(
        COMMENT "Extracting ${archive_filename}"
        OUTPUT ${files}
        VERBATIM
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${package_dir}"
        COMMAND "${CMAKE_COMMAND}" -E chdir "${package_dir}" "${CMAKE_COMMAND}" -E tar xJf "${archive}"
        MAIN_DEPENDENCY "${archive}"
        DEPENDS ${arg_DEPENDS}
    )

    # these two targets allow squeezing patch targets inbetween
    add_custom_target("${target}-extract" DEPENDS ${files})
    add_custom_target("${target}-patched" DEPENDS "${target}-extract")

    add_custom_command(
        COMMENT "Installing ${package_reference}"
        OUTPUT "${output_dir}"
        VERBATIM
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:fhirinstall>"
        COMMAND ${CMAKE_COMMAND} -E env "TEST_RESOURCE_MANAGER_PATH=" "ERP_STDERR_THRESHOLD=1"
            $<TARGET_FILE:fhirinstall>
                -p "${ERP_FHIR_REPOSITORY_DIR}"
                -o "${ERP_FHIR_RESOURCE_DIR}/profiles"
                -x "${exclude_file_dir}"
                -c "${ERP_FHIR_REPOSITORY_DIR}"
                ${ERP_FHIR_SUBSTITUTION_ARGUMENTS}
                -- "${package_reference}"
        DEPENDS "${target}-patched"
        DEPENDS ${ERP_HL7_ORG_DEFINITIONS_FILES}
        DEPENDS configuration
    )

    add_custom_target(${target} DEPENDS "${output_dir}")

    add_dependencies(resources ${target})

endfunction()
