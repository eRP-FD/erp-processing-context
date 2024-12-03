/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_CRYPTO_TEE_TEECONTEXT_HXX
#define EPA_LIBRARY_CRYPTO_TEE_TEECONTEXT_HXX

#include "library/crypto/AuthorizationAssertion.hxx"
#include "library/crypto/tee/KeyId.hxx"
#include "library/crypto/tee/MessageCounter.hxx"

#include <optional>
#include <string>

namespace epa
{
enum class SymmetricKey
{
    ClientToServer,
    ServerToClient
};

struct SymmetricKeys
{
    EncryptionKey clientToServer;
    EncryptionKey serverToClient;

    const EncryptionKey& get(SymmetricKey which) const;
    EncryptionKey& get(SymmetricKey which);

    SymmetricKeys() = default;
    SymmetricKeys(EncryptionKey&& clientToServer, EncryptionKey&& serverToClient);
    SymmetricKeys(const SymmetricKeys&) = default;
    SymmetricKeys(SymmetricKeys&&) noexcept;

    SymmetricKeys& operator=(const SymmetricKeys&);
    SymmetricKeys& operator=(SymmetricKeys&&) noexcept;

    ~SymmetricKeys();

    void clear();
};


// GEMREQ-start A_14645-01
/**
 * Store keyId and symmetric keys for usage in encryption and decryption of requests and responses
 * to and from the document manager.
 *
 * Typical life cycle is
 * - created by TeeHandshake
 * - owned by HttpsClient
 * - used by ClientRequest and ClientResponse
 */
class TeeContext
{
public:
    static TeeContext createForClient(const KeyId& keyId, SymmetricKeys symmetricKeys);
    static TeeContext createEmptyContextForClient();

    static TeeContext createForServer(
        const KeyId& keyId,
        SymmetricKeys symmetricKeys,
        std::string subjectNameId,
        AuthorizationAssertion::AuthorizationType authorizationType);
    static TeeContext createEmptyContextForServer();

    TeeContext(const TeeContext& other);
    TeeContext(TeeContext&& other) = default;
    ~TeeContext();
    TeeContext& operator=(const TeeContext& other) = delete;
    TeeContext& operator=(TeeContext&& other) = default;

    bool isSet() const;

    const KeyId& getKeyId() const;

    const SymmetricKeys& getSymmetricKeys() const;

    const MessageCounter& getMessageCounter() const;
    MessageCounter& getMessageCounter();

    const std::string& getSubjectNameId() const;

    AuthorizationAssertion::AuthorizationType getAuthorizationType() const;

    /**
     * Clear all key material in a safe way.
     * Typically called as response to an error that requires abortion of the TEE protocol.
     */
    void clear();

    /**
     * Class for a smart pointer to a TeeContext which says that the TeeContext is in use. As long
     * as a non-null instance of this exists, the related TeeConnection is not replaced or removed
     * by the TeeConnectionManager or TeeContextCleaner. If no User instance exists for a
     * TeeContext, the TeeConnection and TeeContext may still be in use to complete a CloseContext
     * request (see implementation of ClientConnectionManager::closeContext).
     */
    class User
    {
    public:
        explicit User(const TeeContext* teeContext = nullptr);
        void reset();

    private:
        std::shared_ptr<void> mUserCounter;
    };

    /**
     * Whether any instance of User on this TeeContext exists.
     */
    bool isInUse() const;

private:
#ifdef FRIEND_TEST
    FRIEND_TEST(I_Document_Management_Insurant_Test, OpenContext_WrongTeeKey_Error);
    FRIEND_TEST(TeeTest, serverSide_failForWrongKeyId);
#endif

    TeeContext(
        const std::optional<KeyId>& keyId,
        SymmetricKeys symmetricKeys,
        MessageCounter counter,
        std::string subjectNameId,
        AuthorizationAssertion::AuthorizationType authorizationType);

    std::optional<KeyId> mKeyId;

    // GEMREQ-start A_15547
    SymmetricKeys mSymmetricKeys;
    // GEMREQ-end A_15547

    MessageCounter mMessageCounter;

    /**
     * subjectNameId represents the text from Subject/NameId node of authorization assertion.
     * It is Tee-Connection related data on server side and thus is stored in TeeContext.
     * It is server-side-only data, that is not intended to be used on client side.
     */
    std::string mSubjectNameId;

    /**
     * The authorization type from the authorization assertion. Set only on the server side.
     */
    AuthorizationAssertion::AuthorizationType mAuthorizationType;

    /**
     * mUserCounter.use_count() is the number of User instances plus one.
     */
    std::shared_ptr<void> mUserCounter;
};
// GEMREQ-end A_14645-01

} // namespace epa

#endif
