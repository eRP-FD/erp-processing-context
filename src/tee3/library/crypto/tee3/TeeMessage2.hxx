/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_CRYPTO_TEE3_TEEMESSAGE2_HXX
#define EPA_LIBRARY_CRYPTO_TEE3_TEEMESSAGE2_HXX

#include "library/crypto/tee3/Tee3Protocol.hxx"
#include "library/util/BinaryBuffer.hxx"

#include <string>

namespace epa
{

struct TeeMessage2
{
    const std::string_view messageType = Tee3Protocol::message2Name;
    Tee3Protocol::EcdhPublicKey ecdhCipherText;
    BinaryBuffer kyber768CipherText;
    BinaryBuffer aeadCipherText;


    template<typename Processor>
    constexpr void processMembers(Processor& processor)
    {
        processor("/MessageType", messageType);
        processor("/ECDH_ct", ecdhCipherText);
        processor("/Kyber768_ct", kyber768CipherText);
        processor("/AEAD_ct", aeadCipherText);
    }
};

} // namespace epa

#endif
