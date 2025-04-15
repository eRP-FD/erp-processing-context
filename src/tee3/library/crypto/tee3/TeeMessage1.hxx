/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_CRYPTO_TEE3_TEEMESSAGE1_HXX
#define EPA_LIBRARY_CRYPTO_TEE3_TEEMESSAGE1_HXX

#include "library/crypto/tee3/Tee3Protocol.hxx"
#include "library/util/BinaryBuffer.hxx"

#include <string>

namespace epa
{

struct TeeMessage1
{
    const std::string_view messageType = Tee3Protocol::message1Name;
    Tee3Protocol::EcdhPublicKey ecdhPublicKey;
    BinaryBuffer kyber768PublicKey;


    template<typename Processor>
    constexpr void processMembers(Processor& processor)
    {
        processor("/MessageType", messageType);
        processor("/ECDH_PK", ecdhPublicKey);
        processor("/Kyber768_PK", kyber768PublicKey);
    }
};

} // namespace epa

#endif
