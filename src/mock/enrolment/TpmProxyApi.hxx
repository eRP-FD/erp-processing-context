/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_TPMPROXYAPI_HXX
#define ERP_PROCESSING_CONTEXT_TPMPROXYAPI_HXX

#include "erp/enrolment/EnrolmentHelper.hxx"

#include "tools/EnrolmentApiClient.hxx"

/**
 * This class is not used in production. It is used in tests and for the blob-db-initialization tool.
 */
class TpmProxyApi : public TpmProxy
{
public:
    Tpm::EndorsementKeyOutput getEndorsementKey(void) override;
    Tpm::AttestationKeyOutput getAttestationKey(bool isEnrolmentActive = false) override;
    Tpm::AuthenticateCredentialOutput authenticateCredential(Tpm::AuthenticateCredentialInput&& input) override;
    Tpm::QuoteOutput getQuote(Tpm::QuoteInput&& input, const std::string& message) override;

private:
    ::EnrolmentApiClient mClient{};
};

#endif// ERP_PROCESSING_CONTEXT_TPMPROXYAPI_HXX
