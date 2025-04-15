/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/model/Timestamp.hxx"
#include "shared/tsl/TslParser.hxx"
#include "shared/tsl/X509Certificate.hxx"
#include "shared/tsl/error/TslError.hxx"
#include "shared/util/FileHelper.hxx"
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
        ResourceManager::instance().getStringResource("test/generated_pki/tsl/TSL_parserTest.xml"),
        TslMode::TSL,
        *StaticData::getXmlValidator()};

    EXPECT_EQ(tslParser.getId(), TslParsingExpectations::expectedId);
    EXPECT_EQ(tslParser.getSequenceNumber(), TslParsingExpectations::expectedSequenceNumber);
    EXPECT_EQ(tslParser.getNextUpdate(), TslParsingExpectations::expectedNextUpdate);
    EXPECT_EQ(tslParser.getSignerCertificate(), TslParsingExpectations::getExpectedSignerCertificate());
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
        ResourceManager::instance().getStringResource("test/generated_pki/tsl/TSL_outdated.xml"),
        TslMode::TSL,
        *StaticData::getXmlValidator()};

    EXPECT_EQ(tslParser.getId(), "ID31028220210914152510Z");
    EXPECT_EQ(tslParser.getSequenceNumber(), "10281");
}


TEST_F(TslParsingTests, WansimBnaXmlIsParsedCorrectly)
{
    // Just use an outdated TSL.xml to test parsing
    TslParser tslParser{
        ResourceManager::instance().getStringResource("test/generated_pki/tsl/BNA_EC_valid.xml"),
        TslMode::BNA,
        *StaticData::getXmlValidator()};

    EXPECT_EQ(tslParser.getId(), "ID31028220210914152511Z");
    EXPECT_EQ(tslParser.getSequenceNumber(), "10282");
}


TEST_F(TslParsingTests, BnaXmlRsa512)
{
    // Just use an outdated TSL.xml to test parsing
    TslParser tslParser{
        ResourceManager::instance().getStringResource("test/generated_pki/tsl/BNA_RSA_valid_sha512.xml"),
        TslMode::BNA,
        *StaticData::getXmlValidator()};

    EXPECT_EQ(tslParser.getId(), "ID31028220210914152511Z");
    EXPECT_EQ(tslParser.getSequenceNumber(), "10282");
}

TEST_F(TslParsingTests, BnaXmlRsa256)
{
    // Just use an outdated TSL.xml to test parsing
    TslParser tslParser{
        ResourceManager::instance().getStringResource("test/generated_pki/tsl/BNA_RSA_valid_sha256.xml"),
        TslMode::BNA,
        *StaticData::getXmlValidator()};

    EXPECT_EQ(tslParser.getId(), "ID31028220210914152511Z");
    EXPECT_EQ(tslParser.getSequenceNumber(), "10282");
}
