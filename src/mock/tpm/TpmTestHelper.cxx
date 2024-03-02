/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "TpmTestHelper.hxx"
#if !defined __APPLE__ && !defined _WIN32
#include "erp/tpm/TpmProduction.hxx"
#endif
#include "erp/hsm/BlobCache.hxx"
#include "erp/util/Base64.hxx"
#include "erp/util/Expect.hxx"
#include "mock/tpm/TpmMock.hxx"
#include "mock/util/MockConfiguration.hxx"


#if WITH_HSM_TPM_PRODUCTION > 0
#define TPM_SUPPORTED
#else
#undef TPM_SUPPORTED
#endif

#ifdef TPM_SUPPORTED
    #include <tpmclient/Client.h>
    #include <tpmclient/Session.h>
#endif


class MockTpmTestHelper : public TpmTestHelper
{
public:
    Tpm::AuthenticateCredentialInput getAuthenticateCredentialInput (BlobCache& blobCache) override;
};


class ProductionTpmTestHelper : public TpmTestHelper
{
public:
    Tpm::AuthenticateCredentialInput getAuthenticateCredentialInput (BlobCache& blobCache) override;
};


bool TpmTestHelper::isProductionTpmSupportedAndConfigured (void)
{
    const static bool useProductionTpm =
#ifdef TPM_SUPPORTED
        ! MockConfiguration::instance().getOptionalBoolValue(MockConfigurationKey::MOCK_USE_MOCK_TPM, false)
#else
        false
#endif
        ;
    return useProductionTpm;
}


Tpm::Factory TpmTestHelper::createTpmFactory (void)
{
    return []
    (BlobCache& blobCache)
    {
        std::unique_ptr<Tpm> tpm;
#ifdef TPM_SUPPORTED
        TVLOG(1) << "creating production TPM";
        tpm = TpmProduction::createInstance(blobCache);
#else
        (void)blobCache;
#endif
        if (tpm == nullptr)
        {
            TVLOG(1) << "falling back to mock TPM";
            tpm = createMockTpm();
        }
        return tpm;
    };
}


std::unique_ptr<Tpm> TpmTestHelper::createMockTpm (void)
{
    return std::make_unique<TpmMock>();
}


Tpm::Factory TpmTestHelper::createMockTpmFactory (void)
{
    return []
    (BlobCache&)
    {
        return createMockTpm();
    };
}


std::unique_ptr<Tpm> TpmTestHelper::createProductionTpm (BlobCache& blobCache)
{
#ifdef TPM_SUPPORTED
    TVLOG(0) << "TPM support is present";
    if (isProductionTpmSupportedAndConfigured())
    {
        TVLOG(0) << "TPM mock is disabled, connecting to simulated or hardware TPM...";
        return TpmProduction::createInstance(blobCache);
    }
    else
    {
        TVLOG(0) << "use of production TPM is configured off";
        return {};
    }
#endif
    (void) blobCache;
    TVLOG(0) << "TPM support is not present";
    return {};
}


Tpm::Factory TpmTestHelper::createProductionTpmFactory (void)
{
    return []
    (BlobCache& blobCache)
    {
        return createProductionTpm(blobCache);
    };
}


std::unique_ptr<TpmTestHelper> TpmTestHelper::createTestHelperForProduction (void)
{
    if (isProductionTpmSupportedAndConfigured())
        return std::make_unique<ProductionTpmTestHelper>();
    else
        return {};
}


std::unique_ptr<TpmTestHelper> TpmTestHelper::createTestHelperForMock (void)
{
    return std::make_unique<MockTpmTestHelper>();
}


Tpm::AuthenticateCredentialInput MockTpmTestHelper::getAuthenticateCredentialInput (BlobCache&)
{
    Tpm::AuthenticateCredentialInput input;
    input.secretBase64 = "ACB/cZ480vmNR4yRWuRc7ccE2QNne6AmqFZkX+s4N7QVxgAggk/vtgu6kUzmV7fe0TZJ1OA1YW+fd9kAjTDr36ePIiw=";
    input.credentialBase64 = "ACBvwkNP1kGUdMSiFS3NhYFN1uwmZ629eO+YwQkhZp+5Acv6U82XhnLHb+1ZQLlYc/n7iLJmn2dsGAUZeOtoHNwuUZA=";
    input.akNameBase64 = "AAv+egMWhqowY/WSKilTdvXUCQsXOmjta7Z0Qzjv+VY6Xg==";
    return input;
}


Tpm::AuthenticateCredentialInput ProductionTpmTestHelper::getAuthenticateCredentialInput (BlobCache& blobCache)
{
#ifdef TPM_SUPPORTED
    auto session = std::make_shared<tpmclient::Session>();
    session->open();
    auto client = tpmclient::Client(session);

    const auto keyPairBlobEntry = blobCache.getBlob(BlobType::AttestationKeyPair);
    const char* p = keyPairBlobEntry.blob.data;
    tpmclient::KeyPairBlob akKeyPairBlob (p, p+keyPairBlobEntry.blob.data.size());

    auto ak = client.getAk(akKeyPairBlob);
    (void)client.getEk(); // This has to be called for ... reasons.

    const auto madeCredential = client.makeCredential(akKeyPairBlob);
    Tpm::AuthenticateCredentialInput input;
    input.akNameBase64 = Base64::encode(util::rawToBuffer(ak.name.data(), ak.name.size()));
    input.secretBase64 = Base64::encode(util::rawToBuffer(madeCredential.secret.data(), madeCredential.secret.size()));
    input.credentialBase64 = Base64::encode(madeCredential.encryptedCredential);

    return input;
#else
    (void) blobCache;
    Fail("not supported on this platform");
#endif
}
