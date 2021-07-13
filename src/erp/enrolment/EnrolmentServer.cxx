#include "erp/enrolment/EnrolmentServer.hxx"
#include "erp/enrolment/EnrolmentServiceContext.hxx"
#include "erp/enrolment/EnrolmentRequestHandler.hxx"
#include "erp/server/handler/RequestHandlerManager.hxx"


EnrolmentServer::EnrolmentServer(
    const uint16_t enrolmentPort,
    RequestHandlerManager<EnrolmentServiceContext>&& handlerManager,
    std::unique_ptr<EnrolmentServiceContext> serviceContext)
    : mServer(
        HttpsServer<EnrolmentServiceContext>::defaultHost,
        enrolmentPort,
        std::move(handlerManager),
        std::move(serviceContext))
{
}


void EnrolmentServer::addEndpoints (RequestHandlerManager<EnrolmentServiceContext>& handlers)
{
    handlers.onGetDo(
        "/Enrolment/EnclaveStatus",
        std::make_unique<enrolment::GetEnclaveStatus>());
    handlers.onGetDo(
        "/Enrolment/EndorsementKey",
        std::make_unique<enrolment::GetEndorsementKey>());
    handlers.onGetDo(
        "/Enrolment/AttestationKey",
        std::make_unique<enrolment::GetAttestationKey>());
    handlers.onPostDo(
        "/Enrolment/AuthenticateCredential",
        std::make_unique<enrolment::PostAuthenticationCredential>());
    handlers.onPostDo(
        "/Enrolment/GetQuote",
        std::make_unique<enrolment::PostGetQuote>());
    handlers.onPutDo(
        "/Enrolment/KnownEndorsementKey",
        std::make_unique<enrolment::PutKnownEndorsementKey>());
    handlers.onDeleteDo(
        "/Enrolment/KnownEndorsementKey",
        std::make_unique<enrolment::DeleteKnownEndorsementKey>());
    handlers.onPutDo(
        "/Enrolment/KnownAttestationKey",
        std::make_unique<enrolment::PutKnownAttestationKey>());
    handlers.onDeleteDo(
        "/Enrolment/KnownAttestationKey",
        std::make_unique<enrolment::DeleteKnownAttestationKey>());
    handlers.onPutDo(
        "/Enrolment/KnownQuote",
        std::make_unique<enrolment::PutKnownQuote>());
    handlers.onDeleteDo(
        "/Enrolment/KnownQuote",
        std::make_unique<enrolment::DeleteKnownQuote>());
    handlers.onPutDo(
        "/Enrolment/EciesKeypair",
        std::make_unique<enrolment::PutEciesKeypair>());
    handlers.onDeleteDo(
        "/Enrolment/EciesKeypair",
        std::make_unique<enrolment::DeleteEciesKeypair>());
    handlers.onPutDo(
        "/Enrolment/{Type}/DerivationKey",
        std::make_unique<enrolment::PutDerivationKey>());
    handlers.onDeleteDo(
        "/Enrolment/{Type}/DerivationKey",
        std::make_unique<enrolment::DeleteDerivationKey>());
    handlers.onPutDo(
        "/Enrolment/KvnrHashKey",
        std::make_unique<enrolment::PutKvnrHashKey>());
    handlers.onPutDo(
        "/Enrolment/TelematikIdHashKey",
        std::make_unique<enrolment::PutTelematikIdHashKey>());
    handlers.onDeleteDo(
        "/Enrolment/KvnrHashKey",
        std::make_unique<enrolment::DeleteKvnrHashKey>());
    handlers.onDeleteDo(
        "/Enrolment/TelematikIdHashKey",
        std::make_unique<enrolment::DeleteTelematikIdHashKey>());
    handlers.onPutDo(
        "/Enrolment/VauSigPrivateKey",
        std::make_unique<enrolment::PutVauSigPrivateKey>());
    handlers.onDeleteDo(
        "/Enrolment/VauSigPrivateKey",
        std::make_unique<enrolment::DeleteVauSigPrivateKey>());
}


HttpsServer<EnrolmentServiceContext>& EnrolmentServer::getServer (void)
{
    return mServer;
}
