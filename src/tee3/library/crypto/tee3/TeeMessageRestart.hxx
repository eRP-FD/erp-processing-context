/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_CRYPTO_TEE3_TEEMESSAGERESTART_HXX
#define EPA_LIBRARY_CRYPTO_TEE3_TEEMESSAGERESTART_HXX

#include "library/crypto/tee3/Tee3Protocol.hxx"
#include "library/util/BinaryBuffer.hxx"

#include <string_view>

namespace epa
{

struct TeeMessageRestart
{
    const std::string_view messageType = Tee3Protocol::messageRestartName;
    BinaryBuffer keyId; // KeyID


    template<typename Processor>
    constexpr void processMembers(Processor& processor)
    {
        processor("/MessageType", messageType);
        processor("/KeyID", keyId);
    }
};

} // namespace epa

#endif
