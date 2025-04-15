/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/crypto/tee/TeeContext.hxx"
#include "library/util/Assert.hxx"

#include <utility>

#include "shared/util/String.hxx"

namespace epa
{


namespace
{
    struct EmptyStruct
    {
    };
}


// ===== TeeContext ==========================================================

TeeContext::TeeContext(
    const std::optional<KeyId>& keyId,
    SymmetricKeys symmetricKeys,
    MessageCounter counter,
    std::string subjectNameId,
    AuthorizationAssertion::AuthorizationType authorizationType)
  : mKeyId(keyId),
    mSymmetricKeys(std::move(symmetricKeys)),
    mMessageCounter(std::move(counter)),
    mSubjectNameId(std::move(subjectNameId)),
    mAuthorizationType(authorizationType),
    mUserCounter(std::make_shared<EmptyStruct>())
{
}


TeeContext::TeeContext(const TeeContext& other)
  : mKeyId(other.mKeyId),
    mSymmetricKeys(other.mSymmetricKeys),
    mMessageCounter(other.mMessageCounter),
    mSubjectNameId(other.mSubjectNameId),
    mAuthorizationType(other.mAuthorizationType),
    mUserCounter(std::make_shared<EmptyStruct>())
{
}


TeeContext::~TeeContext()
{
    clear();
}


// "VAU-Protokoll: Client, verschlüsselte Kommunikation (1)"
// GEMREQ-start A_16945-02#messageCounter
TeeContext TeeContext::createForClient(const KeyId& keyId, SymmetricKeys symmetricKeys)
{
    return TeeContext(
        keyId,
        std::move(symmetricKeys),
        MessageCounter::createForClient(),
        "",
        AuthorizationAssertion::NO_AUTHORIZATION);
}
// GEMREQ-end A_16945-02#messageCounter


TeeContext TeeContext::createForServer(
    const KeyId& keyId,
    SymmetricKeys symmetricKeys,
    std::string subjectNameId,
    AuthorizationAssertion::AuthorizationType authorizationType)
{
    return TeeContext(
        keyId,
        std::move(symmetricKeys),
        MessageCounter::createForServer(),
        std::move(subjectNameId),
        authorizationType);
}


TeeContext TeeContext::createEmptyContextForServer()
{
    return TeeContext(
        std::nullopt,
        SymmetricKeys(),
        MessageCounter::createForServer(),
        "",
        AuthorizationAssertion::NO_AUTHORIZATION);
}


TeeContext TeeContext::createEmptyContextForClient()
{
    return TeeContext(
        std::nullopt,
        SymmetricKeys(),
        MessageCounter::createForClient(),
        "",
        AuthorizationAssertion::NO_AUTHORIZATION);
}


bool TeeContext::isSet() const
{
    return mKeyId.has_value() && ! mSymmetricKeys.clientToServer.empty()
           && ! mSymmetricKeys.serverToClient.empty();
}


const KeyId& TeeContext::getKeyId() const
{
    Assert(mKeyId.has_value());
    return *mKeyId;
}


const SymmetricKeys& TeeContext::getSymmetricKeys() const
{
    return mSymmetricKeys;
}


const MessageCounter& TeeContext::getMessageCounter() const
{
    return mMessageCounter;
}


MessageCounter& TeeContext::getMessageCounter()
{
    return mMessageCounter;
}


const std::string& TeeContext::getSubjectNameId() const
{
    return mSubjectNameId;
}


AuthorizationAssertion::AuthorizationType TeeContext::getAuthorizationType() const
{
    return mAuthorizationType;
}


// "VAU-Protokoll: Aktionen bei Protokollabbruch"
// GEMREQ-start A_16849#clear
void TeeContext::clear()
{
    mKeyId.reset();
    mSymmetricKeys.clear();
}
// GEMREQ-end A_16849#clear


TeeContext::User::User(const TeeContext* teeContext)
  : mUserCounter(teeContext ? teeContext->mUserCounter : nullptr)
{
}


void TeeContext::User::reset()
{
    mUserCounter.reset();
}


bool TeeContext::isInUse() const
{
    return mUserCounter.use_count() > 1;
}


// ===== SymmetricKeys =======================================================

const EncryptionKey& SymmetricKeys::get(SymmetricKey which) const
{
    if (which == SymmetricKey::ClientToServer)
        return clientToServer;
    else
        return serverToClient;
}


EncryptionKey& SymmetricKeys::get(SymmetricKey which)
{
    if (which == SymmetricKey::ClientToServer)
        return clientToServer;
    else
        return serverToClient;
}


SymmetricKeys::SymmetricKeys(EncryptionKey&& clientToServer, EncryptionKey&& serverToClient)
  : clientToServer{std::move(clientToServer)},
    serverToClient{std::move(serverToClient)}
{
}


SymmetricKeys::SymmetricKeys(SymmetricKeys&& other) noexcept
{
    // Delegate to move assignment operator.
    *this = std::move(other);
}


SymmetricKeys& SymmetricKeys::operator=(const SymmetricKeys& other)
{
    if (this != &other)
    {
        // Delegate to move assignment operator, and rely on the destructor to clean up the copy.
        *this = SymmetricKeys{other};
    }

    return *this;
}


SymmetricKeys& SymmetricKeys::operator=(SymmetricKeys&& other) noexcept
{
    if (this != &other)
    {
        clear();
        // This actually creates a copy, then overwrites the original, as there is no guarantee that
        // moving a std::string will move a pointer. It could also create a copy of a string that
        // uses the small string optimization.
        clientToServer = other.clientToServer;
        serverToClient = other.serverToClient;
        other.clear();
    }

    return *this;
}


// "ePA-Frontend des Versicherten: Kommunikation zwischen ePA-FdV und VAU"
// GEMREQ-start A_15546#clearKeys, A_15547
SymmetricKeys::~SymmetricKeys()
{
    // Explanation for A_15546:
    // This will be called implicitly when a ClientSession is destroyed, which frees
    // its ClientConnection object, and indirectly also these keys.
    // This complies with the section of A_15546 that specifies that the client should safely delete
    // VAU/TEE protocol keys.
    clear();
}
// GEMREQ-end A_15546


// GEMREQ-start A_16849#SymmetricKeys_clear, A_15547#SymmetricKeys_clear
void SymmetricKeys::clear()
{
    clientToServer.cleanse();
    serverToClient.cleanse();
}
// GEMREQ-end A_16849#SymmetricKeys_clear, A_15547#SymmetricKeys_clear
// GEMREQ-end A_15546#clearKeys, A_15547

} // namespace epa
