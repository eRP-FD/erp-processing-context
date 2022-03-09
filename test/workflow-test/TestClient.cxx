/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "TestClient.hxx"

#include "erp/crypto/Certificate.hxx"
#include "erp/util/Configuration.hxx"
#include "mock/crypto/MockCryptography.hxx"


TestClient::Factory TestClient::mFactory;

void TestClient::setFactory(TestClient::Factory&& factory)
{
    mFactory = std::move(factory);
}

std::unique_ptr<TestClient> TestClient::create(std::shared_ptr<XmlValidator> xmlValidator)
{
    return mFactory(std::move(xmlValidator));
}

Certificate TestClient::getEciesCertificate (void)
{
    const auto pemString = Configuration::instance().getOptionalStringValue(ConfigurationKey::ECIES_CERTIFICATE, "");
    if (pemString.empty())
        return Certificate::build().withPublicKey(MockCryptography::getEciesPublicKey()).build();
    else
        return Certificate::fromPem(pemString);
}
