#
# (C) Copyright IBM Deutschland GmbH 2021, 2025
# (C) Copyright IBM Corp. 2021, 2025
#
# non-exclusively licensed to gematik GmbH
#

cmake_minimum_required(VERSION 3.16)

find_package(Boost REQUIRED)
find_package(GTest REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(PostgreSQL REQUIRED)
find_package(RapidJSON REQUIRED)
find_package(ZLIB REQUIRED)
find_package(antlr4-runtime REQUIRED)
find_package(botan REQUIRED)
find_package(date REQUIRED)
find_package(glog REQUIRED)
find_package(gsl-lite REQUIRED)
find_package(hiredis REQUIRED)
find_package(libpqxx REQUIRED)
find_package(libunwind REQUIRED)
find_package(libxml2 REQUIRED)
find_package(magic_enum REQUIRED)
find_package(redis++ REQUIRED)
find_package(zstd REQUIRED)

project(erp-medication-exporter)

set(ERP_APP_NAME ${PROJECT_NAME})

set(ERP_ENROLMENT_DIR "${CMAKE_SOURCE_DIR}/resources/enrolment")

# Use gethostbyname() in boost for hostname lookups because
# the default case with getaddrinfo() causes pending open
# sockets within gramine which are not cleaned up. Sooner or
# later this behaviour consumes all available fds in the process
# and leads to failing lookups.

add_compile_definitions(BOOST_ASIO_DISABLE_GETADDRINFO)

# Helper macros to manage transition away from error_code return values.
add_compile_definitions(BOOST_ASIO_NO_DEPRECATED)

#
# Define how to build the exporter executable.
#
list(
    APPEND erp-medication-exporter-source-files
        Application.cxx
        BdeMessage.cxx
        BdeMessage.hxx
        Epa4AllTransformer.cxx
        EpaAccountLookup.cxx
        EpaAccountLookupClient.cxx
        EventProcessor.cxx
        MedicationExporterMain.cxx
        TelematikLookup.cxx
        TelematikLookup.hxx
        VauAutTokenSigner.cxx
        admin/PutRuntimeConfigHandler.cxx
        database/TaskEvent.cxx
        database/MainDatabaseFrontend.cxx
        database/MainDatabaseFrontend.hxx
        database/MainPostgresBackend.cxx
        database/MainPostgresBackend.hxx
        database/MedicationExporterDatabaseFrontend.cxx
        database/MedicationExporterDatabaseFrontendInterface.hxx
        database/MedicationExporterPostgresBackend.cxx
        database/TaskEventConverter.cxx
        model/ConsentDecisionsResponseType.cxx
        model/DataAbsentReason.cxx
        model/EventKvnr.cxx
        model/EpaOpCancelPrescriptionErpInputParameters.cxx
        model/EpaMedicationPznIngredient.cxx
        model/EpaMedicationTypeExtension.cxx
        model/EpaOpProvideDispensationErpInputParameters.cxx
        model/EpaOpProvidePrescriptionErpInputParameters.cxx
        model/EpaOpRxDispensationErpOutputParameters.cxx
        model/EpaOpRxPrescriptionErpOutputParameters.cxx
        model/HashedKvnr.cxx
        model/OrganizationDirectory.cxx
        model/ResourceInstances.cxx
        model/TaskEvent.cxx
        network/client/HttpsClientPool.cxx
        network/client/Tee3Client.cxx
        network/client/Tee3ClientPool.cxx
        network/client/Tee3ClientsForHost.cxx
        pc/MedicationExporterFactories.hxx
        pc/MedicationExporterServiceContext.cxx
        client/EpaMedicationClient.cxx
        client/EpaMedicationClientImpl.cxx
        eventprocessing/CancelPrescription.cxx
        eventprocessing/EventDispatcher.cxx
        eventprocessing/EventProcessingBase.cxx
        eventprocessing/Outcome.cxx
        eventprocessing/ProvideDispensation.cxx
        eventprocessing/ProvidePrescription.cxx
        model/ConsentDecisionsResponseType.cxx
        model/EpaErrorType.cxx
        model/EpaOperationOutcome.cxx
        server/HealthHandler.cxx
        server/HttpsServer.cxx
        server/ServerSession.cxx
        server/SessionContext.cxx
        tee3/EpaCertificateService.cxx
        util/ConfigurationFormatter.cxx
        util/RuntimeConfiguration.cxx
        util/health/HealthCheck.cxx
        )

add_library(
    erp-medication-exporter-files OBJECT
    ${erp-medication-exporter-source-files}
)

target_compile_definitions(erp-medication-exporter-files
    PUBLIC
        $<$<BOOL:${ERP_WITH_HSM_MOCK}>:WITH_HSM_MOCK=1>
        $<$<BOOL:${ERP_WITH_HSM_TPM_PRODUCTION}>:WITH_HSM_TPM_PRODUCTION=1>
)

target_link_libraries (
    erp-medication-exporter-files
        PUBLIC
            erp-shared
            tee3
            rapidjson
            boost::boost
            date::date
            glog::glog
            gsl::gsl-lite
            libpqxx::pqxx
            LibXml2::LibXml2
            magic_enum::magic_enum
)

add_executable(erp-medication-exporter erp-medication-exporter-main.cxx)
# Add -rdynamic to enrich the symbol table so that stack traces become more readable.
set_property(TARGET erp-medication-exporter PROPERTY ENABLE_EXPORTS ON)

target_link_libraries (
    erp-medication-exporter
        PUBLIC
            erp-medication-exporter-files
)

add_dependencies(erp-medication-exporter resources)


# warning is triggered somewhere in boost library - we cannot do anything about it. (boost 1.86.0 has the same issue still)
set_property(SOURCE network/client/Tee3Client.cxx PROPERTY COMPILE_OPTIONS -Wno-mismatched-new-delete)

install(TARGETS erp-medication-exporter COMPONENT production)
install_libs_for(erp-medication-exporter production)
if (NOT TARGET production)
    add_custom_target(production)
endif()
add_dependencies(production erp-medication-exporter)
