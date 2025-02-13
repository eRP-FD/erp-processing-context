/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/TelematikLookup.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/FileHelper.hxx"
#include "test/util/ResourceManager.hxx"

#include <gtest/gtest.h>
#include <fstream>
#include <sstream>


class TelematikLookupTest : public ::testing::Test
{
};

TEST_F(TelematikLookupTest, construct)
{
    {// construct empty
        std::stringstream ss;
        TelematikLookup tl(ss);
        EXPECT_EQ(tl.size(), 0);
    }

    {// construct one line
        std::stringstream ss;
        ss << "1;3-SMC-B-Testkarte-883110000120312";
        TelematikLookup tl(ss);
        EXPECT_TRUE(tl.hasSerialNumber("1"));
        EXPECT_FALSE(tl.hasSerialNumber("2"));
        EXPECT_FALSE(tl.hasSerialNumber("3"));
        EXPECT_FALSE(tl.hasSerialNumber("4"));

        EXPECT_EQ(tl.serialNumber2TelematikId("1"), "3-SMC-B-Testkarte-883110000120312");
        EXPECT_EQ(tl.size(), 1);
    }
    {// construct two lines
        std::stringstream ss;
        ss << "1;3-SMC-B-Testkarte-883110000120312\n"
           << "3;3-SMC-B-Testkarte-883110000120313";
        TelematikLookup tl(ss);
        EXPECT_TRUE(tl.hasSerialNumber("1"));
        EXPECT_FALSE(tl.hasSerialNumber("2"));
        EXPECT_TRUE(tl.hasSerialNumber("3"));
        EXPECT_FALSE(tl.hasSerialNumber("4"));

        EXPECT_EQ(tl.serialNumber2TelematikId("1"), "3-SMC-B-Testkarte-883110000120312");
        EXPECT_EQ(tl.serialNumber2TelematikId("3"), "3-SMC-B-Testkarte-883110000120313");
        EXPECT_EQ(tl.size(), 2);
    }
    {// construct with \r\n\ lines
        std::stringstream ss;
        ss << "1;3-SMC-B-Testkarte-883110000120312\r\n"
           << "3;3-SMC-B-Testkarte-883110000120313";
        TelematikLookup tl(ss);
        EXPECT_TRUE(tl.hasSerialNumber("1"));
        EXPECT_FALSE(tl.hasSerialNumber("2"));
        EXPECT_TRUE(tl.hasSerialNumber("3"));
        EXPECT_FALSE(tl.hasSerialNumber("4"));

        EXPECT_EQ(tl.serialNumber2TelematikId("1"), "3-SMC-B-Testkarte-883110000120312");
        EXPECT_EQ(tl.serialNumber2TelematikId("3"), "3-SMC-B-Testkarte-883110000120313");
        EXPECT_EQ(tl.size(), 2);
    }
}


TEST_F(TelematikLookupTest, construct_fromFile)
{
    {
        TelematikLookup tl =
            TelematikLookup::fromFile(ResourceManager::getAbsoluteFilename("test/telematiklookup/two_entries.csv"));
        EXPECT_TRUE(tl.hasSerialNumber("a"));
        EXPECT_FALSE(tl.hasSerialNumber("3-SMC-B-Testkarte-883110000120312"));
        EXPECT_TRUE(tl.hasSerialNumber("c"));
        EXPECT_FALSE(tl.hasSerialNumber("3-SMC-B-Testkarte-883110000120313"));

        EXPECT_EQ(tl.serialNumber2TelematikId("a"), "3-SMC-B-Testkarte-883110000120312");
        EXPECT_EQ(tl.serialNumber2TelematikId("c"), "3-SMC-B-Testkarte-883110000120313");
    }

    {
        EXPECT_THROW(TelematikLookup::fromFile("/not-a-file.csv"), std::exception);
    }
}

TEST_F(TelematikLookupTest, construct_fromConfiguredFile)
{
    const auto ser2tidFile = Configuration::instance().getStringValue(ConfigurationKey::MEDICATION_EXPORTER_SERNO2TID_PATH);
    const auto ser2tidContent = FileHelper::readFileAsString(ser2tidFile);

    std::stringstream ser2tidStream(ser2tidContent);
    EXPECT_NO_THROW(TelematikLookup{ser2tidStream});
}
