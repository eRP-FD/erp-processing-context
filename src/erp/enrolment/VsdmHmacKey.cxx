/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */


#include "erp/enrolment/VsdmHmacKey.hxx"
#include "erp/common/HttpStatus.hxx"
#include "erp/crypto/EllipticCurveUtils.hxx"
#include "erp/tee/OuterTeeRequest.hxx"
#include "erp/util/Base64.hxx"
#include "erp/util/ByteHelper.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/Hash.hxx"
#include "erp/util/TLog.hxx"
#include "erp/ErpRequirements.hxx"

#include <rapidjson/pointer.h>


VsdmHmacKey::VsdmHmacKey(std::string_view jsonText)
    : EnrolmentModel(jsonText)
{
    const auto operatorId = getString(jsonKey::operatorId);
    ErpExpect(operatorId.length() == 1, HttpStatus::BadRequest, "'betreiberkennung' field must be single character");
    mOperatorId = operatorId[0];
    const auto version = getString(jsonKey::version);
    ErpExpect(version.length() == 1, HttpStatus::BadRequest, "'version' field must be single character");
    mVersion = version[0];
}

VsdmHmacKey::VsdmHmacKey(char operatorId, char version)
    : mOperatorId{operatorId}
    , mVersion{version}
{
    set(jsonKey::operatorId, std::string{operatorId});
    set(jsonKey::version, std::string{version});
}


char VsdmHmacKey::operatorId() const
{
    return mOperatorId;
}

char VsdmHmacKey::version() const
{
    return mVersion;
}

void VsdmHmacKey::deleteKeyInformation()
{
    rapidjson::Pointer(jsonKey::encryptedKey.data()).Erase(mDocument);
    rapidjson::Pointer(jsonKey::clearTextKey.data()).Erase(mDocument);
}

SafeString VsdmHmacKey::decryptHmacKey(HsmSession& hsmSession, bool validate) const
{
    const auto encryptedKey = ByteHelper::fromHex(getString(jsonKey::encryptedKey));

    // follow the schema as defined in A_23463, which is identical to A_20161_01
    auto eciesData = OuterTeeRequest::disassemble(encryptedKey);
    ErpExpect(eciesData.version == 1, HttpStatus::BadRequest, "Unexpected version number");
    std::string clientPublicKeyPem;
    try
    {
        auto clientPublicKey = EllipticCurveUtils::createPublicKeyBin(eciesData.getPublicX(), eciesData.getPublicY());
        clientPublicKeyPem = EllipticCurveUtils::publicKeyToX962Der(clientPublicKey);
    }
    catch (const std::runtime_error&)
    {
        ErpFail(HttpStatus::BadRequest, "Unable to create public key from message data");
    }

    const auto clientPublicKey =
        ErpVector{clientPublicKeyPem.data(), clientPublicKeyPem.data() + clientPublicKeyPem.size()};
    SafeString aesKey;
    SafeString plaintext;
    try
    {
        aesKey = SafeString{hsmSession.vauEcies128(clientPublicKey, false)};
    }
    catch (const AesGcmException& exception)
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

    SafeString hmacKey;
    try
    {
        hmacKey = AesGcm128::decrypt(eciesData.ciphertext, aesKey, eciesData.getIv(), eciesData.getAuthenticationTag());
    }
    catch (const AesGcmException& e)
    {
        ErpFail(HttpStatus::BadRequest, "HMAC key decryption failed: " + std::string{e.what()});
    }
    ErpExpect(hmacKey.size() == VsdmHmacKey::keyLength, HttpStatus::BadRequest, "Invalid hmac key length");

    if (validate)
    {
        A_23493.start("Validate hmac key");
        const auto hmacForEmptyStr = util::stringToBuffer(ByteHelper::fromHex(getString(jsonKey::hmacEmptyString)));
        const auto hash = Hash::hmacSha256(hmacKey, "");
        ErpExpect(hash == hmacForEmptyStr, HttpStatus::BadRequest, "VSDM hash validation failed");
        A_23493.finish();
    }
    return hmacKey;
}

void VsdmHmacKey::setPlainTextKey(std::string_view key)
{
    set(jsonKey::clearTextKey, key);
}

std::string_view VsdmHmacKey::plainTextKey() const
{
    const auto& value = getValue(jsonKey::clearTextKey);
    ErpExpect(value.IsString(), HttpStatus::BadRequest, "value is not a string");
    return {value.GetString(), value.GetStringLength()};
}
