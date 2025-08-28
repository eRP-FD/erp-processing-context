// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH


#include "test/erp/database/PostgresDatabaseTest.hxx"
class EuCountryCodeTableTest : public PostgresDatabaseTest
{
    void SetUp() override
    {
        if (!usePostgres())
        {
            GTEST_SKIP();
            return;
        }
        PostgresDatabaseTest::SetUp();
        auto txn = createTransaction();
        txn.exec("INSERT INTO erp.eu_country_codes (iso_3166_alpha_2) VALUES ('DE')");
        txn.exec("INSERT INTO erp.eu_country_codes (iso_3166_alpha_2) VALUES ('FR')");
        txn.commit();
    }

    void cleanup() override
    {
        if(usePostgres())
        {
            auto deleteTxn = createTransaction();
            deleteTxn.exec("DELETE FROM erp.eu_country_codes");
            deleteTxn.commit();
        }
    }
};

TEST_F(EuCountryCodeTableTest, existsCountryCode)
{
    EXPECT_TRUE(database().existsCountryCode(model::CountryCode{"DE"}));
    EXPECT_TRUE(database().existsCountryCode(model::CountryCode{"FR"}));
    EXPECT_FALSE(database().existsCountryCode(model::CountryCode{"US"}));
    database().commitTransaction();
}
