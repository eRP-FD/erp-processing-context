#
# (C) Copyright IBM Deutschland GmbH 2021, 2025
# (C) Copyright IBM Corp. 2021, 2025
#
# non-exclusively licensed to gematik GmbH
#

# Add configuration in `bin` folder
#
# Usage: add_erp_config(<target> TEMPLATE <template_file> COMPONENTS <component...>|NO_INSTALL)
#
# <target>                  target to create for config generation (must be unique)
#
# TEMPLATE <template_file>  input file with placeholders - outputfile will have the last extension removed
#
# COMPONENTS <component...> list of components for installation
#
# NO_INSTALL                indicates that the config should not be installed (mutually exclusive with COMPONENTS)
#
function(add_erp_config target)
    set(options OPTIONAL NO_INSTALL)
    set(oneValueArgs TEMPLATE)
    set(multiValueArgs COMPONENTS)
    cmake_parse_arguments(PARSE_ARGV 0 arg "${options}" "${oneValueArgs}" "${multiValueArgs}")

    if (NOT arg_COMPONENTS AND NOT arg_NO_INSTALL OR arg_COMPONENTS AND arg_NO_INSTALL)
        message(SEND_ERROR "add_erp_config: must provide either COMPONENTS or NO_INSTALL")
    endif()
    find_file(install_config NAMES install-config.cmake PATHS "${CMAKE_SOURCE_DIR}/cmake" NO_CACHE NO_DEFAULT_PATH REQUIRED)
    find_file(template NAMES "${arg_TEMPLATE}" PATHS ${CMAKE_CURRENT_SOURCE_DIR} NO_CACHE NO_DEFAULT_PATH REQUIRED)
    get_filename_component(output_name "${arg_TEMPLATE}" NAME_WLE)
    set(builddir_output_name "${CMAKE_BINARY_DIR}/bin/${output_name}")
    add_custom_command(
        OUTPUT "${builddir_output_name}"
        DEPENDS "${template}"
        DEPENDS "${install_config}"
        COMMAND "${CMAKE_COMMAND}"
                    -DERP_RUNTIME_RESOURCE_DIR="${CMAKE_BINARY_DIR}/bin/resources"
                    -DERP_CONFIGURATION_FILE_IN="${template}"
                    -DERP_CONFIGURATION_FILE="${builddir_output_name}"
                    -P "${install_config}"
    )
    foreach (component ${arg_COMPONENTS})
        install(CODE
            "set(ERP_RUNTIME_RESOURCE_DIR \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/${CMAKE_PROJECT_NAME}\") \n\
             set(ERP_CONFIGURATION_FILE_IN \"${template}\") \n\
             set(ERP_CONFIGURATION_FILE \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/${output_name}\") \n\
             include(\"${install_config}\")"
            COMPONENT "${component}"
        )
    endforeach()
    if (NOT TARGET resources)
        add_custom_target(resources ALL )
    endif()
    add_custom_target(${target} DEPENDS "${builddir_output_name}")
    add_dependencies(resources "${target}")
endfunction()
