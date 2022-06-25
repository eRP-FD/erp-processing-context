/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#include "TpmProxyApi.hxx"

Tpm::EndorsementKeyOutput TpmProxyApi::getEndorsementKey(void)
{
    const auto model = mClient.get("/Enrolment/EndorsementKey");

    Tpm::EndorsementKeyOutput output;
    output.certificateBase64 = model.getString("/certificate");
    output.ekNameBase64 = model.getString("/ekName");
    return output;
}


Tpm::AttestationKeyOutput TpmProxyApi::getAttestationKey(bool)
{
    const auto model = mClient.get("/Enrolment/AttestationKey");

    Tpm::AttestationKeyOutput output;
    output.publicKeyBase64 = model.getString("/publicKey");
    output.akNameBase64 = model.getString("/akName");
    return output;
}


Tpm::AuthenticateCredentialOutput TpmProxyApi::authenticateCredential(Tpm::AuthenticateCredentialInput&& input)
{
    return ::Tpm::AuthenticateCredentialOutput{
        mClient.authenticateCredential(input.secretBase64, input.credentialBase64, input.akNameBase64)};
}


Tpm::QuoteOutput TpmProxyApi::getQuote(Tpm::QuoteInput&& input, const std::string& message)
{
    // The nonce is modified by the enrolment API. Just check that we are called with the correct message.
    Expect(message == "ERP_ENROLLMENT", "getQuote called with the wrong nonce modified");

    EnrolmentModel model = mClient.getQuote(input.nonceBase64, ::PcrSet::fromList(input.pcrSet));

    Tpm::QuoteOutput output;
    output.quotedDataBase64 = model.getString("/quotedData");
    output.quoteSignatureBase64 = model.getString("/quoteSignature");
    return output;
}
