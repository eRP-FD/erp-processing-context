#include <gtest/gtest.h> // should be first or FRIEND_TEST would not work
#include "erp/tsl/TrustStore.hxx"
#include "erp/model/Timestamp.hxx"
#include "erp/tsl/TslParser.hxx"
#include "erp/util/FileHelper.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"
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
        std::string{TEST_DATA_DIR} + "/tsl/TslSignerCertificateIssuer.der");
    EnvironmentVariableGuard newCaDerPathGuard(
        "ERP_TSL_INITIAL_CA_DER_PATH_NEW",
        std::string{TEST_DATA_DIR} + "/tsl/TslSignerCertificate.der");

    TrustStore trustStore(TslMode::TSL);

    std::vector<X509Certificate> tslCas = trustStore.getTslSignerCas();
    ASSERT_EQ(2, tslCas.size());
    ASSERT_EQ("/C=DE/O=gematik GmbH NOT-VALID/OU=TSL-Signer-CA der Telematikinfrastruktur/CN=GEM.TSL-CA28 TEST-ONLY", tslCas[0].getSubject());
    ASSERT_EQ("/C=DE/O=gematik GmbH NOT-VALID/CN=TSL Signing Unit 10 TEST-ONLY", tslCas[1].getSubject());
}


TEST(TrustStoreTest, TrustStoreTslSignerCaHandlingNewStartFutureIgnored)
{
    EnvironmentVariableGuard mainCaDerPathGuard("ERP_TSL_INITIAL_CA_DER_PATH",
                                                std::string{TEST_DATA_DIR} + "/tsl/TslSignerCertificateIssuer.der");
    EnvironmentVariableGuard newCaDerPathGuard("ERP_TSL_INITIAL_CA_DER_PATH_NEW",
                                               std::string{TEST_DATA_DIR} + "/tsl/TslSignerCertificate.der");
    EnvironmentVariableGuard newCaDerPathGuardStart("ERP_TSL_INITIAL_CA_DER_PATH_NEW_START",
                                                    "2041-01-01T00:00:00Z");

    TrustStore trustStore(TslMode::TSL);

    std::vector<X509Certificate> tslCas = trustStore.getTslSignerCas();
    ASSERT_EQ(1, tslCas.size());
    ASSERT_EQ("/C=DE/O=gematik GmbH NOT-VALID/OU=TSL-Signer-CA der Telematikinfrastruktur/CN=GEM.TSL-CA28 TEST-ONLY",
              tslCas[0].getSubject());
}


TEST(TrustStoreTest, TrustStoreTslSignerCaHandlingNewStartPastSet)
{
    EnvironmentVariableGuard mainCaDerPathGuard(
        "ERP_TSL_INITIAL_CA_DER_PATH",
        std::string{TEST_DATA_DIR} + "/tsl/TslSignerCertificateIssuer.der");
    EnvironmentVariableGuard newCaDerPathGuard(
        "ERP_TSL_INITIAL_CA_DER_PATH_NEW",
        std::string{TEST_DATA_DIR} + "/tsl/TslSignerCertificate.der");
    EnvironmentVariableGuard newCaDerPathGuardStart("ERP_TSL_INITIAL_CA_DER_PATH_NEW_START",
                                                    "2020-01-01T00:00:00Z");

    TrustStore trustStore(TslMode::TSL);

    std::vector<X509Certificate> tslCas = trustStore.getTslSignerCas();
    ASSERT_EQ(2, tslCas.size());
    ASSERT_EQ("/C=DE/O=gematik GmbH NOT-VALID/OU=TSL-Signer-CA der Telematikinfrastruktur/CN=GEM.TSL-CA28 TEST-ONLY", tslCas[0].getSubject());
    ASSERT_EQ("/C=DE/O=gematik GmbH NOT-VALID/CN=TSL Signing Unit 10 TEST-ONLY", tslCas[1].getSubject());
}
