/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "test/mock/ClientTeeProtocol.hxx"
#include "erp/tee/ErpTeeProtocol.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/beast/BoostBeastHeader.hxx"
#include "shared/crypto/AesGcm.hxx"
#include "shared/crypto/EllipticCurveUtils.hxx"
#include "shared/crypto/Jwt.hxx"
#include "shared/crypto/SecureRandomGenerator.hxx"
#include "shared/network/client/request/ClientRequestWriter.hxx"
#include "shared/network/client/request/ValidatedClientRequest.hxx"
#include "shared/util/ByteHelper.hxx"
#include "src/shared/tee/OuterTeeRequest.hxx"


namespace todo {
    constexpr const char* headerFieldDelimiter = "\r\n";

    std::unordered_map<std::string, std::string> deserializeFields (std::string s)
    {
        std::unordered_map<std::string, std::string> fields;
        static const std::string errorPrefix = "invalid HTTP header fields:";

        if (s.empty())
            return fields;

        if (s.size() < 2)
            // A single character or an empty line can not be a valid header.
            throw std::runtime_error(errorPrefix + " too short");

        // Make the parsing algorithm a bit simpler by providing a final crlf.
        if (s.substr(s.size()-2) != headerFieldDelimiter)
            s += headerFieldDelimiter;

        size_t fieldStart = 0;
        while (fieldStart != std::string::npos && fieldStart<s.size()-2)
        {
            const auto separatorPosition = s.find(':', fieldStart);
            if (separatorPosition == std::string::npos)
                throw std::runtime_error(errorPrefix + " missing key/value separator");

            const auto delimiterPosition = s.find(headerFieldDelimiter, fieldStart);
            if (delimiterPosition == std::string::npos)
            {
                // We should at least find the crlf that we added.
                throw std::runtime_error(errorPrefix + " missing field delimiter");
            }

            if (separatorPosition > delimiterPosition)
                throw std::runtime_error(errorPrefix + " invalid key");

            const std::string key = s.substr(fieldStart, separatorPosition-fieldStart);
            const std::string value = String::trim(s.substr(separatorPosition+1, delimiterPosition - separatorPosition - 1));

            if (key.find('\r') != std::string::npos)
                throw std::runtime_error(errorPrefix + "key contains CR");
            if (key.find('\n') != std::string::npos)
                throw std::runtime_error(errorPrefix + "key contains LF");
            if (value.find('\r') != std::string::npos)
                throw std::runtime_error(errorPrefix + "value contains CR");
            if (value.find('\n') != std::string::npos)
                throw std::runtime_error(errorPrefix + "value contains LF");

            fields.insert({key, value});

            fieldStart = delimiterPosition+2;
        }

        return fields;
    }

    void parseHeader (Header& header, const std::string& headerString)
    {
        // TODO: Parsing of a generic HTTP header should be done by a specialized library.
        // Maybe we can use boost beast for it.
        // For milestone 1, do it here.
        const auto statusLineEnd = headerString.find("\r\n");
        Expect(statusLineEnd != std::string::npos, "did not find end of status line");
        Expect(statusLineEnd+2 <= headerString.size(), "header string is too short");
        const auto endOfVersion = headerString.find(' ');
        Expect(endOfVersion!=std::string::npos, "did not find end of version");
        const auto startOfStatusCode = headerString.find_first_not_of(' ', endOfVersion);
        Expect(startOfStatusCode!=std::string::npos, "did not find start of status code");
        const auto endOfStatusCode = headerString.find(' ', startOfStatusCode);
        Expect(endOfStatusCode!=std::string::npos, "did not find end of status code");
        // Ignore the status code name for now.
        header.setStatus(static_cast<HttpStatus>(std::stoi(headerString.substr(startOfStatusCode, endOfStatusCode-startOfStatusCode))));
        header.addHeaderFields(deserializeFields(headerString.substr(statusLineEnd+2)));
    }
}


ClientTeeProtocol::ClientTeeProtocol (void)
    : mEphemeralKeyPair(EllipticCurve::BrainpoolP256R1->createKeyPair()),
      mIv(SecureRandomGenerator::generate(AesGcm128::IvLength))
{
}


std::string ClientTeeProtocol::createRequest (
    const Certificate& vauCertificate,
    const ClientRequest& request,
    const JWT& jwt)
{
    A_20161_01.start("1 - create HTTP request");
    // Create a string that contains the serialized version of request.
    const std::string requestContent = ClientRequestWriter(ValidatedClientRequest(request)).toString();
    A_20161_01.finish();

    return createRequest(vauCertificate, requestContent, jwt);
}


std::string ClientTeeProtocol::createRequest (
    const Certificate& vauCertificate,
    const std::string& requestContent,
    const JWT& jwt)
{
    A_20161_01.start("2,4 - create 128 bit random hex Request-Id");
    mRequestIdInRequest = String::toLower(ByteHelper::toHex(SecureRandomGenerator::generate(128 / 8)));
    A_20161_01.finish();

    A_20161_01.start("3,4 - create 128 bit AES key");
    mResponseSymmetricKey = SecureRandomGenerator::generate(AesGcm128::KeyLength);
    const auto aesKey = String::toLower(ByteHelper::toHex(mResponseSymmetricKey));
    A_20161_01.finish();

    A_20161_01.start("5 - create p by concatenating '1', JWT, requestId, aes key and serialized request");
    SafeString p("1 " + jwt.serialize() + " " + mRequestIdInRequest + " " + aesKey + " " + requestContent);
    A_20161_01.finish();

    return encryptInnerRequest(vauCertificate, p);
}


std::string ClientTeeProtocol::createRequest (
    const Certificate& vauCertificate,
    const ClientRequest& request,
    const std::string& jwt)
{
    A_20161_01.start("1 - create HTTP request");
    // Create a string that contains the serialized version of request.
    const std::string requestContent = ClientRequestWriter(ValidatedClientRequest(request)).toString();
    A_20161_01.finish();

    return createRequest(vauCertificate, requestContent, jwt);
}


std::string ClientTeeProtocol::createRequest (
    const Certificate& vauCertificate,
    const std::string& requestContent,
    const std::string& jwt)
{
    A_20161_01.start("2,4 - create 128 bit random hex Request-Id");
    mRequestIdInRequest = String::toLower(ByteHelper::toHex(SecureRandomGenerator::generate(128 / 8)));
    A_20161_01.finish();

    A_20161_01.start("3,4 - create 128 bit AES key");
    mResponseSymmetricKey = SecureRandomGenerator::generate(AesGcm128::KeyLength);
    const auto aesKey = String::toLower(ByteHelper::toHex(mResponseSymmetricKey));
    A_20161_01.finish();

    A_20161_01.start("5 - create p by concatenating '1', JWT, requestId, aes key and serialized request");
    SafeString p("1 " + jwt + " " + mRequestIdInRequest + " " + aesKey + " " + requestContent);
    A_20161_01.finish();

    return encryptInnerRequest(vauCertificate, p);
}


std::string ClientTeeProtocol::encryptInnerRequest (
    const Certificate& vauCertificate,
    const SafeString& p)
{
    // Prepare encryption
    A_20161_01.start("6a - create ephemeral ECDH key pair");
    // The key is created in the constructor.
    const auto [x,y] = EllipticCurveUtils::getPaddedXYComponents(mEphemeralKeyPair, EllipticCurve::KeyCoordinateLength);
    A_20161_01.finish();

    A_20161_01.start("6b - use HKDF");
    auto dh = EllipticCurve::BrainpoolP256R1->createDiffieHellman();
    dh.setPrivatePublicKey(mEphemeralKeyPair);
    dh.setPeerPublicKey(vauCertificate.getPublicKey());
    A_20161_01.finish();

    A_20161_01.start("6c,d - use 'ecies-vau-transport as info to derive AES 128 bit key for AES/GCM (CEK)");
    const auto contentEncryptionKey = SafeString(
        DiffieHellman::hkdf(dh.createSharedKey(), ErpTeeProtocol::keyDerivationInfo, AesGcm128::KeyLength));
    A_20161_01.finish();

    A_20161_01.start("6e - create 96 bit IV");
    // The IV has been created randomly in the constructor to give tests the opportunity to replace it
    // with a deterministically generated value.
    A_20161_01.finish();

    A_20161_01.start("6f - use CEK and IV to encrypt p with AES/GCM to obtain 128 bit authentication tag");
    auto encryptionResult = AesGcm128::encrypt(p, contentEncryptionKey, mIv);
    A_20161_01.finish();

    A_20161_01.start("6g - encode the result by concatenating version, key coordinates, iv, ciphertext, tag");
    OuterTeeRequest message;
    message.version = '\x01';
    std::copy(x.begin(), x.end(), message.xComponent);
    std::copy(y.begin(), y.end(), message.yComponent);
    const std::string_view ivView = mIv;
    std::copy(ivView.begin(), ivView.end(), message.iv);
    message.ciphertext = std::move(encryptionResult.ciphertext);
    std::copy(encryptionResult.authenticationTag.begin(), encryptionResult.authenticationTag.end(), message.authenticationTag);

    return message.assemble();
}


void ClientTeeProtocol::setEphemeralKeyPair (const shared_EVP_PKEY& keyPair)
{
    mEphemeralKeyPair = keyPair;
}


void ClientTeeProtocol::setIv (SafeString&& iv)
{
    mIv = std::move(iv);
}


/**
 * This method is still a bit quick and dirty. But then again, it is test-only code.
 */
ClientResponse ClientTeeProtocol::parseResponse(const ClientResponse& response) const
{
    Expect(response.getHeader().header(Header::ContentType) == "application/octet-stream",
           "response has wrong content type");
    Expect(response.getBody().size() >= AesGcm128::IvLength + AesGcm128::AuthenticationTagLength,
           "response is too short");

    const auto decryptedASf = AesGcm128::decrypt(
        response.getBody().substr(AesGcm128::IvLength,
                                  response.getBody().size() - AesGcm128::IvLength - AesGcm128::AuthenticationTagLength),
        mResponseSymmetricKey, response.getBody().substr(0, AesGcm128::IvLength),
        response.getBody().substr(response.getBody().size() - AesGcm128::AuthenticationTagLength,
                                  AesGcm128::AuthenticationTagLength));

    const std::string_view decryptedA = decryptedASf;

    Expect(decryptedA.size() > 2, "decrypted response is too short");
    Expect(decryptedA[0] == '1', "decrypted response does not start with '1'");
    Expect(decryptedA[1] == ' ', "version in decrypted response is longer than one character");
    const auto requestIdEnd = decryptedA.find(' ', 2);
    Expect(requestIdEnd != std::string::npos, "did not find requestId");
    Expect(requestIdEnd+1 <= decryptedA.size(), "did not find requestId");
    Expect(requestIdEnd > 2, "requestId empty");
    const auto requestId = decryptedA.substr(2, requestIdEnd-2);
    A_20174.start("4 - request ids in request and response must be identical");
    Expect(requestId==mRequestIdInRequest, "requestId in response differs from that in request");
    A_20174.finish();
    const auto headerEnd = decryptedA.find("\r\n\r\n", requestIdEnd+1);
    Expect(headerEnd != std::string::npos, "did not find end of header");

    Header decryptedResponseHeader;
    todo::parseHeader(decryptedResponseHeader,
                      std::string(decryptedA.substr(requestIdEnd + 1, headerEnd + 4 - requestIdEnd - 1)));
    if (headerEnd+4 < decryptedA.size())
    {
        return ClientResponse(decryptedResponseHeader, std::string(decryptedA.substr(headerEnd + 4)));
    }

    return ClientResponse(decryptedResponseHeader, "");
}
