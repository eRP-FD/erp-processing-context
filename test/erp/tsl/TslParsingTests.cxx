/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/model/Timestamp.hxx"
#include "erp/tsl/TslParser.hxx"
#include "erp/tsl/X509Certificate.hxx"
#include "erp/tsl/error/TslError.hxx"
#include "erp/util/FileHelper.hxx"
#include "test/erp/tsl/TslParsingExpectations.hxx"
#include "test/erp/tsl/TslTestHelper.hxx"
#include "test/util/ResourceManager.hxx"
#include "test/util/StaticData.hxx"

#include <gtest/gtest.h>
#include <test_config.h>
#include <chrono>


class TslParsingTests : public testing::Test
{};


TEST_F(TslParsingTests, EmptyXmlIsNotWellformed)//NOLINT(readability-function-cognitive-complexity)
{
    EXPECT_TSL_ERROR_THROW(TslParser("", TslMode::TSL, *StaticData::getXmlValidator()),
                           {TslErrorCode::TSL_NOT_WELLFORMED},
                           HttpStatus::InternalServerError);
}


TEST_F(TslParsingTests, XmlWithEmptyRootIsWellformedButNotValid)//NOLINT(readability-function-cognitive-complexity)
{
    EXPECT_TSL_ERROR_THROW(TslParser("<tag></tag>", TslMode::TSL, *StaticData::getXmlValidator()),
                           {TslErrorCode::TSL_SCHEMA_NOT_VALID},
                           HttpStatus::InternalServerError);
}


TEST_F(TslParsingTests, TslXmlFromTestDataIsValidButManipulated)//NOLINT(readability-function-cognitive-complexity)
{
    std::string manipulatedTsl =
        String::replaceAll(
            ResourceManager::instance().getStringResource("test/generated_pki/tsl/TSL_valid.xml"),
            "<StreetAddress>Vattmannstrasse  1</StreetAddress>",
            "<StreetAddress>Vattmannstrasse  1a</StreetAddress>");
    EXPECT_TSL_ERROR_THROW(TslParser(
                               manipulatedTsl,
                               TslMode::TSL,
                               *StaticData::getXmlValidator()),
                           {TslErrorCode::XML_SIGNATURE_ERROR},
                           HttpStatus::InternalServerError);
}


TEST_F(TslParsingTests, TslXmlWithMultipleNewTslCAIsParsedCorrectly)//NOLINT(readability-function-cognitive-complexity)
{
    EXPECT_NO_THROW(TslParser(
                        ResourceManager::instance().getStringResource("test/generated_pki/tsl/TSL_multiple_new_cas.xml"),
                        TslMode::TSL,
                        *StaticData::getXmlValidator()));
}


TEST_F(TslParsingTests, TslXmlWithBrokenNewTslCAIsParsedCorrectly)//NOLINT(readability-function-cognitive-complexity)
{
    EXPECT_NO_THROW(TslParser(
                        ResourceManager::instance().getStringResource("test/generated_pki/tsl/TSL_broken_new_ca.xml"),
                        TslMode::TSL,
                        *StaticData::getXmlValidator()));
}


bool operator==(const TslParser::ServiceInformation& first, const TslParser::ServiceInformation& second)
{
    return (first.certificate == second.certificate
            && first.serviceSupplyPointList == second.serviceSupplyPointList && first.serviceIdentifier == second.serviceIdentifier);
}


TEST_F(TslParsingTests, TslXmlIsCompletelyValidAndParsedCorrectly)//NOLINT(readability-function-cognitive-complexity)
{
    // Just use an outdated TSL.xml to test parsing
    TslParser tslParser{
        FileHelper::readFileAsString(std::string{TEST_DATA_DIR} + "/tsl/TSL_parserTest.xml"),
        TslMode::TSL,
        *StaticData::getXmlValidator()};

    EXPECT_EQ(tslParser.getId(), TslParsingExpectations::expectedId);
    EXPECT_EQ(tslParser.getSequenceNumber(), TslParsingExpectations::expectedSequenceNumber);
    EXPECT_EQ(tslParser.getNextUpdate(), TslParsingExpectations::expectedNextUpdate);
    EXPECT_EQ(tslParser.getSignerCertificate(), TslParsingExpectations::expectedSignerCertificate);
    EXPECT_EQ(tslParser.getOcspCertificateList(), TslParsingExpectations::expectedOcspCertificateList);

    for (const auto& [key, value] : tslParser.getServiceInformationMap())
    {
        const auto iterator = TslParsingExpectations::expectedServiceInformationMap.find(key);
        ASSERT_TRUE(iterator != TslParsingExpectations::expectedServiceInformationMap.end())
            << "unexpected certificate: [" << key.toString() << "]";

        EXPECT_EQ(value.serviceIdentifier, iterator->second.serviceIdentifier)
            << "unexpected service identifier for cerificate: [" << key.toString() << "]";
        EXPECT_EQ(value.serviceSupplyPointList, iterator->second.serviceSupplyPointList)
            << "unexpected service supply point list for cerificate: [" << key.toString() << "]";
        EXPECT_EQ(value.certificate, iterator->second.certificate)
            << "unexpected service certificate for cerificate: [" << key.toString() << "]";
        EXPECT_EQ(value.serviceAcceptanceHistory, iterator->second.serviceAcceptanceHistory)
                        << "unexpected acceptance history for certificate: [" << key.toString() << "]";
    }

    for (const auto& [key, value] : TslParsingExpectations::expectedServiceInformationMap)
    {
        const auto iterator = tslParser.getServiceInformationMap().find(key);
        EXPECT_TRUE(iterator != tslParser.getServiceInformationMap().end())
                        << "missing certificate: [" << key.toString() << "]";
    }

    EXPECT_EQ(tslParser.getServiceInformationMap(), TslParsingExpectations::expectedServiceInformationMap);
}


TEST_F(TslParsingTests, WansimTslXmlIsParsedCorrectly)
{
    // Just use an outdated TSL.xml to test parsing
    TslParser tslParser{
        FileHelper::readFileAsString(std::string{TEST_DATA_DIR} + "/tsl/TSL_parserWansimTest.xml"),
        TslMode::TSL,
        *StaticData::getXmlValidator()};

    EXPECT_EQ(tslParser.getId(), "ID31008220220413120005Z");
    EXPECT_EQ(tslParser.getSequenceNumber(), "10082");
    EXPECT_EQ(tslParser.getNextUpdate(),
              std::chrono::system_clock::time_point{
                  model::Timestamp::fromFhirDateTime("2022-05-13T00:00:05Z").toChronoTimePoint()});
}


TEST_F(TslParsingTests, WansimBnaXmlIsParsedCorrectly)
{
    // Just use an outdated TSL.xml to test parsing
    TslParser tslParser{
        FileHelper::readFileAsString(std::string{TEST_DATA_DIR} + "/tsl/BNA_parserWansimTest.xml"),
        TslMode::BNA,
        *StaticData::getXmlValidator()};

    EXPECT_EQ(tslParser.getId(), "ID52920210525044050Z");
    EXPECT_EQ(tslParser.getSequenceNumber(), "29");
    EXPECT_EQ(tslParser.getNextUpdate(),
              std::chrono::system_clock::time_point{
                  model::Timestamp::fromFhirDateTime("2022-05-25T16:40:50Z").toChronoTimePoint()});
}
