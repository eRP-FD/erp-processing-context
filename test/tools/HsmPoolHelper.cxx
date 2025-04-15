/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "test/tools/HsmPoolHelper.hxx"


namespace {
    class DerivedHsmPool : public HsmPool
    {
    public:
        /**
         * Reset an HsmPool into the same state it is in directly after creation.
         */
        void reset (void)
        {
            std::lock_guard lock (mMutex);

            mIsPoolReleased = false;
            Expect(mActiveSessionCount==0, "can not reset the HSM pool,there are still active sessions");

            decltype(mAvailableHsmSessions) sessions;
            sessions.swap(mAvailableHsmSessions);
        }
    };
}



void HsmPoolHelper::resetHsmPool (HsmPool& pool)
{
    // This is admittedly an ugly cast. But better than adding assignment operators and constructors to HsmPool
    // that otherwise would not be necessary.
    auto& derivedPool = reinterpret_cast<DerivedHsmPool&>(pool);
    derivedPool.reset();
}
