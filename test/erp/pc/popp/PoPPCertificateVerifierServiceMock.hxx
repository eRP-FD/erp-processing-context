/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once
#include "erp/pc/popp/PoPPCertificateVerifierService.hxx"
#include "shared/crypto/EllipticCurveUtils.hxx"
#include "shared/util/SafeString.hxx"
#include "test/util/ResourceManager.hxx"

#undef Expect
#include <gmock/gmock.h>

class PoPPCertificateVerifierServiceMock : public IPoPPCertificateVerifierService
{
public:
    MOCK_METHOD(void, startConfigured, (), (const, override));
    MOCK_METHOD(std::optional<PoPPCertificateVerifier::EntityStatement>, getEntityStatement, (), (const, override));
    MOCK_METHOD(std::optional<PoPPCertificateVerifier::PoPPCertificateCache::Key>, getKey, (const std::string& kid),
                (const, override));
    MOCK_METHOD(std::list<model::PoPPCertificateHealthData>, getHealthData, (), (const, override));
    MOCK_METHOD(void, healthCheck, (), (const, override));
};

inline void setupDefaultMock(PoPPCertificateVerifierServiceMock& poPpCertificateVerifierServiceMock)
{
    //ON_CALL(poPpCertificateVerifierServiceMock, startConfigured()).WillByDefault();

    const std::string entityStatement{
        "eyJraWQiOiJsVVpqZHhWRHpzM1hxdjBjQ2dTd1Ntc3lETnFUWThicE5CVFUxZ2Vjcl9ZIiwidHlwIjoiSldUIiwiYWxnIjoiRVMyNTYifQ."
        "eyJpc3MiOiJodHRwczovL2VycC1tb2NrLWlkcDo4NDQzIiwic3ViIjoiaHR0cHM6Ly9lcnAtbW9jay1pZHA6ODQ0MyIsImlhdCI6MTc3MjU0MT"
        "ExNiwiZXhwIjoxNzcyNjI3NTE2LCJhdXRob3JpdHlfaGludHMiOlsiaHR0cHM6Ly9hcHAtcmVmLmZlZGVyYXRpb25tYXN0ZXIuZGUiXSwibWV0"
        "YWRhdGEiOnsiZmVkZXJhdGlvbl9lbnRpdHkiOnsib3JnYW5pemF0aW9uX25hbWUiOiJlcnAtbW9jay1pZHAgUG9QUCBTZXJ2ZXIifSwib2F1dG"
        "hfcmVzb3VyY2UiOnsic2lnbmVkX2p3a3NfdXJpIjoiaHR0cHM6Ly9lcnAtbW9jay1pZHA6ODQ0My9wb3BwL2p3a3MuanNvbiJ9fSwiandrcyI6"
        "eyJrZXlzIjpbeyJrdHkiOiJFQyIsInVzZSI6InNpZyIsImNydiI6IlAtMjU2Iiwia2lkIjoibFVaamR4VkR6czNYcXYwY0NnU3dTbXN5RE5xVF"
        "k4YnBOQlRVMWdlY3JfWSIsIngiOiIyQXdkaktXVS1WazZxdzZSOHM2Q1psQ3RLVDZxc0ZIYUVnNC1aMEMxbmVnIiwieSI6Im0xN3VNcmhJdlJ3"
        "ZEVhUnZwME9hajBWOXl0dWotbkNPeW9fUjhpSlJfOXciLCJhbGciOiJFUzI1NiJ9XX19.SxCvy-"
        "powK8oTUJxq2X4yQr8XMpHgvHataDHdTmbZo5PA1EjdB0DwM71IZW7Giqmr8tA3-vPAjurTeh1RB2mCg"};
    const PoPPCertificateVerifier::EntityStatement es{JWT{entityStatement}};
    ON_CALL(poPpCertificateVerifierServiceMock, getEntityStatement()).WillByDefault(testing::Return(es));

    const SafeString prvPem{std::string{ResourceManager::instance().getStringResource(
        "test/generated_pki/sub_ca1_ec/certificates/popp_zd_sig_ec/popp_zd_sig_ec_key.pem")}};
    const auto prvKey = EllipticCurveUtils::pemToPrivatePublicKeyPair(prvPem);
    const auto [x, y] = EllipticCurveUtils::getPublicKeyCoordinatesHex(prvKey);
    const auto pubKey = EllipticCurveUtils::createPublicKeyHex(x, y, NID_X9_62_prime256v1);
    using namespace std::chrono_literals;
    const PoPPCertificateVerifier::PoPPCertificateCache::Key key{.kid = "dont care",
                                                                 .lastSuccessAt = model::Timestamp::now(),
                                                                 .expiration = model::Timestamp::now() + 1h,
                                                                 .certificate = nullptr,
                                                                 .publicKey = pubKey};
    ON_CALL(poPpCertificateVerifierServiceMock, getKey(::testing::_)).WillByDefault(::testing::Return(key));
}
