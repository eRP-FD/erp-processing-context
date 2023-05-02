/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "mock/tpm/TpmMock.hxx"

#include "erp/common/HttpStatus.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/util/Expect.hxx"
#include "mock/tpm/TpmTestData.hxx"


std::unique_ptr<Tpm> TpmMock::createInstance(void)
{
    TVLOG(0) << "creating mock TPM for development use";
    return std::make_unique<TpmMock>();
}

Tpm::Factory TpmMock::createFactory(void)
{
    return [](BlobCache&)
    {
        return createInstance();
    };
}


Tpm::EndorsementKeyOutput TpmMock::getEndorsementKey (void)
{
    EndorsementKeyOutput output;
    output.certificateBase64 = tpm::ekcertecc_crt_base64;
    output.ekNameBase64 = tpm::EKPub_name_base64;
    return output;
}


Tpm::AttestationKeyOutput TpmMock::getAttestationKey (const bool isEnrolmentActive)
{
    (void)isEnrolmentActive;

    AttestationKeyOutput output;
    output.publicKeyBase64 = tpm::AKPub_bin_base64;
    output.akNameBase64 = tpm::h80000002_bin_base64;
    return output;
}


Tpm::AuthenticateCredentialOutput TpmMock::authenticateCredential (
    Tpm::AuthenticateCredentialInput&& input,
    const bool isEnrolmentActive)
{
    (void)isEnrolmentActive;

    // The values used in this method have been recorded during a successful run of TEST_F(TeeTokenUpdaterTest, doUpdateCalled)
    // by logging input and output values of MockEnrolmentManager::getChallengeAnswer().

    if (input.secretBase64 != "ACCMno94o+hePIgdbvWa6JvvX5vrrFfEpzeZF4X1/8ZTiAAg5ntsPvEsiTaqLOilvDDyJ63A3OV3D6AHP8WKNo3UXH4=")
        LOG(WARNING) << "'secret' differs from base64 encoded content of 'secretHSM.bin'";
    if (input.credentialBase64 != "ACDICBOju8CAMmpxTJUGOzBAwtbAKpqzEulOS8YRHB/SRj3s2OsgAQeF2vyvmLdqwF6Yb2h8lGl1B43SDTsl8yZ8gEY=")
        LOG(WARNING) << "'credential' differs from base64 encoded content of 'encCredHSM.bin'";
    if (input.akNameBase64 != "AAuYMqOy91+RXl/w1LNlkItLVih0UAJ5qFUVZxo5FO3/5A==")
        LOG(WARNING) << "'akName' differs from base64 encoded content of 'h80000002.bin'";

    Tpm::AuthenticateCredentialOutput output;
    output.plainTextCredentialBase64 = "Hb3mbn5C6VCKXgLMtD4gEp69dfd+klXaQSlgJdjIn00=";
    return output;
}


Tpm::QuoteOutput TpmMock::getQuote (
    QuoteInput&& input,
    const bool isEnrolmentActive)
{
    (void)isEnrolmentActive;

    // Check hard requirements.
    for (const auto value : input.pcrSet)
        ErpExpect(value<16, HttpStatus::BadRequest, "pcr register index outside [0,16)");
    ErpExpect(input.hashAlgorithm=="SHA256", HttpStatus::BadRequest, "only SHA256 is supported as hash algorithm");

    // Check soft requirements for stubbed enrolment service.
    if (input.nonceBase64 != tpm::QuoteNONCESaved_bin_base64)
        LOG(WARNING) << "'nonce' differs from base64 encoded content of 'QuoteNONCESaved.bin'";
    if (input.pcrSet.size() != 1)
        LOG(WARNING) << "'pcrSet' does not contain exactly a single element";
    else if (input.pcrSet[0] != 0)
        LOG(WARNING) << "'pcrSet' is not [0]";

    QuoteOutput output;
    output.quotedDataBase64 = tpm::attest_bin_base64;
    output.quoteSignatureBase64 = tpm::quotesig_bin_base64;
    return output;
}
