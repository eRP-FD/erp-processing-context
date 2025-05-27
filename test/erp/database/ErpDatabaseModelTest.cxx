/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/database/ErpDatabaseModel.hxx"
#include "shared/database/AccessTokenIdentity.hxx"
#include "test/util/JwtBuilder.hxx"

#include <gtest/gtest.h>

class ErpDatabaseModelTest : public testing::Test
{
};

TEST_F(ErpDatabaseModelTest, AccessToken_getJson)
{
    {
        db_model::AccessTokenIdentity accessTokenId(JwtBuilder::testBuilder().makeJwtArzt());
        rapidjson::Document d;
        d.Parse(accessTokenId.getJson());
        EXPECT_FALSE(d.HasParseError());
        EXPECT_EQ(std::string(d["id"].GetString()), std::string("0123456789"));
        EXPECT_EQ(std::string(d["name"].GetString()), std::string("Institutions- oder Organisations-Bezeichnung"));
        EXPECT_EQ(std::string(d["oid"].GetString()), std::string("1.2.276.0.76.4.30"));
    }

    {
        db_model::AccessTokenIdentity accessTokenId(model::TelematikId(R"(telematik "id")"), R"(a "name")",
                                                    R"(some "oid")");
        rapidjson::Document d;
        d.Parse(accessTokenId.getJson());
        EXPECT_FALSE(d.HasParseError());
        EXPECT_EQ(std::string(d["id"].GetString()), R"(telematik "id")");
        EXPECT_EQ(std::string(d["name"].GetString()), std::string("a \"name\""));
        EXPECT_EQ(std::string(d["oid"].GetString()), std::string("some \"oid\""));
    }
}

TEST_F(ErpDatabaseModelTest, AccessToken_construct)
{
    const db_model::AccessTokenIdentity accessTokenId(
        model::TelematikId("0123456789"), "Institutions- oder Organisations-Bezeichnung", "1.2.276.0.76.4.30");
    EXPECT_EQ(accessTokenId.getId(), std::string("0123456789"));
    EXPECT_EQ(accessTokenId.getName(), std::string("Institutions- oder Organisations-Bezeichnung"));
    EXPECT_EQ(accessTokenId.getOid(), std::string("1.2.276.0.76.4.30"));
}


TEST_F(ErpDatabaseModelTest, AccessToken_from)
{
    const auto accessTokenId = db_model::AccessTokenIdentity::fromJson(
        R"({"id": "0123456789", "name": "Institutions- oder Organisations-Bezeichnung", "oid": "1.2.276.0.76.4.30"})");
    EXPECT_EQ(accessTokenId.getId(), std::string("0123456789"));
    EXPECT_EQ(accessTokenId.getName(), std::string("Institutions- oder Organisations-Bezeichnung"));
    EXPECT_EQ(accessTokenId.getOid(), std::string("1.2.276.0.76.4.30"));
}


TEST_F(ErpDatabaseModelTest, AccessToken_InvalidJson)
{
    const auto expectedString = R"(invalid custom json representation of JWT - Missing a name for object member. at offset 2)";

    try
    {
        db_model::AccessTokenIdentity::fromJson(
            R"(
{{
  "id": "0123456789",
  "name": "Institutions- oder Organisations-Bezeichnung",
  "oid": "1.2.276.0.76.4.30"
}
)");
    }
    catch (const db_model::AccessTokenIdentity::Exception& exc)
    {
        EXPECT_STREQ(exc.what(), expectedString);
    }
}


TEST_F(ErpDatabaseModelTest, AccessToken_InvalidJsons)
{
    // Catch serialized jsons based on following template which was used in the past:
    const auto serializer = [] (const std::string& id, const std::string& name, const std::string& oid)-> std::string {
        return R"({"id": ")" + id + R"(", "name": ")" + name + R"(", "oid": ")" + oid + R"("})";
    };

    const auto dummyToken = db_model::AccessTokenIdentity::fromJson(serializer("1", "2", "3"));
    auto accessTokenId = dummyToken;

    const auto corrupted_json_1 = serializer("0123456789", R"(Institutions- oder " Organisations-Bezeichnung)", "1.2.276.0.76.4.30");
    EXPECT_NO_THROW(accessTokenId = db_model::AccessTokenIdentity::fromJson(corrupted_json_1));
    EXPECT_EQ(accessTokenId.getId(), std::string("0123456789"));
    EXPECT_EQ(accessTokenId.getName(), std::string(R"(Institutions- oder " Organisations-Bezeichnung)"));
    EXPECT_EQ(accessTokenId.getOid(), std::string("1.2.276.0.76.4.30"));

    const auto corrupted_json_2 = serializer("0123456789", R"(Institutions- oder Organisations-Bezeichnung\)", "1.2.276.0.76.4.30");
    EXPECT_NO_THROW(accessTokenId = db_model::AccessTokenIdentity::fromJson(corrupted_json_2));
    EXPECT_EQ(accessTokenId.getId(), std::string("0123456789"));
    EXPECT_EQ(accessTokenId.getName(), std::string(R"(Institutions- oder Organisations-Bezeichnung\)"));
    EXPECT_EQ(accessTokenId.getOid(), std::string("1.2.276.0.76.4.30"));

    const auto corrupted_json_3 = serializer("0123456789", R"(Institutions-\Organisations-Bezeichnung\)", "1.2.276.0.76.4.30");
    EXPECT_NO_THROW(accessTokenId = db_model::AccessTokenIdentity::fromJson(corrupted_json_3));
    EXPECT_EQ(accessTokenId.getId(), std::string("0123456789"));
    EXPECT_EQ(accessTokenId.getName(), std::string(R"(Institutions-\Organisations-Bezeichnung\)"));
    EXPECT_EQ(accessTokenId.getOid(), std::string("1.2.276.0.76.4.30"));

    const auto corrupted_json_4 = serializer("0123456789", R"("Institutions-\Organisations-Bezeichnung\)", "1.x.276.0.76.4.30");
    EXPECT_THROW(accessTokenId = db_model::AccessTokenIdentity::fromJson(corrupted_json_4), db_model::AccessTokenIdentity::Exception);
}
