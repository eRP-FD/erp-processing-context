#
# (C) Copyright IBM Deutschland GmbH 2021, 2024
# (C) Copyright IBM Corp. 2021, 2024
#
# non-exclusively licensed to gematik GmbH
#

cmake_minimum_required(VERSION 3.16)

project(erp-medication-exporter)

set(ERP_APP_NAME ${PROJECT_NAME})

set(ERP_ENROLMENT_DIR "${CMAKE_SOURCE_DIR}/resources/enrolment")

# Use gethostbyname() in boost for hostname lookups because
# the default case with getaddrinfo() causes pending open
# sockets within gramine which are not cleaned up. Sooner or
# later this behaviour consumes all available fds in the process
# and leads to failing lookups.

add_compile_definitions(BOOST_ASIO_DISABLE_GETADDRINFO)

#
# Define how to build the eRP executable.
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
        VauAutTokenSigner.cxx
        admin/PutRuntimeConfigHandler.cxx
        database/ExporterDatabaseModel.cxx
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
        ../erp/crypto/SignedPrescription.cxx
        ../erp/database/ErpDatabaseModel.cxx
        ../erp/model/AbgabedatenPkvBundle.cxx
        ../erp/model/Binary.cxx
        ../erp/model/ChargeItem.cxx
        ../erp/model/Communication.cxx
        ../erp/model/CommunicationPayload.cxx
        ../erp/model/Composition.cxx
        ../erp/model/Consent.cxx
        ../erp/model/Device.cxx
        ../erp/model/ErxReceipt.cxx
        ../erp/model/Iknr.cxx
        ../erp/model/KbvBundle.cxx
        ../erp/model/KbvComposition.cxx
        ../erp/model/KbvCoverage.cxx
        ../erp/model/KbvMedicationCompounding.cxx
        ../erp/model/KbvMedicationFreeText.cxx
        ../erp/model/KbvMedicationIngredient.cxx
        ../erp/model/KbvMedicationPzn.cxx
        ../erp/model/KbvMedicationRequest.cxx
        ../erp/model/KbvOrganization.cxx
        ../erp/model/KbvPracticeSupply.cxx
        ../erp/model/KbvPractitioner.cxx
        ../erp/model/Lanr.cxx
        ../erp/model/Link.cxx
        ../erp/model/MedicationDispense.cxx
        ../erp/model/MedicationDispenseBundle.cxx
        ../erp/model/MedicationDispenseId.cxx
        ../erp/model/MetaData.cxx
        ../erp/model/OuterResponseErrorData.cxx
        ../erp/model/Patient.cxx
        ../erp/model/Pzn.cxx
        ../erp/model/Reference.cxx
        ../erp/model/ResourceInstances.cxx
        ../erp/model/Signature.cxx
        ../erp/model/Subscription.cxx
        ../erp/model/Task.cxx
        ../erp/model/WorkflowParameters.cxx
        ../erp/model/extensions/ChargeItemMarkingFlags.cxx
        ../erp/model/extensions/KBVEXERPAccident.cxx
        ../erp/model/extensions/KBVEXFORLegalBasis.cxx
        ../erp/model/extensions/KBVMedicationCategory.cxx
        ../erp/model/extensions/KBVMultiplePrescription.cxx
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

target_include_directories (
    erp-medication-exporter-files
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..
        PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/.. # Include the erp/ and fhirtools/ prefixes in include statements.
)

target_link_libraries (
    erp-medication-exporter-files
        PUBLIC erp-shared
        PUBLIC tee3
        PUBLIC CONAN_PKG::boost
        PUBLIC CONAN_PKG::date
        PUBLIC CONAN_PKG::glog
        PUBLIC CONAN_PKG::gsl-lite
        PUBLIC CONAN_PKG::openssl
        PUBLIC CONAN_PKG::rapidjson
        PUBLIC CONAN_PKG::libpq
        PUBLIC CONAN_PKG::libpqxx
        PUBLIC CONAN_PKG::libxml2
        PUBLIC CONAN_PKG::redis-plus-plus
        PUBLIC CONAN_PKG::hiredis
        PUBLIC CONAN_PKG::magic_enum
        PUBLIC CONAN_PKG::zstd
        PUBLIC CONAN_PKG::zlib
        PUBLIC $<$<BOOL:${ERP_WITH_HSM_MOCK}>:erp-hsm-tpm-mock>
        PUBLIC $<$<BOOL:${ERP_WITH_HSM_TPM_PRODUCTION}>:CONAN_PKG::hsmclient>
        PUBLIC $<$<BOOL:${ERP_WITH_HSM_TPM_PRODUCTION}>:CONAN_PKG::tpmclient>
        PUBLIC $<$<BOOL:${WITH_PROFILING}>:gperftools::profiler>
)

add_executable(erp-medication-exporter erp-medication-exporter-main.cxx)
# Add -rdynamic to enrich the symbol table so that stack traces become more readable.
set_property(TARGET erp-medication-exporter PROPERTY ENABLE_EXPORTS ON)

target_include_directories (
    erp-medication-exporter
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/.. # Include the erp/ and fhirtools/ prefixes in include statements.
)

target_link_libraries (
    erp-medication-exporter
        PUBLIC
            erp-medication-exporter-files
)

add_dependencies(erp-medication-exporter resources)


# warning is triggred somewhere in boost library - we cannot do anything about it. (boost 1.86.0 has the same issue still)
set_property(SOURCE network/client/Tee3Client.cxx PROPERTY COMPILE_OPTIONS -Wno-mismatched-new-delete)

install(TARGETS erp-medication-exporter COMPONENT production)
install_libs_for(erp-medication-exporter production)
if (NOT TARGET production)
    add_custom_target(production)
endif()
add_dependencies(production erp-medication-exporter)
