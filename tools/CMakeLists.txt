#
# (C) Copyright IBM Deutschland GmbH 2021, 2024
# (C) Copyright IBM Corp. 2021, 2024
#
# non-exclusively licensed to gematik GmbH
#

set(VAU_ENC_SOURCES vau_encrypt/vau_encrypt_main.cxx)

add_library(erp-tool-lib EXCLUDE_FROM_ALL OBJECT ${ERP_TOOL_LIB_SOURCES})
target_compile_definitions(erp-tool-lib PUBLIC WITH_HSM_MOCK=1)
target_include_directories(erp-tool-lib
    PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}"
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(erp-tool-lib
    PUBLIC
        erp-hsm-tpm-mock
        erp-processing-context-files
    )

add_executable(make_task_id EXCLUDE_FROM_ALL make_task_id/make_task_id.cxx)
target_link_libraries(make_task_id
    PUBLIC
        erp-processing-context-files
)

add_library(enrolment-api-client OBJECT
    EnrolmentApiClient.cxx)

target_link_libraries(enrolment-api-client
    PUBLIC
       CONAN_PKG::boost
       CONAN_PKG::date
       CONAN_PKG::glog
       CONAN_PKG::gsl-lite
       CONAN_PKG::libxml2
       CONAN_PKG::magic_enum
       CONAN_PKG::openssl
       CONAN_PKG::rapidjson
)

target_include_directories(enrolment-api-client
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_SOURCE_DIR}/src)

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
            erp-hsm-tpm-mock
            erp-processing-context-files
            CONAN_PKG::magic_enum
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

add_executable(fhirvalidate EXCLUDE_FROM_ALL fhirvalidate/fhirvalidate_main.cxx)
target_link_libraries(fhirvalidate
    PUBLIC
        erp-processing-context-files
        erp-test-util-lib
        erp-test-lib
        fhirtools
)


add_custom_target(tools DEPENDS make_task_id fhirconvert resources)
