/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "EnrolmentServer.hxx"
#include "EnrolmentRequestHandler.hxx"
#include "shared/server/handler/RequestHandlerManager.hxx"

namespace EnrolmentServer
{

void addEndpoints (RequestHandlerManager& handlers)
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
    handlers.onPostDo(
        "/Enrolment/VauSig",
        std::make_unique<enrolment::PostVauSig>());
    handlers.onDeleteDo(
        "/Enrolment/VauSig",
        std::make_unique<enrolment::DeleteVauSig>());
    handlers.onPostDo("/Enrolment/VsdmHmacKey", std::make_unique<enrolment::PostVsdmHmacKey>());
    handlers.onGetDo("/Enrolment/VsdmHmacKey", std::make_unique<enrolment::GetVsdmHmacKey>());
    handlers.onDeleteDo("/Enrolment/VsdmHmacKey", std::make_unique<enrolment::DeleteVsdmHmacKey>());
    handlers.onPostDo("/Enrolment/VauAut", std::make_unique<enrolment::PostVauAut>());
    handlers.onDeleteDo("/Enrolment/VauAut", std::make_unique<enrolment::DeleteVauAut>());
}

} // namespace EnrolmentServer
