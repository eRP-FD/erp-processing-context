/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/crypto/DtbpPseudonymization.hxx"
#include "shared/util/Base64.hxx"
#include "shared/util/Buffer.hxx"
#include "shared/util/ByteHelper.hxx"
#include "shared/util/Random.hxx"
#include "shared/hsm/ErpTypes.hxx"

#include <gtest/gtest.h>


TEST(DtbpPseudonymizationTest, roundTrip)
{
    const auto key = SafeString{Random::randomBinaryData(Aes128Length)};

    const std::string_view input("hello world");

    auto pseudonymized = DtbpPseudonymization::encryptLogData(input, key);
    auto cleartext = DtbpPseudonymization::decryptLogData(pseudonymized, key);
    ASSERT_EQ(input, cleartext);
}

TEST(DtbpPseudonymizationTest, fail_wrongKeyLength)
{
    const auto key = SafeString{Random::randomBinaryData(Aes128Length+1)};

    const std::string_view input("hello world");
    ASSERT_ANY_THROW(DtbpPseudonymization::encryptLogData(input, key));
}
