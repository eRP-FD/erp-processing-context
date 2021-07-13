#include "erp/model/MetaData.hxx"
#include "erp/erp-serverinfo.hxx"
#include "erp/fhir/Fhir.hxx"

#include "test/util/StaticData.hxx"

#include <gtest/gtest.h>


TEST(MetaDataTest, Construct)
{
    model::MetaData metaData;

    EXPECT_EQ(metaData.version(), ErpServerInfo::ReleaseVersion);
    EXPECT_EQ(metaData.date(), model::Timestamp::fromXsDateTime(ErpServerInfo::ReleaseDate));
    EXPECT_EQ(metaData.releaseDate(), model::Timestamp::fromXsDateTime(ErpServerInfo::ReleaseDate));

    ASSERT_NO_THROW(StaticData::getJsonValidator()->validate(
       model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(metaData.jsonDocument()), SchemaType::fhir));

    const auto* version = "1.1.012a";
    const model::Timestamp releaseDate = model::Timestamp::now();
    EXPECT_NO_THROW(metaData.setVersion(version));
    EXPECT_NO_THROW(metaData.setDate(releaseDate));
    EXPECT_NO_THROW(metaData.setReleaseDate(releaseDate));
    EXPECT_EQ(metaData.version(), version);
    EXPECT_EQ(metaData.releaseDate(), releaseDate);
    EXPECT_EQ(metaData.date(), releaseDate);
}

