/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/hsm/HsmPoolSession.hxx"

#include "erp/util/Expect.hxx"


HsmPoolSession::HsmPoolSession (void)
    : mSession(),
      mSessionRemover()
{
}

HsmPoolSession::HsmPoolSession (std::unique_ptr<HsmSession>&& session, const std::shared_ptr<HsmPoolSessionRemover>& sessionRemover)
    : mSession(std::move(session)),
      mSessionRemover(sessionRemover)
{
    Expect(mSession != nullptr, "session is missing");
}


HsmPoolSession::HsmPoolSession (HsmPoolSession&& other) noexcept
    : mSession(std::move(other.mSession)),
      mSessionRemover(std::move(other.mSessionRemover))
{
}


HsmPoolSession& HsmPoolSession::operator= (HsmPoolSession&& other) noexcept
{
    if (this != &other)
    {
        releaseBackIntoPool();

        mSession = std::move(other.mSession);
        mSessionRemover = other.mSessionRemover;
    }
    return *this;
}


HsmPoolSession::~HsmPoolSession (void)
{
    releaseBackIntoPool();
}

HsmSession& HsmPoolSession::session()
{
    return *mSession;
}

void HsmPoolSession::releaseBackIntoPool (void)
{
    if (mSession != nullptr && mSessionRemover != nullptr)
    {
        mSessionRemover->removeSession(std::move(mSession));
    }
}
