
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


function(fhir_install package version)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs PACKAGE_FIX_SCRIPT DEPENDS)
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

    set(package_fix_arguments)
    if (DEFINED arg_PACKAGE_FIX_SCRIPT)
        list(POP_FRONT arg_PACKAGE_FIX_SCRIPT script)
        set(script_arguments ${arg_PACKAGE_FIX_SCRIPT})
        set(package_fix_arguments
            COMMAND "${CMAKE_COMMAND}"
                    "-DSOURCE_DIR=${CMAKE_SOURCE_DIR}"
                    "-DBINARY_DIR=${CMAKE_BINARY_DIR}"
                    "-DCURRENT_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
                    "-DCURRENT_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}"
                    "-DFHIR_CURRENT_PACKAGE_DIR=${package_dir}"
                    "-DFHIR_PACKAGE_VERSION=${version}"
                    ${script_arguments}
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/${script}"
            DEPENDS ${script})

    endif()

    add_custom_command(
        COMMENT "Extracting ${archive_filename}"
        OUTPUT ${files}
        VERBATIM
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${package_dir}"
        COMMAND "${CMAKE_COMMAND}" -E chdir "${package_dir}" "${CMAKE_COMMAND}" -E tar xJf "${archive}"
        ${package_fix_arguments}
        DEPENDS "${archive}" ${arg_DEPENDS}
    )

    add_custom_target("${target}-extract" DEPENDS ${files})

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
        DEPENDS "${target}-extract"
        DEPENDS ${ERP_HL7_ORG_DEFINITIONS_FILES}
        DEPENDS configuration
    )

    add_custom_target(${target} DEPENDS "${output_dir}")

    add_dependencies(resources ${target})

endfunction()
