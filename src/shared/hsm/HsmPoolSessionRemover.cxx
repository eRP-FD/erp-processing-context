/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/hsm/HsmPoolSessionRemover.hxx"


HsmPoolSessionRemover::HsmPoolSessionRemover (std::function<void(std::unique_ptr<HsmSession>&&)>&& remover)
    : mRemover(std::move(remover))
{
}


void HsmPoolSessionRemover::removeSession (std::unique_ptr<HsmSession>&& session)
{
    // The mutex is required to avoid `notifyPoolRelease` exchanging mRemover while the previous value is being used.
    std::lock_guard lock (mMutex);
    if (mRemover)
        mRemover(std::move(session));
}


void HsmPoolSessionRemover::notifyPoolRelease (void)
{
    std::lock_guard lock (mMutex);
    mRemover = {};
}
