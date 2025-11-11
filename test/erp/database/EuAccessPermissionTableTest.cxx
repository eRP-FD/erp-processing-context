// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "erp/model/eu/EuAccessPermission.hxx"
#include "test/erp/database/PostgresDatabaseTestFixture.hxx"
class EuAccessPermissionTableTest : public PostgresDatabaseTest
{
    void SetUp() override
    {
        if (!usePostgres())
        {
            GTEST_SKIP();
        }
        PostgresDatabaseTest::SetUp();
    }
    void cleanup() override
    {
        if (usePostgres())
        {
            auto deleteTxn = createTransaction();
            deleteTxn.exec("DELETE FROM erp.eu_access_permission");
            deleteTxn.commit();
        }
    }
};

TEST_F(EuAccessPermissionTableTest, createEuAccessPermission)
{
    ASSERT_NO_THROW(database().createEuAccessPermission(
        model::Kvnr{"X123456789"},
        model::EuAccessPermission{model::EuAccessCode{SafeString{"aaaaaa"}}, model::CountryCode("FR")}));

    ASSERT_THROW(
        database().createEuAccessPermission(
            model::Kvnr{"X123456789"},
            model::EuAccessPermission{model::EuAccessCode{SafeString{"aaaaaa"}}, model::CountryCode("FR")}),
        pqxx::unique_violation);
    database().commitTransaction();
}

TEST_F(EuAccessPermissionTableTest, deleteEuAccessPermission)
{
    ASSERT_NO_THROW(database().createEuAccessPermission(
        model::Kvnr{"X123456789"},
        model::EuAccessPermission{model::EuAccessCode{SafeString{"aaaaaa"}}, model::CountryCode("FR")}));

    ASSERT_NO_THROW(database().deleteEuAccessPermission(model::Kvnr{"X123456789"}));
    ASSERT_NO_THROW(database().deleteEuAccessPermission(model::Kvnr{"X123456789"}));
    database().commitTransaction();
}

TEST_F(EuAccessPermissionTableTest, retrieveEuAccessPermission)
{
    auto retrieved = database().retrieveEuAccessPermission(model::Kvnr{"X123456789"});
    ASSERT_FALSE(retrieved.has_value());

    ASSERT_NO_THROW(database().createEuAccessPermission(
    model::Kvnr{"X123456789"},
    model::EuAccessPermission{model::EuAccessCode{SafeString{"aaaaaa"}}, model::CountryCode("FR")}));
    database().commitTransaction();

    retrieved = database().retrieveEuAccessPermission(model::Kvnr{"X123456789"});
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->getCountryCode().toString(), "FR");
    EXPECT_STREQ(retrieved->getAccessCode().toString().c_str(), "aaaaaa");

    database().commitTransaction();
}