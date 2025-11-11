/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/enrolment/PseudonameLogKeyPackage.hxx"
#include "shared/crypto/EllipticCurveUtils.hxx"
#include "shared/network/message/HttpStatus.hxx"
#include "shared/tee/OuterTeeRequest.hxx"
#include "shared/util/ByteHelper.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/TLog.hxx"


PseudonameLogKeyPackage::PseudonameLogKeyPackage(std::string_view jsonText)
    : EnrolmentModel(jsonText)
{
    const auto version = getString(JsonKey::version);
    mVersion = static_cast<unsigned int>(std::strtoul(version.c_str(), nullptr, 10));
}

unsigned int PseudonameLogKeyPackage::version() const
{
    return mVersion;
}

// GEMREQ-start A_27392
SafeString PseudonameLogKeyPackage::decryptPseudonameLogKey(HsmSession& hsmSession) const
{
    const auto encryptedKey = ByteHelper::fromHex(getString(JsonKey::encryptedKey));

    // follow the schema as defined in A_23463, which is identical to A_20161_01
    auto eciesData = OuterTeeRequest::disassemble(encryptedKey);
    ErpExpect(eciesData.version == 1, HttpStatus::BadRequest, "Unexpected version number");
    std::string clientPublicKeyPem;
    try
    {
        auto clientPublicKey = EllipticCurveUtils::createPublicKeyBin(eciesData.getPublicX(), eciesData.getPublicY());
        clientPublicKeyPem = EllipticCurveUtils::publicKeyToX962Der(clientPublicKey);
    }
    catch (const std::runtime_error& e)
    {
        ErpFail(HttpStatus::BadRequest,
                std::string{"Unable to create public key from message data: "}.append(e.what()));
    }

    const auto clientPublicKey = ErpVector::create(clientPublicKeyPem);
    SafeString aesKey;
    try
    {
        aesKey = SafeString{hsmSession.vauEcies128(clientPublicKey, false)};
    }
    catch (const AesGcmException&)
    {
        try
        {
            aesKey = SafeString{hsmSession.vauEcies128(clientPublicKey, true)};
        }
        catch (const ErpException&)
        {
            // Fallback is not available. Continue with error from standard try.
            ErpFail(HttpStatus::BadRequest, "Unable to perform Ecies decryption");
        }
    }

    SafeString key;
    try
    {
        key = AesGcm128::decrypt(eciesData.ciphertext, aesKey, eciesData.getIv(), eciesData.getAuthenticationTag());
    }
    catch (const AesGcmException& e)
    {
        ErpFail(HttpStatus::BadRequest, "HMAC key decryption failed: " + std::string{e.what()});
    }
    ErpExpect(key.size() == PseudonameLogKeyPackage::keyLength, HttpStatus::BadRequest,
              "Invalid pseudoname key length");

    return key;
}
// GEMREQ-end A_27392
