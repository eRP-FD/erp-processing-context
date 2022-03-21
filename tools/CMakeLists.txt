#
# (C) Copyright IBM Deutschland GmbH 2021
# (C) Copyright IBM Corp. 2021
#

set(ERP_TOOL_LIB_SOURCES
    jwt/JwtBuilder.cxx
    ResourceManager.cxx
)

set(JWT_SOURCES
    jwt/jwt_main.cxx
)

set(VAU_ENC_SOURCES vau_encrypt/vau_encrypt_main.cxx)

set(RESOURCE_SOURCE_DIR "${CMAKE_SOURCE_DIR}/resources")
set(RESOURCE_BINARY_DIR "${CMAKE_BINARY_DIR}/bin/resources")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/tool_config.hxx.in" "${CMAKE_CURRENT_BINARY_DIR}/tool_config.hxx")

add_library(erp-tool-lib EXCLUDE_FROM_ALL OBJECT ${ERP_TOOL_LIB_SOURCES})
target_compile_definitions(erp-tool-lib PUBLIC WITH_HSM_MOCK=1)
target_include_directories(erp-tool-lib
    PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}"
        ${PROJECT_SOURCE_DIR}   # Allow a leading `test/` or `util/` in include paths.
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(erp-tool-lib
    PUBLIC
        erp-hsm-tpm-mock
        erp-processing-context-files
    )

add_executable(jwt EXCLUDE_FROM_ALL ${JWT_SOURCES})
target_link_libraries(jwt
    PUBLIC
        erp-tool-lib
        erp-test-lib
        erp-processing-context-files
)
add_executable(vau_encrypt EXCLUDE_FROM_ALL ${VAU_ENC_SOURCES})
target_link_libraries(vau_encrypt
    PUBLIC
        erp-tool-lib
        erp-test-lib
        erp-processing-context-files
)
target_include_directories (
    vau_encrypt
        PRIVATE ${PROJECT_SOURCE_DIR}/..   # Allow a leading `test/` or `util/` in include paths.
)

add_executable(make_task_id EXCLUDE_FROM_ALL make_task_id/make_task_id.cxx)
target_link_libraries(make_task_id
        PUBLIC
        erp-processing-context-files
)

# The blob-db-initialization relies on HSM and TPM support. On platforms where they are not supported, the tool can
# not be built.
if(ERP_WITH_HSM_TPM_PRODUCTION)
    add_executable(
        blob-db-initialization
            blob-db-initialization/BlobDbInitialization.cxx
            )
    target_link_libraries(
        blob-db-initialization
        PRIVATE
            erp-mock-enrollmentmanager
            erp-processing-context-files
    )
    target_include_directories(
        blob-db-initialization
            PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..  # Allow a leading `tools/` in include paths.
    )
    install_libs_for(blob-db-initialization blob-db-initialization)
    install(TARGETS blob-db-initialization COMPONENT blob-db-initialization)
endif()

add_executable(fhirconvert EXCLUDE_FROM_ALL fhirconvert/fhirconvert_main.cxx)
target_link_libraries(fhirconvert
        PUBLIC
        erp-processing-context-files
)

add_custom_target(tools DEPENDS jwt vau_encrypt make_task_id fhirconvert resources)


