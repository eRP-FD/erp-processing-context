/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/util/ByteHelper.hxx"

#include <gtest/gtest.h>
#include <boost/algorithm/string.hpp>
#include <sstream>


TEST(ByteHelperTest, fromAndToHex)
{
    std::ostringstream binStrm;
    std::ostringstream hexStrm;
    for(unsigned int i = 0; i < 512; ++i)
    {
        binStrm << static_cast<unsigned char>(i);
        hexStrm << std::setfill('0') << std::setw(2) << std::hex << static_cast<unsigned int>(static_cast<unsigned char>(i));
    }
    std::string testHexString = ByteHelper::toHex(binStrm.str());
    // compare with expected result:
    ASSERT_EQ(testHexString, hexStrm.str());

    // convert back to binary and compare with original:
    ASSERT_EQ(ByteHelper::fromHex(testHexString), binStrm.str());

    // same check with upper case hex string:
    testHexString = boost::to_upper_copy(testHexString);
    ASSERT_EQ(ByteHelper::fromHex(testHexString), binStrm.str());
}


TEST(ByteHelperTest, fromHex_error)//NOLINT(readability-function-cognitive-complexity)
{
    // check error: odd number of hex characters:
    std::ostringstream hexStrm;
    for(unsigned int i = 0; i < 32; ++i)
    {
        hexStrm << std::hex << i % 16;
        if((i + 1) % 2)
            ASSERT_ANY_THROW(ByteHelper::fromHex(hexStrm.str()));
        else
            ASSERT_NO_THROW(ByteHelper::fromHex(hexStrm.str()));
    }

    // check error: invalid hex character value:
    for(unsigned char c = 0; c < 255; ++c) {
        std::string errString = hexStrm.str() + '0';
        errString += static_cast<char>(c);
        errString += hexStrm.str();
        if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))
            ASSERT_NO_THROW(ByteHelper::fromHex(errString));
        else
            ASSERT_ANY_THROW(ByteHelper::fromHex(errString));
    }
}
