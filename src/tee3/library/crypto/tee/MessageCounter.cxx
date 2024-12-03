/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/crypto/tee/MessageCounter.hxx"
#include "library/crypto/tee/TeeConstants.hxx"
#include "library/crypto/tee3/Tee3Protocol.hxx"
#include "library/log/Logging.hxx"
#include "library/util/Assert.hxx"

#include <limits>

namespace epa
{

// "VAU-Protokoll: Client, verschlüsselte Kommunikation (1)"
// GEMREQ-start A_16945-02
MessageCounter MessageCounter::createForClient()
{
    // A_16945-02: initial value of message counter for client is 1
    return MessageCounter(true, 1);
}
// GEMREQ-end A_16945-02


MessageCounter MessageCounter::createForServer()
{
    // A_16952-02: initial value of message counter for server is 0
    return MessageCounter(false, 0);
}


MessageCounter MessageCounter::createForTest(bool isClient, ValueType initialValue)
{
    return MessageCounter(isClient, initialValue);
}


MessageCounter::MessageCounter(const bool isClient, const ValueType initialValue)
  : mMutex(),
    mIsClient(isClient),
    mRequestValue(initialValue)
{
}


// NOLINTNEXTLINE(*-member-init)
MessageCounter::MessageCounter(const MessageCounter& other) noexcept
{
    *this = other;
}


// NOLINTNEXTLINE(*-member-init)
MessageCounter::MessageCounter(MessageCounter&& other) noexcept
{
    *this = std::move(other);
}


MessageCounter& MessageCounter::operator=(const MessageCounter& other) noexcept
{
    if (&other == this)
        return *this;

    std::scoped_lock joinedLock(mMutex, other.mMutex);

    mIsClient = other.mIsClient;
    mRequestValue = other.mRequestValue;

    return *this;
}


MessageCounter& MessageCounter::operator=(MessageCounter&& other) noexcept
{
    std::scoped_lock joinedLock(mMutex, other.mMutex);

    mIsClient = other.mIsClient;
    mRequestValue = other.mRequestValue;
    other.mRequestValue = 0;

    return *this;
}


MessageCounter::ValueType MessageCounter::getRequestValue() const
{
    std::lock_guard lock(mMutex);

    return mRequestValue;
}


// GEMREQ-start A_16957-01#getResponseValue
MessageCounter::ValueType MessageCounter::getResponseValue() const
{
    std::lock_guard lock(mMutex);

    return guardedAdd(mRequestValue, 1);
}
// GEMREQ-end A_16957-01#getResponseValue


bool MessageCounter::operator==(const MessageCounter& other) const
{
    std::scoped_lock joinedLock(mMutex, other.mMutex);

    return mRequestValue == other.mRequestValue && mIsClient == other.mIsClient;
}


// GEMREQ-start A_17069#guardedAdd
std::uint64_t MessageCounter::guardedAdd(const ValueType value, const ValueType offset)
{
    Assert(std::numeric_limits<std::uint64_t>::max() - value >= offset)
        << assertion::exceptionType<Tee3Protocol::AbortingException>()
        << vau::error::messageCounterOverflow;

    return value + offset;
}
// GEMREQ-end A_17069#guardedAdd


MessageCounter::ExclusiveAccess MessageCounter::getExclusiveAccess()
{
    return ExclusiveAccess(*this);
}


MessageCounter::ExclusiveAccess::ExclusiveAccess(MessageCounter& messageCounter)
  : mLock(messageCounter.mMutex),
    mMessageCounter(messageCounter)
{
}


MessageCounter::ValueType MessageCounter::ExclusiveAccess::getRequestValue() const
{
    return mMessageCounter.mRequestValue;
}


MessageCounter::ValueType MessageCounter::ExclusiveAccess::getResponseValue() const
{
    return guardedAdd(mMessageCounter.mRequestValue, 1);
}


void MessageCounter::ExclusiveAccess::setRequestCounter(const ValueType newValue)
{
    Assert(! mMessageCounter.mIsClient)
        << "only the server is allowed to modify its message counter";

    if (newValue <= mMessageCounter.mRequestValue)
        throw Tee3Protocol::Exception(vau::error::invalidCounterValue);
    mMessageCounter.mRequestValue = newValue;

    TLOG(INFO2) << "did set server counters to "
                << logging::confidential(mMessageCounter.mRequestValue) << "/"
                << logging::confidential(mMessageCounter.mRequestValue + 1);
}


// GEMREQ-start A_16957-01#advanceToNextRequest, A_17069#advanceToNextRequest, A_16952-02#advanceToNextRequest
void MessageCounter::ExclusiveAccess::advanceToNextRequest()
{
    if (mMessageCounter.mIsClient)
    {
        // For the client we have to add 2 to move from one request to the next.
        mMessageCounter.mRequestValue = guardedAdd(mMessageCounter.mRequestValue, 2);
        TLOG(INFO3) << "increasing client request counters to "
                    << logging::sensitive(mMessageCounter.mRequestValue) << "/"
                    << logging::sensitive(mMessageCounter.mRequestValue + 1);
    }
    else
    {
        // Server sets the message counter to the value that comes in from the client.
        // It has to add only 1 to advance to the next value because the value that is provided by
        // the client is at least 1 higher than the old server counter.
        mMessageCounter.mRequestValue = guardedAdd(mMessageCounter.mRequestValue, 1);
        TLOG(INFO3) << "increasing server request counters to "
                    << logging::confidential(mMessageCounter.mRequestValue) << "/"
                    << logging::confidential(mMessageCounter.mRequestValue + 1);
    }
}
// GEMREQ-end A_16957-01#advanceToNextRequest, A_17069#advanceToNextRequest, A_16952-02#advanceToNextRequest
} // namespace epa
