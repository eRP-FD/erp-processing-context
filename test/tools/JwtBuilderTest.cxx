/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include <gtest/gtest.h>// should be first or FRIEND_TEST would not work

#include "test/util/JwtBuilder.hxx"
#include "erp/idp/Idp.hxx"
#include "erp/idp/IdpUpdater.hxx"
#include "erp/pc/ProfessionOid.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/util/Environment.hxx"
#include "test/erp/tsl/TslTestHelper.hxx"

#include "test/util/ResourceManager.hxx"


TEST(JwtBuilderTest, testBuilder)//NOLINT(readability-function-cognitive-complexity)
{
    auto testBuilder = JwtBuilder::testBuilder();
    Idp idp;
    auto tslEnvironmentGuard = std::make_unique<EnvironmentVariableGuard>(
        "ERP_TSL_INITIAL_CA_DER_PATH",
        ResourceManager::getAbsoluteFilename("test/generated_pki/sub_ca1_ec/ca.der"));

    auto idpCertificate = Certificate::fromPem(
        FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/IDP-Wansim.pem"));
    auto idpCertificateCa = Certificate::fromPem(
        FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/IDP-Wansim-CA.pem"));

    auto tslManager = TslTestHelper::createTslManager<TslManager>(
        {},
        {},
        {
            {"http://ocsp-testref.tsl.telematik-test/ocsp",
             {{TslTestHelper::getTslSignerCertificate(), TslTestHelper::getTslSignerCACertificate(), MockOcsp::CertificateOcspTestMode::SUCCESS},
                 {idpCertificate, idpCertificateCa, MockOcsp::CertificateOcspTestMode::SUCCESS}}}}
        );

    std::weak_ptr<TslManager> mgrWeakPtr{tslManager};
    tslManager->addPostUpdateHook([mgrWeakPtr, idpCertificateCa]{
        auto tslManager = mgrWeakPtr.lock();
        if (! tslManager)
            return;
        TslTestHelper::addCaCertificateToTrustStore(idpCertificateCa, *tslManager, TslMode::TSL);
    });

    std::string idpResponseJson = FileHelper::readFileAsString(
        std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/idpResponse.json");
    idpResponseJson = std::regex_replace(idpResponseJson, std::regex{"###CERTIFICATE##"},
                                         idpCertificate.toBase64Der());

    const std::string idpResponseJwk = FileHelper::readFileAsString(
        std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/idpResponseJwk.txt");
    std::shared_ptr<UrlRequestSenderMock> idpRequestSender = std::make_shared<UrlRequestSenderMock>(
        std::unordered_map<std::string, std::string>{
            {"https://idp.lu.erezepttest.net:443/certs/puk_idp_sig.json", idpResponseJson},
            {"https://idp.lu.erezepttest.net:443/.well-known/openid-configuration", idpResponseJwk}});

    boost::asio::io_context io;
    auto updater = IdpUpdater::create(
        idp,
        *tslManager,
        io,
        true,
        idpRequestSender);

    auto publicKey = idp.getCertificate().getPublicKey();
    EXPECT_NO_THROW(testBuilder.makeJwtApotheke().verify(publicKey));
    EXPECT_NO_THROW(testBuilder.makeJwtArzt().verify(publicKey));
    EXPECT_NO_THROW(testBuilder.makeJwtVersicherter("X987654326").verify(publicKey));
}


TEST(JwtBuilderTest, OIDs)
{
    auto testBuilder = JwtBuilder::testBuilder();
    auto jwtApotheke = testBuilder.makeJwtApotheke();
    auto jwtArzt = testBuilder.makeJwtArzt();
    auto jwtVersicherter = testBuilder.makeJwtVersicherter("X987654326");
    // clang-format off
    EXPECT_EQ(jwtApotheke    .stringForClaim(JWT::professionOIDClaim), profession_oid::oid_oeffentliche_apotheke);
    EXPECT_EQ(jwtArzt        .stringForClaim(JWT::professionOIDClaim), profession_oid::oid_arzt                 );
    EXPECT_EQ(jwtVersicherter.stringForClaim(JWT::professionOIDClaim), profession_oid::oid_versicherter         );
    // clang-format on
}


TEST(JwtBuilderTest, audClaimOverride)
{
    auto testBuilder = JwtBuilder::testBuilder();

    const std::string registeredFdUri = Configuration::instance().getOptionalStringValue(ConfigurationKey::IDP_REGISTERED_FD_URI, "");

    // Value from configuration file is used.
    auto jwtApotheke = testBuilder.makeJwtApotheke();
    EXPECT_EQ(jwtApotheke.stringForClaim(JWT::audClaim), registeredFdUri);

    // Value from env var is used.
    Environment::set("ERP_IDP_REGISTERED_FD_URI", "12345");
    jwtApotheke = testBuilder.makeJwtApotheke();
    EXPECT_EQ(jwtApotheke.stringForClaim(JWT::audClaim), "12345");
    Environment::unset("ERP_IDP_REGISTERED_FD_URI");
}
