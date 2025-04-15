/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/util/Configuration.hxx"
#include "shared/util/UrlHelper.hxx"
#include "exporter/EpaAccountLookup.hxx"

#include <gtest/gtest.h>

class EpaAccountLookupIt : public testing::Test
{

};

TEST_F(EpaAccountLookupIt, DISABLED_useMock)
{
    model::Kvnr kvnr{"X123456788", model::Kvnr::Type::gkv};
    /*auto result = EpaAccountLookup{TlsCertificateVerifier::withVerificationDisabledForTesting()}.lookup(kvnr);
    EXPECT_EQ(result.lookupResult, EpaAccount::Code::allowed);
    EXPECT_EQ(result.kvnr, kvnr);
    auto cfg = Configuration::instance().getArray(ConfigurationKey::MEDICATION_EXPORTER_EPA_ACCOUNT_LOOKUP_EPA_AS_FQDN);
    auto urlParts = UrlHelper::parseUrl(cfg[0]);
    EXPECT_EQ(result.host, urlParts.mHost);
    EXPECT_EQ(result.port, urlParts.mPort);*/
}
