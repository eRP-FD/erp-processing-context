/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_TPMMOCK_HXX
#define ERP_PROCESSING_CONTEXT_TPMMOCK_HXX

#include "shared/tpm/Tpm.hxx"


#if WITH_HSM_MOCK  != 1
#error TpmMock.hxx included but WITH_HSM_MOCK not enabled
#endif
/**
 * A mock implementation of the Tpm interface that does not rely on the tpmclient and therefore can be built on
 * other platforms than Linux.
 */
class TpmMock : public Tpm
{
public:
    static std::unique_ptr<Tpm> createInstance();
    static Tpm::Factory createFactory(void);

    EndorsementKeyOutput getEndorsementKey (void) override;

    AttestationKeyOutput getAttestationKey (
        bool isEnrolmentActive = false) override;

    AuthenticateCredentialOutput authenticateCredential (
        AuthenticateCredentialInput&& input,
        bool isEnrolmentActive = false) override;

    QuoteOutput getQuote (
        QuoteInput&& input,
        bool isEnrolmentActive = false) override;
};

#endif
