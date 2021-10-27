#
# (C) Copyright IBM Deutschland GmbH 2021
# (C) Copyright IBM Corp. 2021
#

cmake_minimum_required(VERSION 3.16)

project(erp-processing-context)

#
# Handle any debug configuration.
#
if(NOT CMAKE_BUILD_TYPE)
    # Perform a Debug build by default,
    set(CMAKE_BUILD_TYPE Debug)
endif()
if(CMAKE_BUILD_TYPE)
    if((CMAKE_BUILD_TYPE MATCHES Debug) OR (CMAKE_BUILD_TYPE MATCHES RelWithDebInfo))
        add_compile_options(-DENABLE_DEBUG_LOG)
    endif()
endif()

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/erp-serverinfo.hxx.in" "${CMAKE_CURRENT_BINARY_DIR}/erp-serverinfo.hxx")

set(ERP_ENROLMENT_DIR "${CMAKE_SOURCE_DIR}/resources/enrolment")


#
# Define how to build the eRP executable.
#
list(
    APPEND erp-processing-context-source-files
        ErpConstants.hxx
        ErpMain.cxx
        ErpProcessingContext.cxx
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
        common/BoostBeastHttpStatus.cxx
        common/BoostBeastMethod.cxx
        common/Header.cxx
        common/HttpMethod.cxx
        common/HttpStatus.cxx
        common/MimeType.cxx
        compression/Compression.cxx
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
        database/Data.cxx
        database/Database.cxx
        database/DatabaseCodec.cxx
        database/DatabaseFrontend.cxx
        database/DatabaseModel.cxx
        database/PostgresBackend.cxx
        database/PostgresConnection.cxx
        database/RedisClient.cxx
        hsm/BlobCache.cxx
        hsm/BlobDatabase.cxx
        hsm/production/HsmProductionClient.cxx
        hsm/production/ProductionBlobDatabase.cxx
        enrolment/EnrolmentData.cxx
        enrolment/EnrolmentHelper.cxx
        enrolment/EnrolmentModel.cxx
        enrolment/EnrolmentRequestHandler.cxx
        enrolment/EnrolmentServer.cxx
        enrolment/EnrolmentServiceContext.cxx
        enrolment/EnrolmentTemplateInstantiations.cxx
        fhir/Fhir.cxx
        fhir/FhirCanonicalizer.cxx
        fhir/FhirConverter.cxx
        fhir/FhirElement.cxx
        fhir/FhirStructureDefinition.cxx
        fhir/FhirStructureRepository.cxx
        fhir/internal/FhirJsonToXmlConverter.cxx
        fhir/internal/FhirSAXHandler.cxx
        fhir/internal/FhirStructureDefinitionParser.cxx
        hsm/ErpTypes.cxx
        hsm/HsmClient.cxx
        hsm/HsmFactory.cxx
        hsm/HsmIdentity.cxx
        hsm/HsmPool.cxx
        hsm/HsmPoolSession.cxx
        hsm/HsmPoolSessionRemover.cxx
        hsm/HsmException.cxx
        hsm/KeyDerivation.cxx
        hsm/production/HsmProductionFactory.cxx
        hsm/production/HsmRawSession.cxx
        hsm/HsmSessionExpiredException.cxx
        hsm/production/TeeTokenProductionUpdater.cxx
        hsm/HsmSession.cxx
        hsm/TeeTokenUpdater.cxx
        idp/Idp.cxx
        idp/IdpUpdater.cxx
        model/AuditData.cxx
        model/AuditEvent.cxx
        model/Binary.cxx
        model/Bundle.cxx
        model/Communication.cxx
        model/CommunicationPayload.cxx
        model/CommunicationPayloadItem.cxx
        model/Composition.cxx
        model/Device.cxx
        model/ErxReceipt.cxx
        model/Health.cxx
        model/Link.cxx
        model/MedicationDispense.cxx
        model/MetaData.cxx
        model/ModelException.cxx
        model/NumberAsStringParserDocument.cxx
        model/NumberAsStringParserWriter.cxx
        model/OperationOutcome.cxx
        model/OuterResponseErrorData.cxx
        model/Parameters.cxx
        model/Patient.cxx
        model/PrescriptionId.cxx
        model/Resource.cxx
        model/Signature.cxx
        model/Task.cxx
        model/TimePeriod.cxx
        model/Timestamp.cxx
        pc/PcServiceContext.cxx
        pc/SeedTimer.cxx
        pc/PcTemplateInstantiations.cxx
        pc/pre_user_pseudonym/PreUserPseudonym.cxx
        pc/pre_user_pseudonym/PreUserPseudonymManager.cxx
        registration/HeartbeatSender.cxx
        registration/RegistrationManager.cxx
        server/AccessLog.cxx
        server/ErrorHandler.cxx
        server/HttpsServer.cxx
        server/ServerSocketHandler.cxx
        server/SslStream.cxx
        server/ThreadPool.cxx
        server/TlsSettings.cxx
        server/Worker.cxx
        server/context/ServiceContext.cxx
        server/context/SessionContext.cxx
        server/handler/RequestHandlerContext.cxx
        server/handler/RequestHandlerInterface.cxx
        server/handler/RequestHandlerManager.cxx
        server/request/ServerRequest.cxx
        server/request/ServerRequestReader.cxx
        server/response/ResponseBuilder.cxx
        server/response/ResponseValidator.cxx
        server/response/ServerResponse.cxx
        server/response/ServerResponseWriter.cxx
        server/response/ValidatedServerResponse.cxx
        server/session/ServerSession.cxx
        server/session/AsynchronousServerSession.cxx
        server/session/SynchronousServerSession.cxx
        service/AuditDataCollector.cxx
        service/AuditEventCreator.cxx
        service/AuditEventTextTemplates.cxx
        service/AuditEventHandler.cxx
        service/CommunicationDeleteHandler.cxx
        service/CommunicationGetHandler.cxx
        service/CommunicationPostHandler.cxx
        service/DeviceHandler.cxx
        service/DosHandler.cxx
        service/ErpRequestHandler.cxx
        service/HealthHandler.cxx
        service/MedicationDispenseHandler.cxx
        service/MetaDataHandler.cxx
        service/Operation.cxx
        service/RedisInterface.hxx
        service/VauRequestHandler.cxx
        service/task/AbortTaskHandler.cxx
        service/task/AcceptTaskHandler.cxx
        service/task/ActivateTaskHandler.cxx
        service/task/CloseTaskHandler.cxx
        service/task/CreateTaskHandler.cxx
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
        util/Buffer.cxx
        util/ByteHelper.cxx
        util/Condition.cxx
        util/Configuration.cxx
        util/DurationConsumer.cxx
        util/Environment.cxx
        util/ErpException.cxx
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
        util/search/UrlArguments.cxx
        util/WorkDay.cxx
        validation/JsonValidator.cxx
        validation/XmlValidator.cxx
        xml/SaxHandler.cxx
        xml/XmlDocument.cxx
        xml/XmlMemory.cxx
        )

if(NOT APPLE AND NOT MSVC)
    list(
        APPEND erp-processing-context-source-files
            tpm/TpmProduction.cxx
     )
endif()

add_library(
    erp-processing-context-files OBJECT
    ${erp-processing-context-source-files}
        )

target_include_directories (
    erp-processing-context-files
        PUBLIC ${PROJECT_SOURCE_DIR}/..
        PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/.. # Include the erp/ and library/ prefixes in include statements.
)

target_link_libraries (
    erp-processing-context-files
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
)
if(NOT APPLE AND NOT MSVC)
    target_link_libraries (
        erp-processing-context-files
            PUBLIC
            CONAN_PKG::hsmclient
            CONAN_PKG::tpmclient
    )
endif()

add_executable(erp-processing-context erp-main.cxx)

target_include_directories (
    erp-processing-context
        PRIVATE ${PROJECT_SOURCE_DIR}/.. # Include the erp/ and library/ prefixes in include statements.
)

target_link_libraries (
    erp-processing-context
        PUBLIC erp-processing-context-files
)

add_dependencies(erp-processing-context resources)


install(TARGETS erp-processing-context COMPONENT production)
install_libs_for(erp-processing-context production)
if (NOT TARGET production)
    add_custom_target(production)
endif()
add_dependencies(production erp-processing-context)
