#include "erp/tee/ErpTeeProtocol.hxx"

#include "InnerTeeResponse.hxx"
#include "erp/ErpRequirements.hxx"
#include "erp/crypto/AesGcm.hxx"
#include "erp/crypto/EllipticCurveUtils.hxx"
#include "erp/hsm/HsmPool.hxx"
#include "erp/tee/OuterTeeRequest.hxx"


InnerTeeRequest ErpTeeProtocol::decrypt (const std::string& outerRequestBody, HsmPool& hsmPool)
{
    A_20163.start("2 - try to decrypt the outer request body");
    const auto message = OuterTeeRequest::disassemble(outerRequestBody);
    Expect3(message.version==1, "invalid TEE version", AesGcmException);
    auto hsmSession = hsmPool.acquire();
    std::string clientPublicKeyPem;
    try
    {
        auto clientPublicKey = EllipticCurveUtils::createPublicKeyBin(message.getPublicX(), message.getPublicY());
        clientPublicKeyPem = EllipticCurveUtils::publicKeyToX962Der(clientPublicKey);
    }
    catch(const std::runtime_error& exc)
    {
        ErpFail(HttpStatus::BadRequest, "Unable to create public key from message data");
    }
    const SafeString symmetricKey{
        hsmSession.session().vauEcies128(
            ErpVector(clientPublicKeyPem.data(), clientPublicKeyPem.data() + clientPublicKeyPem.size()))};

    SafeString plaintext = decryptOuterRequestBody(message, symmetricKey);
    A_20163.finish();

    A_20163.start("3 - verify p structure");
    InnerTeeRequest innerTeeRequest (plaintext);
    Expect3(innerTeeRequest.version() == "1", "wrong version", AesGcmException);
    Expect3(innerTeeRequest.aesKey().size() == AesGcm128::KeyLength, "AES key has wrong size", AesGcmException);
    A_20163.finish();

    return innerTeeRequest;
}


OuterTeeResponse ErpTeeProtocol::encrypt (const ServerResponse& serverResponse, const SafeString& aesKey,
                                          const std::string& requestId)
{
    A_20163.start("8 - encode response");
    InnerTeeResponse innerResponse (requestId, serverResponse.getHeader(), serverResponse.getBody());
    A_20163.finish();

    A_20163.start("9 - encrypt response");
    OuterTeeResponse outerResponse (innerResponse.getA(), aesKey);
    A_20163.finish();

    return outerResponse;
}


SafeString ErpTeeProtocol::decryptOuterRequestBody (const OuterTeeRequest& message, const SafeString& symmetricKey)
{
    return AesGcm128::decrypt(message.ciphertext, symmetricKey, message.getIv(), message.getAuthenticationTag());
}
