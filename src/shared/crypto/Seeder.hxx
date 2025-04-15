/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SEEDER_HXX
#define ERP_PROCESSING_CONTEXT_SEEDER_HXX

#include "shared/crypto/RandomSource.hxx"
#include "shared/hsm/ErpTypes.hxx"
#include "shared/util/SafeString.hxx"

#include <atomic>
#include <shared_mutex>

class HsmPool;

class Seeder
{
public:
    static constexpr size_t seedBlockSize = MaxRndBytes;
    static constexpr size_t seedBytes = 32;

    explicit Seeder(HsmPool& randomSource);

    [[nodiscard]]
    SafeString getNextSeed();

private:
    [[nodiscard]]
    SafeString getNextSeedInternal(size_t index);
    void refreshSeeds();
    void setSeeds(ErpVector&& newSeed);
    bool haveSeed(size_t index) const;

    HsmPool& mHsmPool;

    std::shared_mutex mSeedsMutex;
    ErpVector mSeeds;
    std::atomic<size_t> mNextSeedIndex = 0;
};


#endif// ERP_PROCESSING_CONTEXT_SEEDER_HXX
