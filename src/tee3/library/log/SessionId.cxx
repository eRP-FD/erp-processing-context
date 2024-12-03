/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/log/SessionId.hxx"
#include "library/util/Assert.hxx"
#include "library/util/Uuid.hxx"

#include <atomic>

#include "shared/util/ThreadNames.hxx"

namespace epa
{

const std::string SessionId::NotSetValue = "<not-set>";

thread_local SessionId* SessionId::mCurrentSessionId = nullptr;
SessionId SessionId::mUnboundSessionId(false);


SessionId& SessionId::current()
{
    auto* sessionId = mCurrentSessionId;
    if (sessionId == nullptr)
        sessionId = &mUnboundSessionId;
    return *sessionId;
}


const std::string& SessionId::sessionId() const
{
    return mSessionId;
}


void SessionId::setSessionId(std::string id)
{
    if (mThrowOnModification)
        Failure() << "unbound SessionId must not be modified";

    mSessionId = std::move(id);
}


const std::string& SessionId::uniqueSessionId() const
{
    return mUniqueSessionId;
}


const std::string& SessionId::traceId() const
{
    return mTraceId;
}


void SessionId::setTraceId(std::string id)
{
    if (mThrowOnModification)
        Failure() << "unbound SessionId must not be modified";

    mTraceId = std::move(id);
}


const std::string& SessionId::description() const
{
    return mDescription;
}


void SessionId::setDescription(const std::string& description)
{
    mDescription = description;
}


SessionId::SessionId(SessionId&& other) noexcept
  : mSessionId(std::move(other.mSessionId)),
    mTraceId(std::move(other.mTraceId)),
    mUniqueSessionId(other.mUniqueSessionId),
    mDescription(std::move(other.mDescription)),
    mThrowOnModification(other.mThrowOnModification)
{
    // Update mCurrentSessionId.
    mCurrentSessionId = this;

    // other.mSessionId is in a valid but undefined state (according to specification).
    // Make it a defined state so that the destructor knows not to reset the thread global current
    // SessionId object.
    other.mSessionId = "";
}


SessionId::SessionId(const bool isBound)
  : mSessionId(NotSetValue),
    mTraceId(NotSetValue),
    mUniqueSessionId(isBound ? Uuid().toString() : NotSetValue),
    mDescription(),
    mThrowOnModification(! isBound)
{
    if (isBound)
        activate(true);
}


SessionId::SessionId(std::string&& uniqueSessionId)
  : mSessionId(NotSetValue),
    mTraceId(NotSetValue),
    mUniqueSessionId(std::move(uniqueSessionId)),
    mDescription(),
    mThrowOnModification(false)
{
    activate(true);
}


SessionId::~SessionId()
{
    if (! mSessionId.empty())
        mCurrentSessionId = nullptr;
}


void SessionId::activate(const bool expectNoActiveSessionId)
{
    if (expectNoActiveSessionId)
    {
        Assert(mCurrentSessionId == nullptr)
            << "there already is a current SessionId (" << mCurrentSessionId << ") for thread "
            << ThreadNames::instance().getCurrentThreadName() << ", can not set it to " << this;
    }
    mCurrentSessionId = this;
}


AdhocSessionId::AdhocSessionId()
  : SessionId([] {
        Assert(mCurrentSessionId == nullptr)
            << "Can't create ad hoc session ID since the thread already has a session ID.";
        return true;
    }())
{
}

} // namespace eps
