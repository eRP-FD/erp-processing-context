/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_CRYPTO_TEE3_TEE3CONTEXT_HXX
#define EPA_LIBRARY_CRYPTO_TEE3_TEE3CONTEXT_HXX

#include "library/crypto/SensitiveDataGuard.hxx"
#include "library/crypto/tee3/Tee3Protocol.hxx"
#include "library/crypto/tee3/Tee3SessionContext.hxx"

#include <atomic>

namespace epa
{

/**
 * A collection of data that is associated with a TEE channel (VAU-Kanal):
 * - K2_c2s_app_data for the encryption of messages from client to server
 * - K2_s2c_app_data for the encryption of messages from server to client
 * - KeyID to identify the TEE channel
 * - VAU-CID is the endpoint that is returned with message2 and has to be used
 *   for all requestes of the TEE channel
 *
 * Should the VAU-NP pseudonym be integrated as well?
 *
 * Implementation details: due to atomic_uint64_t not having an applicable copy
 * constructor, we have to provide our own.
 */
// GEMREQ-start A_15549-01#secureDeletion, A_15547-01#aesKeys
struct Tee3Context
{
    Tee3Protocol::SymmetricKeys k2ApplicationData;
    KeyId keyId;
    Tee3Protocol::VauCid vauCid;
    // GEMREQ-end A_15549-01#secureDeletion, A_15547-01#aesKeys

    /**
     * The channel id is a value that identifies a VAU channel so that the second TEE handshake
     * request can be linked with the associated temporary keys.
     *
     * Use and uniqueness of the channel id has to be discussed.
     */
    std::string channelId;

    /**
     * The TEE protocol version is pinned by the specification (A_24628) to a fixed value (2).
     */
    const uint8_t version;

    /**
     * The isPU value with which this context was created.
     */
    const bool isPU;

    // GEMREQ-start A_24629, A_24631
    /**
     * The message counter is the only variable value in this structure. It is increased for
     * every request.
     */
    static constexpr uint64_t initialMessageCounter = 0;
    std::atomic_uint64_t messageCounter;
    // GEMREQ-end A_24629, A_24631

    static constexpr uint64_t initialRequestCounter = 0;
    std::atomic_uint64_t requestCounter;


    Tee3Context(
        const Tee3Protocol::SymmetricKeys& k2ApplicationData,
        const KeyId& keyId,
        const Tee3Protocol::VauCid& vauCid,
        const std::string& channelId,
        bool isPU);

    Tee3Context() = delete;

    /**
     * The copy constructor is only allowed while `messageCounter` is 0, i.e. while the Tee3Context
     * is being set up. When the counter is >0, a copy could result in the same counter value
     * being used for more than one request.
     */
    Tee3Context(const Tee3Context& other);

    Tee3Context(Tee3Context&& other) noexcept;

    ~Tee3Context() = default;

    /**
     * Assignment operators are not supported, due to const members.
     */
    Tee3Context& operator=(const Tee3Context& other) = delete;
    Tee3Context& operator=(Tee3Context&& other) noexcept;

    bool operator==(const Tee3Context& other) const = default;

    /**
     * Return true when the context has been fully initialized.
     */
    bool isSet() const;

    struct SessionContexts
    {
        Tee3SessionContext request;
        Tee3SessionContext response;
    };

    /**
     * Create two Tee3SessionContext objects from the Tee3Context, one for the request and
     * one for the response. The returned objects should be used for exactly one session to
     * either encrypt a request and decrypt the associated response (client) or
     * decrypt an incoming request and encrypt the associated response (server).
     *
     * The Tee3SessionContext objects pin the message counter and select the correct K2 key
     * (server-to-client or client-to-server).
     *
     * The method returns objects without references to the original context to allow multiple
     * server threads to call this method for multiple sessions without interference between them.
     */
    SessionContexts createSessionContexts(const std::optional<uint64_t>& requestCounter = {});

    /**
     * Returns the current snapshot of Session Contexts in Tee3Context.
     */
    SessionContexts getCurrentSessionContexts() const;
};
} // namespace epa

#endif
