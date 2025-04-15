/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "TestClient.hxx"
#include "shared/crypto/Certificate.hxx"
#include "shared/util/Expect.hxx"
#include "test/util/TestConfiguration.hxx"
#include "mock/crypto/MockCryptography.hxx"


TestClient::Factory TestClient::mFactory;

void TestClient::setFactory(TestClient::Factory&& factory)
{
    mFactory = std::move(factory);
}

std::unique_ptr<TestClient> TestClient::create(std::shared_ptr<XmlValidator> xmlValidator, Target target)
{
    return mFactory(std::move(xmlValidator), target);
}

Certificate TestClient::getEciesCertificate(void)
{
    const auto pemString = TestConfiguration::instance().getOptionalStringValue(TestConfigurationKey::TEST_ECIES_CERTIFICATE, "");
    if (pemString.empty())
        return Certificate::createSelfSignedCertificateMock(MockCryptography::getEciesPrivateKey());
    else
        return Certificate::fromPem(pemString);
}

PcServiceContext* TestClient::getContext() const
{
    return nullptr;
}

std::string to_string(TestClient::Target target)
{
    using namespace std::string_literals;
    switch (target)
    {
        case TestClient::Target::ADMIN:
            return "ADMIN"s;
        case TestClient::Target::VAU:
            return "VAU"s;
        case TestClient::Target::ENROLMENT:
            return "ENROLMENT"s;
    }
    Fail2("invalid value for Target: " + std::to_string(static_cast<intmax_t>(target)), std::logic_error);
}

std::ostream& operator<<(std::ostream& out, TestClient::Target target)
{
    return out << to_string(target);
}
