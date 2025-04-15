/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_CRYPTO_TEE3_TEEMESSAGE4_HXX
#define EPA_LIBRARY_CRYPTO_TEE3_TEEMESSAGE4_HXX

#include "library/crypto/tee3/Tee3Protocol.hxx"
#include "library/util/BinaryBuffer.hxx"

#include <string>

namespace epa
{

struct TeeMessage4
{
    const std::string_view messageType = Tee3Protocol::message4Name;
    BinaryBuffer aeadCipherTextKeyConfirmation; // AEAD_ct_key_confirmation


    template<typename Processor>
    constexpr void processMembers(Processor& processor)
    {
        processor("/MessageType", messageType);
        processor("/AEAD_ct_key_confirmation", aeadCipherTextKeyConfirmation);
    }
};

} // namespace epa

#endif
