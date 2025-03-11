/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_CRYPTO_TEE3_TEE3SESSIONCONTEXT_HXX
#define EPA_LIBRARY_CRYPTO_TEE3_TEE3SESSIONCONTEXT_HXX

#include "library/crypto/SensitiveDataGuard.hxx"
#include "library/crypto/tee/KeyId.hxx"
#include "library/crypto/tee3/Tee3Protocol.hxx"
#include "library/util/BinaryBuffer.hxx"

#include <cstdint>

namespace epa
{

struct Tee3Context;

/**
 * This class represents a Tee3Context whose variable fields have been fixed for a specific session,
 * i.e. a request or a response, client-to-server or server-to-client.
 */
class Tee3SessionContext
{
public:
    // The numerical values match the ones used in gemSpec_Krypt to allow simple conversion with
    // static_cast.
    enum Direction : uint8_t
    {
        ClientToServer = 1,
        ServerToClient = 2
    };


    const SensitiveDataGuard k2Key;
    const KeyId keyId;
    const Tee3Protocol::VauCid vauCid;
    const uint8_t version;
    const bool isPU;
    const uint64_t messageCounter;
    const uint64_t requestCounter;
    const Direction direction;
};

} // namespace epa

#endif
