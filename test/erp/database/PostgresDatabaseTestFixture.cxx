// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "test/erp/database/PostgresDatabaseTestFixture.hxx"
#include "mock/crypto/MockCryptography.hxx"
#include "test/mock/MockBlobDatabase.hxx"

PostgresDatabaseTest::PostgresDatabaseTest()
    : mIdpPrivateKey(MockCryptography::getIdpPrivateKey())
    , mJwtBuilder(mIdpPrivateKey)
    , mPharmacy(mJwtBuilder.makeJwtApotheke().stringForClaim(JWT::idNumberClaim).value())
    , mConnection(nullptr)
    , mDatabase(nullptr)
    , mBlobCache(nullptr)
    , mHsmPool(nullptr)
    , mKeyDerivation(nullptr)
    , mDurationConsumerGuard(nullptr)
{
    mBlobCache = MockBlobDatabase::createBlobCache(MockBlobCache::MockTarget::MockedHsm);

    auto blobCache = mBlobCache;
    mHsmPool = std::make_unique<HsmPool>(
        std::make_unique<HsmMockFactory>(std::make_unique<HsmMockClient>(), std::move(blobCache)),
        TeeTokenUpdater::createMockTeeTokenUpdaterFactory(), std::make_shared<Timer>());
    mKeyDerivation = std::make_unique<KeyDerivation>(*mHsmPool);
    mDurationConsumerGuard = std::make_unique<DurationConsumerGuard>("PostgresDatabaseTest");
}
