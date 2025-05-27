#
# (C) Copyright IBM Deutschland GmbH 2021, 2025
# (C) Copyright IBM Corp. 2021, 2025
#
# non-exclusively licensed to gematik GmbH
#

find_package(Boost REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(RapidJSON REQUIRED)
find_package(date REQUIRED)
find_package(glog REQUIRED)
find_package(gsl-lite REQUIRED)
find_package(libxml2 REQUIRED)
find_package(magic_enum REQUIRED)

set(VAU_ENC_SOURCES vau_encrypt/vau_encrypt_main.cxx)

include_directories(${CMAKE_SOURCE_DIR}) # "tools/Header.hxx"

add_library(erp-tool-lib EXCLUDE_FROM_ALL OBJECT ${ERP_TOOL_LIB_SOURCES})
target_compile_definitions(erp-tool-lib PUBLIC WITH_HSM_MOCK=1)
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
    PRIVATE
       LibXml2::LibXml2
       boost::boost
       date::date
       glog::glog
       gsl::gsl-lite
       magic_enum::magic_enum
       openssl::openssl
       rapidjson
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
            erp-hsm-tpm-mock
            erp-processing-context-files
            magic_enum::magic_enum
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


add_executable(tee3send EXCLUDE_FROM_ALL tee3send/tee3send_main.cxx)
target_link_libraries(tee3send
    PRIVATE
        erp-medication-exporter-files
        boost::boost
)

add_custom_target(tools DEPENDS make_task_id fhirconvert fhirvalidate tee3send resources)

add_subdirectory(fhirinstall)
