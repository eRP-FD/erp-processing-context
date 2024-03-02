#
# (C) Copyright IBM Deutschland GmbH 2021, 2024
# (C) Copyright IBM Corp. 2021, 2024
#
# non-exclusively licensed to gematik GmbH
#

cmake_minimum_required(VERSION 3.16)

project(erp-processing-context)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/erp-serverinfo.cxx.in" "${CMAKE_CURRENT_BINARY_DIR}/erp-serverinfo.cxx")

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
        ErpConstants.hxx
        ErpMain.cxx
        ErpProcessingContext.cxx
        admin/AdminRequestHandler.cxx
        admin/AdminServer.cxx
        admin/PutRuntimeConfigHandler.cxx
        beast/BoostBeastHeader.cxx
        beast/BoostBeastStringReader.cxx
        beast/BoostBeastStringWriter.cxx
        client/ClientBase.cxx
        client/HttpClient.cxx
        client/HttpsClient.cxx
        client/RootCertificatesMgr.cxx
        client/TcpStream.cxx
        client/UrlRequestSender.cxx
        client/implementation/ClientImpl.cxx
        client/implementation/TlsSession.cxx
        client/request/ClientRequest.cxx
        client/request/ClientRequestWriter.cxx
        client/request/ValidatedClientRequest.cxx
        client/response/ClientResponse.cxx
        client/response/ClientResponseReader.cxx
        common/BoostBeastMethod.cxx
        common/Header.cxx
        common/HttpMethod.cxx
        common/HttpStatus.cxx
        common/MimeType.cxx
        compression/Compression.cxx
        compression/Deflate.cxx
        compression/ZStd.cxx
        compression/ZStdDictionary.cxx
        compression/ZStdDictionaryRepository.cxx
        crypto/AesGcm.cxx
        crypto/CMAC.cxx
        crypto/CadesBesSignature.cxx
        crypto/Certificate.cxx
        crypto/DiffieHellman.cxx
        crypto/EllipticCurve.cxx
        crypto/EllipticCurvePublicKey.cxx
        crypto/EllipticCurveUtils.cxx
        crypto/EssCertificateHelper.cxx
        crypto/Jws.cxx
        crypto/Jwt.cxx
        crypto/OpenSslHelper.cxx
        crypto/Pbkdf2Hmac.cxx
        crypto/RandomSource.cxx
        crypto/SecureRandomGenerator.cxx
        crypto/Seeder.cxx
        crypto/Sha256.cxx
        database/Database.cxx
        database/DatabaseCodec.cxx
        database/DatabaseConnectionInfo.cxx
        database/DatabaseConnectionTimer.cxx
        database/DatabaseFrontend.cxx
        database/DatabaseModel.cxx
        database/PostgresBackend.cxx
        database/PostgresBackendHelper.cxx
        database/PostgresBackendChargeItem.cxx
        database/PostgresBackendTask.cxx
        database/PostgresConnection.cxx
        database/RedisClient.cxx
        database/redis/RateLimiter.cxx
        hsm/BlobCache.cxx
        hsm/BlobDatabase.cxx
        hsm/VsdmKeyCache.cxx
        enrolment/EnrolmentData.cxx
        enrolment/EnrolmentModel.cxx
        enrolment/EnrolmentRequestHandler.cxx
        enrolment/EnrolmentServer.cxx
        enrolment/VsdmHmacKey.cxx
        fhir/Fhir.cxx
        fhir/FhirCanonicalizer.cxx
        fhir/FhirConverter.cxx
        fhir/internal/FhirJsonToXmlConverter.cxx
        fhir/internal/FhirSAXHandler.cxx
        hsm/ErpTypes.cxx
        hsm/HsmFactory.cxx
        hsm/HsmIdentity.cxx
        hsm/HsmPool.cxx
        hsm/HsmPoolSession.cxx
        hsm/HsmPoolSessionRemover.cxx
        hsm/HsmException.cxx
        hsm/KeyDerivation.cxx
        hsm/HsmSession.cxx
        hsm/HsmSessionExpiredException.cxx
        hsm/production/ProductionBlobDatabase.cxx
        hsm/production/ProductionVsdmKeyBlobDatabase.cxx
        hsm/TeeTokenUpdater.cxx
        idp/Idp.cxx
        idp/IdpUpdater.cxx
        model/AbgabedatenPkvBundle.cxx
        model/AuditData.cxx
        model/AuditEvent.cxx
        model/Binary.cxx
        model/Bundle.cxx
        model/ChargeItem.cxx
        model/Communication.cxx
        model/CommunicationPayload.cxx
        model/Composition.cxx
        model/Consent.cxx
        model/Device.cxx
        model/ErxReceipt.cxx
        model/Extension.cxx
        model/extensions/ChargeItemMarkingFlags.cxx
        model/extensions/KBVEXERPAccident.cxx
        model/extensions/KBVEXFORLegalBasis.cxx
        model/extensions/KBVMedicationCategory.cxx
        model/extensions/KBVMultiplePrescription.cxx
        model/Health.cxx
        model/Iknr.cxx
        model/KbvBundle.cxx
        model/KbvComposition.cxx
        model/KbvCoverage.cxx
        model/KbvMedicationBase.cxx
        model/KbvMedicationCompounding.cxx
        model/KbvMedicationDummy.cxx
        model/KbvMedicationFreeText.cxx
        model/KbvMedicationIngredient.cxx
        model/KbvMedicationPzn.cxx
        model/KbvMedicationRequest.cxx
        model/KbvOrganization.cxx
        model/KbvPractitioner.cxx
        model/KbvPracticeSupply.cxx
        model/Kvnr.cxx
        model/Lanr.cxx
        model/Link.cxx
        model/MedicationDispense.cxx
        model/MedicationDispenseBundle.cxx
        model/MedicationDispenseId.cxx
        model/MetaData.cxx
        model/ModelException.cxx
        model/NumberAsStringParserDocument.cxx
        model/NumberAsStringParserWriter.cxx
        model/OperationOutcome.cxx
        model/OuterResponseErrorData.cxx
        model/Parameters.cxx
        model/Patient.cxx
        model/PrescriptionId.cxx
        model/PrescriptionType.cxx
        model/Pzn.cxx
        model/Reference.cxx
        model/Resource.cxx
        model/ResourceFactory.cxx
        model/ResourceVersion.cxx
        model/Signature.cxx
        model/Subscription.cxx
        model/Task.cxx
        model/TelematikId.cxx
        model/TimePeriod.cxx
        model/Timestamp.cxx
        pc/CFdSigErpManager.cxx
        pc/PcServiceContext.cxx
        pc/SeedTimer.cxx
        pc/pre_user_pseudonym/PreUserPseudonym.cxx
        pc/pre_user_pseudonym/PreUserPseudonymManager.cxx
        pc/telematic_pseudonym/TelematicPseudonymManager.cxx
        pc/telematik_report_pseudonym/PseudonameKeyRefreshJob.cxx
        registration/ApplicationHealthAndRegistrationUpdater.cxx
        registration/RegistrationManager.cxx
        server/AccessLog.cxx
        server/ErrorHandler.cxx
        server/HttpsServer.cxx
        server/ServerSocketHandler.cxx
        server/ServerSession.cxx
        server/SslStream.cxx
        server/ThreadPool.cxx
        server/TlsSettings.cxx
        server/Worker.cxx
        server/context/SessionContext.cxx
        server/handler/RequestHandlerContext.cxx
        server/handler/RequestHandlerInterface.cxx
        server/handler/RequestHandlerManager.cxx
        server/request/ServerRequest.cxx
        server/request/ServerRequestReader.cxx
        server/response/ResponseBuilder.cxx
        server/response/ServerResponse.cxx
        server/response/ServerResponseWriter.cxx
        server/response/ValidatedServerResponse.cxx
        service/AuditDataCollector.cxx
        service/AuditEventCreator.cxx
        service/AuditEventTextTemplates.cxx
        service/AuditEventHandler.cxx
        service/CommunicationDeleteHandler.cxx
        service/CommunicationGetHandler.cxx
        service/CommunicationPostHandler.cxx
        service/DeviceHandler.cxx
        service/ErpRequestHandler.cxx
        service/HealthHandler.cxx
        service/MedicationDispenseHandler.cxx
        service/MetaDataHandler.cxx
        service/Operation.cxx
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
        service/task/GetTaskHandler.cxx
        service/task/RejectTaskHandler.cxx
        service/task/TaskHandler.cxx
        tee/ErpTeeProtocol.cxx
        tee/InnerTeeRequest.cxx
        tee/InnerTeeResponse.cxx
        tee/OuterTeeRequest.cxx
        tee/OuterTeeResponse.cxx
        tpm/PcrSet.cxx
        tpm/Tpm.cxx
        tsl/C14NHelper.cxx
        tsl/CertificateId.cxx
        tsl/OcspHelper.cxx
        tsl/OcspService.cxx
        tsl/OcspUrl.hxx
        tsl/TrustStore.cxx
        tsl/error/TslErrorCode.hxx
        tsl/error/TslError.cxx
        tsl/TslManager.cxx
        tsl/TslMode.hxx
        tsl/TslParser.cxx
        tsl/TslRefreshJob.cxx
        tsl/TslService.cxx
        tsl/X509Certificate.cxx
        tsl/X509Store.cxx
        util/Base64.cxx
        util/BoostUrl.cxx
        util/Buffer.cxx
        util/ByteHelper.cxx
        util/Condition.cxx
        util/Configuration.cxx
        util/ConfigurationFormatter.cxx
        util/CrashHandler.cxx
        util/Demangle.cxx
        util/DurationConsumer.cxx
        util/Environment.cxx
        util/ErpException.cxx
        util/ErpServiceException.cxx
        util/ExceptionHelper.cxx
        util/ExceptionWrapper.cxx
        util/Expect.cxx
        util/FileHelper.cxx
        util/GLogConfiguration.cxx
        util/Hash.cxx
        util/Holidays.cxx
        util/InvalidConfigurationException.cxx
        util/JsonLog.cxx
        util/JsonXmlParserHelper.cxx
        util/PeriodicTimer.cxx
        util/Random.cxx
        util/RapidjsonDocument.cxx
        util/RuntimeConfiguration.cxx
        util/SafeString.cxx
        util/ServerException.cxx
        util/SignalHandler.cxx
        util/String.cxx
        util/TerminationHandler.cxx
        util/ThreadNames.cxx
        util/Timer.cxx
        util/TimerJobBase.cxx
        util/TLog.cxx
        util/UrlHelper.cxx
        util/Uuid.cxx
        util/health/ApplicationHealth.cxx
        util/health/HealthCheck.cxx
        util/search/PagingArgument.cxx
        util/search/SearchArgument.cxx
        util/search/SearchParameter.cxx
        util/search/SortArgument.cxx
        util/Version.cxx
        util/search/UrlArguments.cxx
        util/WorkDay.cxx
        validation/InCodeValidator.cxx
        validation/JsonValidator.cxx
        validation/KbvBundleValidator.cxx
        validation/KbvCompositionValidator.cxx
        validation/KbvCoverageValidator.cxx
        validation/KbvMedicationCompoundingValidator.cxx
        validation/KbvMedicationFreeTextValidator.cxx
        validation/KbvMedicationIngredientValidator.cxx
        validation/KbvMedicationPZNValidator.cxx
        validation/KbvMedicationRequestValidator.cxx
        validation/KbvOrganizationValidator.cxx
        validation/KbvPatientValidator.cxx
        validation/KbvPractitionerValidator.cxx
        validation/KbvPracticeSupplyValidator.cxx
        validation/KbvValidationUtils.cxx
        validation/RapidjsonErrorDocument.cxx
        validation/XmlValidator.cxx
        xml/XmlDocument.cxx
        xml/XmlMemory.cxx
        "${CMAKE_CURRENT_BINARY_DIR}/erp-serverinfo.cxx"
        )

if(ERP_WITH_HSM_TPM_PRODUCTION)
    list(
        APPEND erp-processing-context-source-files
            enrolment/EnrolmentHelper.cxx
            hsm/production/HsmProductionClient.cxx
            hsm/production/HsmProductionFactory.cxx
            hsm/production/HsmRawSession.cxx
            hsm/production/TeeTokenProductionUpdater.cxx
            tpm/TpmProduction.cxx
     )
endif()

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
        PUBLIC fhirtools
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
