#
# (C) Copyright IBM Deutschland GmbH 2021, 2025
# (C) Copyright IBM Corp. 2021, 2025
#
# non-exclusively licensed to gematik GmbH
#

find_program(GZIP_EXECUTABLE gzip REQUIRED)
add_custom_command(OUTPUT "public/cdn.min.js"
    DEPENDS "src/cdn.min.js.gz"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/public"
    COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_SOURCE_DIR}/src/cdn.min.js.gz" "${CMAKE_CURRENT_BINARY_DIR}/public"
    COMMAND "${GZIP_EXECUTABLE}" --force --decompress "${CMAKE_CURRENT_BINARY_DIR}/public/cdn.min.js.gz"
)

add_custom_target(erp-fhir-ws-files DEPENDS "public/cdn.min.js")

add_dependencies(resources erp-fhir-ws-files)

install(
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/public" "${CMAKE_CURRENT_BINARY_DIR}/public"
    DESTINATION "shared/erp-fhir-ws"
    COMPONENT erp-fhir-ws
)
