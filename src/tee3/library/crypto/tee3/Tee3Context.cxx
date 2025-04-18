/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/crypto/tee3/Tee3Context.hxx"
#include "library/util/Assert.hxx"

namespace epa
{
// GEMREQ-start A_24629
Tee3Context::Tee3Context(
    const Tee3Protocol::SymmetricKeys& k2ApplicationData,
    const KeyId& keyId,
    const Tee3Protocol::VauCid& vauCid,
    const std::string& channelId, bool isPU)
  : k2ApplicationData(k2ApplicationData),
    keyId(keyId),
    vauCid(vauCid),
    channelId(channelId),
    version(0x02),
    isPU(isPU),
    messageCounter(initialMessageCounter),
    requestCounter(initialRequestCounter)
{
}
// GEMREQ-end A_24629


Tee3Context::Tee3Context(const Tee3Context& other)
  : Tee3Context(other.k2ApplicationData, other.keyId, other.vauCid, other.channelId, other.isPU)
{
    Assert(
        other.messageCounter == initialMessageCounter
        && other.requestCounter == initialRequestCounter)
        << "can not copy TEE context that is already in use";
}


Tee3Context::Tee3Context(Tee3Context&& other) noexcept
  : k2ApplicationData(std::move(other.k2ApplicationData)),
    keyId(other.keyId),
    vauCid(std::move(other.vauCid)),
    channelId(std::move(other.channelId)),
    version(other.version),
    isPU(other.isPU),
    messageCounter(initialMessageCounter),
    requestCounter(initialRequestCounter)
{
    Assert(
        other.messageCounter == initialMessageCounter
        && other.requestCounter == initialRequestCounter)
        << "can not copy TEE context that is already in use";
}


bool Tee3Context::isSet() const
{
    return ! k2ApplicationData.clientToServer.empty() && ! k2ApplicationData.serverToClient.empty();
}

// GEMREQ-start A_24629#incrementCounter
Tee3Context::SessionContexts Tee3Context::createSessionContexts(
    const std::optional<uint64_t>& newRequestCounter)
{
    // The message counter is fixed to the next/increased message counter value.
    // Only one of the two session contexts should use this value to encrypt a response or a request.
    // The other session context should be used to decrypt a request or response.
    auto messageCounterTemp = ++messageCounter;
    uint64_t requestCounterTemp = 0;
    if (newRequestCounter.has_value())
    {
        requestCounter.store(*newRequestCounter);
        requestCounterTemp = *newRequestCounter;
    }
    else
        requestCounterTemp = ++requestCounter;

    return {
        Tee3SessionContext{
            .k2Key = k2ApplicationData.clientToServer,
            .keyId = keyId,
            .vauCid = vauCid,
            .version = version,
            .isPU = isPU,
            .messageCounter = messageCounterTemp,
            .requestCounter = requestCounterTemp,
            .direction = Tee3SessionContext::Direction::ClientToServer},
        Tee3SessionContext{
            .k2Key = k2ApplicationData.serverToClient,
            .keyId = keyId,
            .vauCid = vauCid,
            .version = version,
            .isPU = isPU,
            .messageCounter = messageCounterTemp,
            .requestCounter = requestCounterTemp,
            .direction = Tee3SessionContext::Direction::ServerToClient},
    };
}
// GEMREQ-end A_24629#incrementCounter

Tee3Context::SessionContexts Tee3Context::getCurrentSessionContexts() const
{
    const uint64_t messageCounterTemp = messageCounter;
    const uint64_t requestCounterTemp = requestCounter;
    return {
        Tee3SessionContext{
            .k2Key = k2ApplicationData.clientToServer,
            .keyId = keyId,
            .vauCid = vauCid,
            .version = version,
            .isPU = isPU,
            .messageCounter = messageCounterTemp,
            .requestCounter = requestCounterTemp,
            .direction = Tee3SessionContext::Direction::ClientToServer},
        Tee3SessionContext{
            .k2Key = k2ApplicationData.serverToClient,
            .keyId = keyId,
            .vauCid = vauCid,
            .version = version,
            .isPU = isPU,
            .messageCounter = messageCounterTemp,
            .requestCounter = requestCounterTemp,
            .direction = Tee3SessionContext::Direction::ServerToClient},
    };
}


Tee3Context& Tee3Context::operator=(Tee3Context&& other) noexcept
{
    if (this != &other)
    {
        k2ApplicationData = std::move(other.k2ApplicationData);
        keyId = other.keyId;
        vauCid = std::move(other.vauCid);
        channelId = std::move(other.channelId);

        Assert(version == other.version);
        Assert(isPU == other.isPU);

        messageCounter.store(other.messageCounter);
        requestCounter.store(other.requestCounter);
    }
    return *this;
}

} // namespace epa
