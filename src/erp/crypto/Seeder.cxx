/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/crypto/Seeder.hxx"

#include "erp/crypto/SecureRandomGenerator.hxx"
#include "erp/hsm/HsmPool.hxx"
#include "erp/server/ThreadPool.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/TLog.hxx"


Seeder::Seeder(HsmPool& randomSource)
    : mHsmPool(randomSource)
{
    static_assert((seedBlockSize % seedBytes) == 0, "seedBlockSize must be divisible by seedBytes");
}


void Seeder::refreshSeeds()
{
    TVLOG(3) << "Fetching new seed block from HSM.";
    auto hsmPoolSession = mHsmPool.acquire();
    setSeeds(hsmPoolSession.session().getRandomData(seedBlockSize));
}

void Seeder::setSeeds(ErpVector&& newSeed)
{
    Expect(newSeed.size() == seedBlockSize, "Invalid number of bytes received from HSM.");
    OPENSSL_cleanse(mSeeds.data(), mSeeds.size());
    mSeeds.swap(newSeed);
    mNextSeedIndex = 0;
}

bool Seeder::haveSeed(size_t index) const
{
    return (index + seedBytes <= mSeeds.size());
}


SafeString Seeder::getNextSeedInternal(size_t index)
{
    SafeString seed{SafeString::no_zero_fill, seedBytes};
    std::copy(mSeeds.begin() + gsl::narrow<ErpVector::difference_type>(index),
              mSeeds.begin() + gsl::narrow<ErpVector::difference_type>(index + seedBytes),
              static_cast<char*>(seed));
    return seed;
}

SafeString Seeder::getNextSeed()
{
    {
        std::shared_lock lock{mSeedsMutex};
        size_t index = mNextSeedIndex.fetch_add(seedBytes);
        if (haveSeed(index))
        {
            return getNextSeedInternal(index);
        }
    }
    std::lock_guard lock{mSeedsMutex};
    size_t index = mNextSeedIndex.fetch_add(seedBytes);
    if (!haveSeed(index))
    {
        refreshSeeds();
        index = mNextSeedIndex.fetch_add(seedBytes);
    }
    return getNextSeedInternal(index);
}

