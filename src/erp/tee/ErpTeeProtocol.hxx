/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_TEE_ERPTEEPROTOCOL_HXX
#define ERP_PROCESSING_CONTEXT_TEE_ERPTEEPROTOCOL_HXX

#include "shared/util/SafeString.hxx"
#include "erp/tee/InnerTeeRequest.hxx"
#include "shared/crypto/OpenSslHelper.hxx"
#include "shared/server/response/ServerResponse.hxx"
#include "OuterTeeResponse.hxx"

#include <string>

class HsmPool;
class OuterTeeRequest;


/**
 * Implementation of the TEE (VAU) protocol for eRp. Server side.
 */
class ErpTeeProtocol
{
public:
    static constexpr const char* keyDerivationInfo = "ecies-vau-transport";

    /**
     * Decrypt an incoming request and return its inner request.
     * @param hsmPool  is used to perform the ECDH and HKDF.
     */
    static InnerTeeRequest decrypt (const std::string& outerRequestBody, HsmPool& hsmPool);

    /**
     * Encrypt a response.
     * The request is used to transport requestId and AES key with which the resonse is to be encrypted.
     */
    static OuterTeeResponse encrypt(const ServerResponse& response, const SafeString& aesKey, const std::string& requestId);

private:
    static SafeString decryptOuterRequestBody (const OuterTeeRequest& message, const SafeString& symmetricKey);
};


#endif
