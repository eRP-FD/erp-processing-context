#
# (C) Copyright IBM Deutschland GmbH 2021, 2024
# (C) Copyright IBM Corp. 2021, 2024
#
# non-exclusively licensed to gematik GmbH
#

cmake_minimum_required(VERSION 3.16)

project(erp-processing-context)

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
    APPEND erp-processing-context-source-files
        ErpMain.cxx
        ErpProcessingContext.cxx
        admin/AdminRequestHandler.cxx
        admin/AdminServer.cxx
        admin/PutRuntimeConfigHandler.cxx
        crypto/SignedPrescription.cxx
        database/Database.cxx
        database/DatabaseConnectionTimer.cxx
        database/DatabaseFrontend.cxx
        database/ErpDatabaseModel.cxx
        database/PostgresBackend.cxx
        database/PostgresBackendChargeItem.cxx
        database/PostgresBackendTask.cxx
        database/RedisClient.cxx
        database/redis/RateLimiter.cxx
        model/AbgabedatenPkvBundle.cxx
        model/Binary.cxx
        model/ChargeItem.cxx
        model/Communication.cxx
        model/CommunicationPayload.cxx
        model/Composition.cxx
        model/Consent.cxx
        model/Device.cxx
        model/ErxReceipt.cxx
        model/extensions/ChargeItemMarkingFlags.cxx
        model/extensions/KBVEXERPAccident.cxx
        model/extensions/KBVEXFORLegalBasis.cxx
        model/extensions/KBVMedicationCategory.cxx
        model/extensions/KBVMultiplePrescription.cxx
        model/GemErpPrMedication.cxx
        model/Iknr.cxx
        model/KbvBundle.cxx
        model/KbvComposition.cxx
        model/KbvCoverage.cxx
        model/KbvMedicationCompounding.cxx
        model/KbvMedicationFreeText.cxx
        model/KbvMedicationIngredient.cxx
        model/KbvMedicationPzn.cxx
        model/KbvMedicationRequest.cxx
        model/KbvOrganization.cxx
        model/KbvPractitioner.cxx
        model/KbvPracticeSupply.cxx
        model/Lanr.cxx
        model/Link.cxx
        model/MedicationDispense.cxx
        model/MedicationDispenseBundle.cxx
        model/MedicationDispenseId.cxx
        model/MedicationDispenseOperationParameters.cxx
        model/MedicationsAndDispenses.cxx
        model/MetaData.cxx
        model/OuterResponseErrorData.cxx
        model/Patient.cxx
        model/Pzn.cxx
        model/Reference.cxx
        model/ResourceInstances.cxx
        model/Signature.cxx
        model/Subscription.cxx
        model/Task.cxx
        model/WorkflowParameters.cxx
        pc/CFdSigErpManager.cxx
        pc/PcServiceContext.cxx
        pc/SeedTimer.cxx
        pc/pre_user_pseudonym/PreUserPseudonym.cxx
        pc/pre_user_pseudonym/PreUserPseudonymManager.cxx
        pc/telematic_pseudonym/TelematicPseudonymManager.cxx
        pc/telematik_report_pseudonym/PseudonameKeyRefreshJob.cxx
        registration/ApplicationHealthAndRegistrationUpdater.cxx
        registration/RegistrationManager.cxx
        server/HttpsServer.cxx
        server/ServerSession.cxx
        server/ServerSocketHandler.cxx
        server/context/SessionContext.cxx
        server/RequestHandler.cxx
        service/AuditEventHandler.cxx
        service/CommunicationDeleteHandler.cxx
        service/CommunicationGetHandler.cxx
        service/CommunicationPostHandler.cxx
        service/DeviceHandler.cxx
        service/ErpRequestHandler.cxx
        service/HealthHandler.cxx
        service/MedicationDispenseHandler.cxx
        service/MedicationDispenseHandlerBase.cxx
        service/MetaDataHandler.cxx
        service/RedisInterface.hxx
        service/SubscriptionPostHandler.cxx
        service/VauRequestHandler.cxx
        service/chargeitem/ChargeItemBodyHandlerBase.cxx
        service/chargeitem/ChargeItemDeleteHandler.cxx
        service/chargeitem/ChargeItemGetHandler.cxx
        service/chargeitem/ChargeItemHandlerBase.cxx
        service/chargeitem/ChargeItemPostHandler.cxx
        service/chargeitem/ChargeItemPutHandler.cxx
        service/chargeitem/ChargeItemPatchHandler.cxx
        service/consent/ConsentDeleteHandler.cxx
        service/consent/ConsentGetHandler.cxx
        service/consent/ConsentPostHandler.cxx
        service/task/AbortTaskHandler.cxx
        service/task/AcceptTaskHandler.cxx
        service/task/ActivateTaskHandler.cxx
        service/task/CloseTaskHandler.cxx
        service/task/CreateTaskHandler.cxx
        service/task/DispenseTaskHandler.cxx
        service/task/GetTaskHandler.cxx
        service/task/RejectTaskHandler.cxx
        service/task/TaskHandler.cxx
        tee/ErpTeeProtocol.cxx
        tee/InnerTeeRequest.cxx
        tee/InnerTeeResponse.cxx
        tee/OuterTeeResponse.cxx
        util/health/HealthCheck.cxx
        util/search/PagingArgument.cxx
        util/search/SearchArgument.cxx
        util/search/SearchParameter.cxx
        util/search/SortArgument.cxx
        util/search/UrlArguments.cxx
        )

add_library(
    erp-processing-context-files OBJECT
    ${erp-processing-context-source-files}
)

target_compile_definitions(erp-processing-context-files
    PUBLIC
        $<$<BOOL:${ERP_WITH_HSM_MOCK}>:WITH_HSM_MOCK=1>
        $<$<BOOL:${ERP_WITH_HSM_TPM_PRODUCTION}>:WITH_HSM_TPM_PRODUCTION=1>
)

target_include_directories (
    erp-processing-context-files
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..
        PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/.. # Include the erp/ and fhirtools/ prefixes in include statements.
)

target_link_libraries (
    erp-processing-context-files
        PUBLIC erp-shared
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

add_executable(erp-processing-context erp-main.cxx)
# Add -rdynamic to enrich the symbol table so that stack traces become more readable.
set_property(TARGET erp-processing-context PROPERTY ENABLE_EXPORTS ON)

target_include_directories (
    erp-processing-context
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/.. # Include the erp/ and fhirtools/ prefixes in include statements.
)

target_link_libraries (
    erp-processing-context
        PUBLIC
            erp-processing-context-files
)

add_dependencies(erp-processing-context resources)


install(TARGETS erp-processing-context COMPONENT production)
install_libs_for(erp-processing-context production)
if (NOT TARGET production)
    add_custom_target(production)
endif()
add_dependencies(production erp-processing-context)
