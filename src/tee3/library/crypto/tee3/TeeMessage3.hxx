/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_CRYPTO_TEE3_TEEMESSAGE3_HXX
#define EPA_LIBRARY_CRYPTO_TEE3_TEEMESSAGE3_HXX

#include "library/crypto/tee3/Tee3Protocol.hxx"
#include "library/util/BinaryBuffer.hxx"

#include <string>

namespace epa
{

struct TeeMessage3InnerLayer
{
    Tee3Protocol::EcdhPublicKey ecdhCipherText; // ECDH_ct
    BinaryBuffer kyber768CipherText;            // Kyber768_ct
    bool erp = false;                           // ERP = Enforce Replay Protection
    bool eso = false;                           // ESO = Enforce Sequence Order


    template<typename Processor>
    constexpr void processMembers(Processor& processor)
    {
        processor("/ECDH_ct", ecdhCipherText);
        processor("/Kyber768_ct", kyber768CipherText);
        processor("/ERP", erp);
        processor("/ESO", eso);
    }
};


struct TeeMessage3
{
    const std::string_view messageType = Tee3Protocol::message3Name;
    BinaryBuffer aeadCipherText;                // AEAD_ct
    BinaryBuffer aeadCipherTextKeyConfirmation; // AEAD_ct_key_confirmation


    template<typename Processor>
    constexpr void processMembers(Processor& processor)
    {
        processor("/MessageType", messageType);
        processor("/AEAD_ct", aeadCipherText);
        processor("/AEAD_ct_key_confirmation", aeadCipherTextKeyConfirmation);
    }
};

} // namespace epa

#endif
