/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/hsm/production/ProductionBlobDatabase.hxx"

#include "erp/database/PostgresBackend.hxx"
#include "erp/database/PostgresConnection.hxx"
#include "erp/util/Configuration.hxx"
#include "test/util/BlobDatabaseHelper.hxx"
#include "test/util/TestConfiguration.hxx"

#include "test_config.h"
#include "erp/erp-serverinfo.hxx"

#include <gtest/gtest.h>


/**
* Test in this file require a ProductionBlobDatabase to work on and can not be applied to an instance of
 * the BlobDatabase interface.
 * Any tests that can work with the BlobDatabase interface belong in the BlobDatabaseTest or even BlobCacheTest.
*/

class ProductionBlobDatabaseTestBase
{
public:
    std::unique_ptr<ProductionBlobDatabase> database;

    void setup (void)
    {
        // Skip test when the postgres database is not active.
        if (!usePostgres())
            GTEST_SKIP();

        database = std::make_unique<ProductionBlobDatabase>();

        BlobDatabaseHelper::removeUnreferencedBlobs();
    }

    pqxx::result execute (const std::string& query) const
    {
        auto transaction = database->createTransaction();
        auto result = transaction->exec(query);
        transaction->commit();
        return result;
    }

    static bool usePostgres(void)
    {
        return TestConfiguration::instance().getOptionalBoolValue(TestConfigurationKey::TEST_USE_POSTGRES, false);
    }
};
class ProductionBlobDatabaseTest
    : public ProductionBlobDatabaseTestBase,
      public testing::Test
{
public:
    void SetUp (void) override
    {
        setup();
    }
};

class ProductionBlobDatabaseHostIpTest
    : public ProductionBlobDatabaseTestBase,
      public testing::TestWithParam<std::tuple<BlobType, bool, bool>>
{
public:
    void SetUp (void) override
    {
        setup();
    }
};

/**
 * Verify that blobs with type Attestation* are stored with the current host ip.
 */
TEST_P(ProductionBlobDatabaseHostIpTest, storeBlob) // NOLINT(readability-function-cognitive-complexity)
{
    const auto[blobType, isHostIpExpected, isBuildExpected] = GetParam();

    std::string entryName = "ProductionBlobDatabaseHostIpTest.storeBlob.";
    entryName += magic_enum::enum_name(blobType);

    BlobDatabase::Entry entry;
    entry.type = blobType;
    entry.name = ErpVector::create(entryName);
    database->storeBlob(std::move(entry));

    ASSERT_FALSE(Configuration::instance().serverHost().empty());

    const auto result = execute("SELECT host_ip, build FROM erp.blob WHERE name='" + entryName + "'");
    ASSERT_EQ(result.size(), 1);
    const auto& row = result.front();
    ASSERT_EQ(row.size(), 2);

    if (isHostIpExpected)
        ASSERT_EQ(row[0].as<std::string>(), Configuration::instance().serverHost());
    else
        ASSERT_TRUE(row[0].is_null());
    if (isBuildExpected)
        ASSERT_EQ(row[1].as<std::string>(), std::string(ErpServerInfo::ReleaseVersion())
                                    + "/" + std::string(ErpServerInfo::BuildVersion())
                                    + "/" + std::string(ErpServerInfo::BuildType()));
    else
        ASSERT_TRUE(row[1].is_null());
    (void)execute("DELETE FROM erp.blob WHERE name='" + entryName + "'");
}

INSTANTIATE_TEST_SUITE_P(
    HostIpTests,
    ProductionBlobDatabaseHostIpTest,
    testing::Values(
             std::tuple<BlobType, bool, bool>{BlobType::EndorsementKey,             true,  false},
             std::tuple<BlobType, bool, bool>{BlobType::AttestationPublicKey,       true,  false},
             std::tuple<BlobType, bool, bool>{BlobType::AttestationKeyPair,         true,  false},
             std::tuple<BlobType, bool, bool>{BlobType::Quote,                      false, true},
             std::tuple<BlobType, bool, bool>{BlobType::EciesKeypair,               false, false},
             std::tuple<BlobType, bool, bool>{BlobType::TaskKeyDerivation,          false, false},
             std::tuple<BlobType, bool, bool>{BlobType::CommunicationKeyDerivation, false, false},
             std::tuple<BlobType, bool, bool>{BlobType::AuditLogKeyDerivation,      false, false},
             std::tuple<BlobType, bool, bool>{BlobType::KvnrHashKey,                false, false},
             std::tuple<BlobType, bool, bool>{BlobType::TelematikIdHashKey,         false, false},
             std::tuple<BlobType, bool, bool>{BlobType::VauSig,                     false, false}
));

// Pretty names for individual test cases.
void PrintTo(const std::tuple<BlobType, bool, bool>& tuple, std::ostream* os)
{
    *os << magic_enum::enum_name(std::get<0>(tuple));
}



/**
 * Verify that blobs with a host ip are only retrieved if the host ip is identical to the current host ip.
 */
TEST_F(ProductionBlobDatabaseTest, getAllBlobsSortedById_withHostIp)
{
    // BlobType AttestationPublicKey == 2                                                              v
    const auto result1 = execute("INSERT INTO erp.blob (type, host_ip, name, data, generation) VALUES (2, '127.0.0.1', 'localhost-1', 'data-1', 1) RETURNING blob_id");
                         execute("INSERT INTO erp.blob (type, host_ip, name, data, generation) VALUES (2, '127.0.0.2', 'localhost-2', 'data-2', 1) RETURNING blob_id");

    const auto id1 = result1.front()[0].as<size_t>();

    const auto blobs = database->getAllBlobsSortedById();
    ASSERT_GE(blobs.size(), 1);
    ASSERT_EQ(blobs.back().name, ErpVector::create("localhost-1"));
    ASSERT_EQ(blobs.back().id, id1);
}


/**
 * Verify that blobs with a host ip are only retrieved if the host ip is identical to the current host ip.
 */
TEST_F(ProductionBlobDatabaseTest, getAllBlobsSortedById_withoutHostIp)
{
    // BlobType EndorsementKey == 1                                                           v
    const auto result1 = execute("INSERT INTO erp.blob (type, name, data, generation) VALUES (1, 'localhost-1', 'data-1', 1) RETURNING blob_id");

    const auto id1 = result1.front()[0].as<size_t>();

    const auto blobs = database->getAllBlobsSortedById();
    ASSERT_GE(blobs.size(), 1);
    ASSERT_EQ(blobs.back().name, ErpVector::create("localhost-1"));
    ASSERT_EQ(blobs.back().id, id1);
}


/**
 * Verify that blobs with a host ip are only retrieved if the host ip is identical to the current host ip.
 */
TEST_F(ProductionBlobDatabaseTest, getBlob_withHostIp) // NOLINT(readability-function-cognitive-complexity)
{
    const auto result1 = execute("INSERT INTO erp.blob (type, host_ip, name, data, generation) VALUES (2, '127.0.0.1', 'localhost-1', 'data-1', 1) RETURNING blob_id");
    const auto result2 = execute("INSERT INTO erp.blob (type, host_ip, name, data, generation) VALUES (2, '127.0.0.2', 'localhost-2', 'data-2', 1) RETURNING blob_id");

    const auto id1 = result1.front()[0].as<size_t>();
    const auto id2 = result2.front()[0].as<size_t>();

    // Verify that the first entry is found because it has host_ip == "127.0.0.1".
    {
        BlobDatabase::Entry blob;
        ASSERT_NO_THROW(
            blob = database->getBlob(BlobType::AttestationPublicKey, gsl::narrow_cast<BlobId>(id1)));
        ASSERT_EQ(blob.name, ErpVector::create("localhost-1"));
        ASSERT_EQ(blob.id, id1);
    }

    // Verify that the first entry is not found because it has host_ip == "127.0.0.2".
    {
        ASSERT_ANY_THROW(
            database->getBlob(BlobType::AttestationPublicKey, gsl::narrow_cast<BlobId>(id2)));
    }
}


/**
 * Verify that blobs without a host ip are retrieved regardless of the current host ip.
 */
TEST_F(ProductionBlobDatabaseTest, getBlob_withoutHostIp)
{
    const auto result1 = execute("INSERT INTO erp.blob (type, name, data, generation) VALUES (1, 'localhost-1', 'data-1', 1) RETURNING blob_id");

    const auto id1 = result1.front()[0].as<size_t>();

    const auto blobs = database->getAllBlobsSortedById();
    ASSERT_GE(blobs.size(), 1);
    ASSERT_EQ(blobs.back().name, ErpVector::create("localhost-1"));
    ASSERT_EQ(blobs.back().id, id1);
}


/**
 * Verify that the content of the build column is written for quote blobs and that it has the right format.
 */
TEST_F(ProductionBlobDatabaseTest, buildForQuote)
{
    BlobDatabase::Entry entry;
    entry.type = BlobType::Quote;
    entry.name = ErpVector::create("blob-name");
    database->storeBlob(std::move(entry));

    const auto result = execute("SELECT build FROM erp.blob WHERE name='blob-name'");
    ASSERT_EQ(result.size(), 1);
    ASSERT_EQ(result.front()[0].as<std::string>(), std::string(ErpServerInfo::ReleaseVersion())
                                                       .append("/")
                                                       .append(ErpServerInfo::BuildVersion())
                                                       .append("/")
                                                       .append(ErpServerInfo::BuildType()));
}


/**
 * Verify that the content of the build column is empty for blobs other than quotes.
 */
TEST_F(ProductionBlobDatabaseTest, noBuildForOtherBlobs)
{
    BlobDatabase::Entry entry;
    entry.type = BlobType::AttestationPublicKey;
    entry.name = ErpVector::create("blob-name");
    database->storeBlob(std::move(entry));

    const auto result = execute("SELECT build FROM erp.blob WHERE name='blob-name'");
    ASSERT_EQ(result.size(), 1);
    ASSERT_TRUE(result.front()[0].is_null());
}


/**
 * Delete operates on unique name and does not require filtering by host_ip
 */
TEST_F(ProductionBlobDatabaseTest, deleteBlob)
{
    size_t expectedCount = 0;
    {
        const auto countResult = execute("SELECT COUNT(*) FROM erp.blob");
        expectedCount = countResult.front()[0].as<size_t>();
    }

    const auto result1 = execute("INSERT INTO erp.blob (type,          name, data, generation) VALUES (1,              'ek',   'data', 1) RETURNING blob_id");
    const auto result2 = execute("INSERT INTO erp.blob (type, host_ip, name, data, generation) VALUES (2, '127.0.0.1', 'ak-1', 'data', 1) RETURNING blob_id");
    const auto result3 = execute("INSERT INTO erp.blob (type, host_ip, name, data, generation) VALUES (2, '127.0.0.2', 'ak-2', 'data', 1) RETURNING blob_id");
    expectedCount += 3;
    {
        const auto countResult = execute("SELECT COUNT(*) FROM erp.blob");
        ASSERT_EQ(countResult.front()[0].as<size_t>(), expectedCount);
    }

    database->deleteBlob(BlobType::EndorsementKey, ErpVector::create("ek"));
    --expectedCount;
    {
        const auto countResult = execute("SELECT COUNT(*) FROM erp.blob");
        ASSERT_EQ(countResult.front()[0].as<size_t>(), expectedCount);
    }
    --expectedCount;
    database->deleteBlob(BlobType::AttestationPublicKey, ErpVector::create("ak-1"));

    {
        const auto countResult = execute("SELECT COUNT(*) FROM erp.blob");
        ASSERT_EQ(countResult.front()[0].as<size_t>(), expectedCount);
    }

    database->deleteBlob(BlobType::AttestationPublicKey, ErpVector::create("ak-2"));
    --expectedCount;
    {
        const auto countResult = execute("SELECT COUNT(*) FROM erp.blob");
        ASSERT_EQ(countResult.front()[0].as<size_t>(), expectedCount);
    }
}
