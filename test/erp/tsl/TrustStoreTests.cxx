/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include <gtest/gtest.h>// should be first or FRIEND_TEST would not work
#include "shared/model/Timestamp.hxx"
#include "shared/tsl/TrustStore.hxx"
#include "shared/tsl/TslParser.hxx"
#include "shared/util/FileHelper.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"
#include "test/util/ResourceManager.hxx"
#include "test/util/StaticData.hxx"

#include <memory>
#include <test_config.h>


TEST(TrustStoreTest, TrustStoreInitallyIsEmpty)
{
    std::unique_ptr<TrustStore> trustStore =
        std::make_unique<TrustStore>(TslMode::TSL);

    EXPECT_EQ(nullptr, trustStore->getX509Store(std::nullopt).getStore());
}


TEST(TrustStoreTest, TrustStoreTslSignerCaHandlingNewSet)
{
    EnvironmentVariableGuard mainCaDerPathGuard(
        "ERP_TSL_INITIAL_CA_DER_PATH",
        ResourceManager::getAbsoluteFilename("test/generated_pki/sub_ca1_ec/ca.der"));
    EnvironmentVariableGuard newCaDerPathGuard(
        "ERP_TSL_INITIAL_CA_DER_PATH_NEW",
        ResourceManager::getAbsoluteFilename("test/generated_pki/sub_ca1_ec/certificates/tsl_signer_ec/tsl_signer_ec.der"));

    TrustStore trustStore(TslMode::TSL);

    std::vector<X509Certificate> tslCas = trustStore.getTslSignerCas();
    ASSERT_EQ(2, tslCas.size());
    ASSERT_EQ("/C=DE/ST=Berlin/L=Berlin/O=Example Inc./OU=IT/CN=Example Inc. Sub CA EC 1", tslCas[0].getSubject());
    ASSERT_EQ("/C=DE/ST=Berlin/L=Berlin/O=Example Inc./OU=IT/CN=TSL signer", tslCas[1].getSubject());
}


TEST(TrustStoreTest, TrustStoreTslSignerCaHandlingNewStartFutureIgnored)
{
    EnvironmentVariableGuard mainCaDerPathGuard("ERP_TSL_INITIAL_CA_DER_PATH",
                                                ResourceManager::getAbsoluteFilename("test/generated_pki/sub_ca1_ec/ca.der"));
    EnvironmentVariableGuard newCaDerPathGuard("ERP_TSL_INITIAL_CA_DER_PATH_NEW",
                                               ResourceManager::getAbsoluteFilename("test/generated_pki/sub_ca1_ec/certificates/tsl_signer_ec/tsl_signer_ec.der"));
    EnvironmentVariableGuard newCaDerPathGuardStart("ERP_TSL_INITIAL_CA_DER_PATH_NEW_START",
                                                    "2041-01-01T00:00:00Z");

    TrustStore trustStore(TslMode::TSL);

    std::vector<X509Certificate> tslCas = trustStore.getTslSignerCas();
    ASSERT_EQ(1, tslCas.size());
    ASSERT_EQ("/C=DE/ST=Berlin/L=Berlin/O=Example Inc./OU=IT/CN=Example Inc. Sub CA EC 1",
              tslCas[0].getSubject());
}


TEST(TrustStoreTest, TrustStoreTslSignerCaHandlingNewStartPastSet)
{
    EnvironmentVariableGuard mainCaDerPathGuard(
        "ERP_TSL_INITIAL_CA_DER_PATH",
        ResourceManager::getAbsoluteFilename("test/generated_pki/sub_ca1_ec/ca.der"));
    EnvironmentVariableGuard newCaDerPathGuard(
        "ERP_TSL_INITIAL_CA_DER_PATH_NEW",
        ResourceManager::getAbsoluteFilename("test/generated_pki/sub_ca1_ec/certificates/tsl_signer_ec/tsl_signer_ec.der"));
    EnvironmentVariableGuard newCaDerPathGuardStart("ERP_TSL_INITIAL_CA_DER_PATH_NEW_START",
                                                    "2020-01-01T00:00:00Z");

    TrustStore trustStore(TslMode::TSL);

    std::vector<X509Certificate> tslCas = trustStore.getTslSignerCas();
    ASSERT_EQ(2, tslCas.size());
    ASSERT_EQ("/C=DE/ST=Berlin/L=Berlin/O=Example Inc./OU=IT/CN=Example Inc. Sub CA EC 1", tslCas[0].getSubject());
    ASSERT_EQ("/C=DE/ST=Berlin/L=Berlin/O=Example Inc./OU=IT/CN=TSL signer", tslCas[1].getSubject());
}
