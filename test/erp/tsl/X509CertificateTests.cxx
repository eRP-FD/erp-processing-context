/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/tsl/X509Certificate.hxx"

#include "erp/crypto/Certificate.hxx"
#include "erp/model/Timestamp.hxx"
#include "erp/pc/ProfessionOid.hxx"
#include "erp/tsl/TslService.hxx"
#include "erp/util/Base64.hxx"
#include "erp/util/FileHelper.hxx"

#include <gtest/gtest.h>
#include <string>
#include <test_config.h>
#include <vector>


namespace
{
    // fingerprint (SHA1):
    // "05:99:D6:6F:5F:94:BB:7C:BB:E9:F1:6A:DD:75:49:38:FB:4C:A5:82"
    const std::string TSL_CA_CERTIFICATE{
        "MIIEGDCCAwCgAwIBAgIBAjANBgkqhkiG9w0BAQsFADBtMQswCQYDVQQGEwJERTEVMBMGA1UECgwMZ2VtYXRpayB"
        "HbWJIMTQwMgYDVQQLDCtaZW50cmFsZSBSb290LUNBIGRlciBUZWxlbWF0aWtpbmZyYXN0cnVrdHVyMREwDwYDVQ"
        "QDDAhHRU0uUkNBMTAeFw0xNTAyMTkxMDE4NDlaFw0yMzAyMTcxMDE4NDhaMG0xCzAJBgNVBAYTAkRFMRUwEwYDV"
        "QQKDAxnZW1hdGlrIEdtYkgxMTAvBgNVBAsMKFRTTC1TaWduZXItQ0EgZGVyIFRlbGVtYXRpa2luZnJhc3RydWt0"
        "dXIxFDASBgNVBAMMC0dFTS5UU0wtQ0ExMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA5tPmdjiMN2G"
        "cBJVAPwcWJYVPZ6xyYjNRj9jkZWib6ionw2pHbWlqAt3dSuUF7TDv1BYI7kmAZOFi2x2YxuQasOeQWPRuQAqF7n"
        "0f1BofvX+N3mrJj3+DmXwdizumREb1s3ku/KsRNdQxpVM0oMsVd3KaM6WsEbqPGoC7aiESXB3rZ2w+TUjgZSsbv"
        "BJ6P7YgtEWRnFVsYAaseFJQ/B7i2EFZ+C5PkxF9k3XozYk6CW231fr27szyQPWVHGJxOnXuhxPn4GHvTaO7Ylo3"
        "ndeVfDHmf1ymh0Ot+dH1Ge11rByKrnv1ylCSy28uAGj37QaQfrE+fFpV1fvSwi37moFshQIDAQABo4HCMIG/MB0"
        "GA1UdDgQWBBQ44AzdIfdArjDncJFzwKziJ/TnnDAfBgNVHSMEGDAWgBSEhCB3W2WDNzhNAFynjqCg8vjSNTBCBg"
        "grBgEFBQcBAQQ2MDQwMgYIKwYBBQUHMAGGJmh0dHA6Ly9vY3NwLnJvb3QtY2EudGktZGllbnN0ZS5kZS9vY3NwM"
        "BIGA1UdEwEB/wQIMAYBAf8CAQAwDgYDVR0PAQH/BAQDAgEGMBUGA1UdIAQOMAwwCgYIKoIUAEwEgSMwDQYJKoZI"
        "hvcNAQELBQADggEBAEAQ9aM5RUUiXzuCmwtLXNFg6Qtv9Vxm0NHh/zaFe1jwBPl+k8LOQE0/qW7ZkYfCVcxle0W"
        "b5hnAY52GtRk0xpFbZrdswgdYJsn5IvSx978kqXdUbckiMT3M6mc9ksM0ygoxVQIxXW7AvQgt200lnsLh8a8/zT"
        "9lNU6z0ruLtXEoKbGadQAOB4Lv7WURywxF/J1wGHy9YL/HoN8bGcv40oVBUycIZDO/M2k9urxLwBZCVNThpjhic"
        "C3tiGyhvw7a+TgH22zUyWnvl+cgqC2m1jrdTc8fbejkAWtKnQqOZJc3JX4ybcrXSQp8Uj1DbJzarF5YjKUDuWG6"
        "/wBmM+zy6YE="
    };

    const std::string AKTOR_CERTIFICATE{
        "MIIEvzCCA6egAwIBAgIHAhpo+Vj77jANBgkqhkiG9w0BAQsFADCBljELMAkGA1UEBhMCREUxHzAdBgNVBAoMFmdlbW"
        "F0aWsgR21iSCBOT1QtVkFMSUQxRTBDBgNVBAsMPEVsZWt0cm9uaXNjaGUgR2VzdW5kaGVpdHNrYXJ0ZS1DQSBkZXIg"
        "VGVsZW1hdGlraW5mcmFzdHJ1a3R1cjEfMB0GA1UEAwwWR0VNLkVHSy1DQTI0IFRFU1QtT05MWTAeFw0xOTExMTkwMD"
        "AwMDBaFw0yNDExMTgyMzU5NTlaMIG2MQswCQYDVQQGEwJERTEdMBsGA1UECgwUVGVzdCBHS1YtU1ZOT1QtVkFMSUQx"
        "EzARBgNVBAsMClgxMTA0MTAzMTcxEjAQBgNVBAsMCTEwOTUwMDk2OTEUMBIGA1UEBAwLUG9ww7N3aXRzY2gxGTAXBg"
        "NVBCoMEEFkZWxoZWlkIEdyw6RmaW4xLjAsBgNVBAMMJUFkZWxoZWlkIEdyw6RmaW4gUG9ww7N3aXRzY2hURVNULU9O"
        "TFkwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQC/iKyZQ6MWMgyGzmydR4NyORy8jhdXNu6r+8+wONGI7F"
        "dW9gYC/t/2aDms6IWjj3edmGqs7nshCUlnGa7eoBlXXkfn6PB3i4FQ8ttWXS3eL7v52H56RJ6dv/71AmvjGbha51Ty"
        "sv84k93IEkmpaUSfJR0A37sebx3gYfKA5ZEPafwg2So500qecCfE3uvNxsRDbBRf19N0iDxb56rX/k1eaAmOuWVopm"
        "nFhmzGKMBdG/oNf6cIuqlm3bWB9KTO5yau6tZ30U9TW19GEAS4QF7EVxmd7HJhjEO2XkFSSBihG0uK4nhYE0WZr054"
        "6uZMLBGYN0Jc6zXvGyNqUcoLeI6RAgMBAAGjge8wgewwIAYDVR0gBBkwFzAKBggqghQATASBIzAJBgcqghQATARGMB"
        "0GA1UdDgQWBBRszdemmtlfMf6uvoQVmpaUN3k5gjAfBgNVHSMEGDAWgBSwop3aOwscbPPxnrB1NoBx66WwoTAwBgUr"
        "JAgDAwQnMCUwIzAhMB8wHTAQDA5WZXJzaWNoZXJ0ZS8tcjAJBgcqghQATAQxMAwGA1UdEwEB/wQCMAAwOAYIKwYBBQ"
        "UHAQEELDAqMCgGCCsGAQUFBzABhhxodHRwOi8vZWhjYS5nZW1hdGlrLmRlL29jc3AvMA4GA1UdDwEB/wQEAwIHgDAN"
        "BgkqhkiG9w0BAQsFAAOCAQEACwrQFXbprY20lpsOgIfHTRz8Ru92q+T8/Hp4BIJ/lochRx6FwnAQ4wo1HPX3+icIfG"
        "DVI1MxwRbdtU1ZXqmqjXj8Z+VGLcLOi7jMdLtGC0T31t8LXgz+BTcI8ibub5Rdja42DUT5Ap1VkwtNpMgYURq9ITqv"
        "kV6F2qhXf2Xr+KM37l4XL0U9RGAqd7Jo/0EAaT+xqaLMhDYAsLX1aM2uZi7Pj/lBfhge3a5PFP2ur89E/+KaY5oNru"
        "Wt/5N81WCOS99EgS+M35x+rUpidrPLaAlF2FmfZX7MPXRvJdZ+B05T5Nj0DzwqoHtSkvT3OouFdt7cH3hWUrg6P3+6"
        "T4IHIg=="
    };

    const std::string EGK_CERTIFICATE{
        "MIIDdDCCAxqgAwIBAgIDFEc9MAoGCCqGSM49BAMCMIGqMQswCQYDVQQGEwJERTEzMDEGA1UECgwqQXRvcyBJbmZvcm"
        "1hdGlvbiBUZWNobm9sb2d5IEdtYkggTk9ULVZBTElEMUUwQwYDVQQLDDxFbGVrdHJvbmlzY2hlIEdlc3VuZGhlaXRz"
        "a2FydGUtQ0EgZGVyIFRlbGVtYXRpa2luZnJhc3RydWt0dXIxHzAdBgNVBAMMFkFUT1MuRUdLLUNBNyBURVNULU9OTF"
        "kwHhcNMTkwNzE5MDkwNjQzWhcNMjQwNzE5MDkwNjQzWjCBnzELMAkGA1UEBhMCREUxHzAdBgNVBAoMFlRlY2huaWtl"
        "ciBLcmFua2Vua2Fzc2UxEjAQBgNVBAsMCTEwMTU3NTUxOTETMBEGA1UECwwKUDMyNjE2MTY1NDEPMA0GA1UEBAwGUG"
        "VydGVzMRYwFAYDVQQqDA1LbGF1cyBNYXJxdWVzMR0wGwYDVQQDDBRLbGF1cyBNYXJxdWVzIFBlcnRlczBaMBQGByqG"
        "SM49AgEGCSskAwMCCAEBBwNCAARiBW5aouSwQJ6oGSI8cEho5aALqOh1fAvlCnqlE1DxyqS9EArAYdX6J/DOJ118di"
        "NCIyyZmsdDHcmDoY0zWdtCo4IBNTCCATEwHQYDVR0OBBYEFBHnW1MI0ZF94CKHDsPfKnmVTQFKMA4GA1UdDwEB/wQE"
        "AwIHgDAMBgNVHRMBAf8EAjAAMFEGA1UdIARKMEgwCQYHKoIUAEwERjA7BggqghQATASBIzAvMC0GCCsGAQUFBwIBFi"
        "FodHRwOi8vd3d3LmdlbWF0aWsuZGUvZ28vcG9saWNpZXMwNwYIKwYBBQUHAQEEKzApMCcGCCsGAQUFBzABhhtodHRw"
        "Oi8vb2NzcC5lZ2stdGVzdC10c3AuZGUwHwYDVR0jBBgwFoAUhc+8WluAPPhBED4RAnG/W1+Jp1swEwYDVR0lBAwwCg"
        "YIKwYBBQUHAwIwMAYFKyQIAwMEJzAlMCMwITAfMB0wEAwOVmVyc2ljaGVydGUvLXIwCQYHKoIUAEwEMTAKBggqhkjO"
        "PQQDAgNIADBFAiEAoZIftDEVf5MH4cN5/y6JEfXlENFzASJYrwg3/aJsFUsCIAC95DlUzHfErA0YvG1+xByGrLhnYJ"
        "PjF5VourUFqoqE"
    };

    // cf. EPA-11273
    const std::string VAU_TEE_HANDSHAKE_CERTIFICATE{
        "MIIBdzCCAR4CBEmWAtIwCgYIKoZIzj0EAwIwRjELMAkGA1UEBhMCREUxIzAhBgNVBAoMGm1vY2stY2"
        "VydGlmaWNhdGUtYXV0aG9yaXR5MRIwEAYDVQQDDAlsb2NhbGhvc3QwHhcNMjAwMzI2MTUxMzI1WhcN"
        "MjAwMzI2MTYxMzI1WjBGMQswCQYDVQQGEwJERTEjMCEGA1UECgwabW9jay1jZXJ0aWZpY2F0ZS1hdX"
        "Rob3JpdHkxEjAQBgNVBAMMCWxvY2FsaG9zdDBaMBQGByqGSM49AgEGCSskAwMCCAEBBwNCAASVcG+f"
        "5CVkVDU+XcskEGLKSanJQGNDLUd0+UKeNoj+HVktPyQktx3oSGE+YuIcrBYwlBlfekGb0ybWSxSA1a"
        "s5MAoGCCqGSM49BAMCA0cAMEQCIH6xC4+4zqp2ylOz0aW2gz1QI4BYgFjLpLCU5tjNB0hFAiBooMrp"
        "37XlYwjnoTN+1H93oOdeFr+LaqonbAhFH8nuHg=="
    };

    const std::string TELEMATIK_ID_CERTIFICATE{
        "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"
    };

    // role OIDs
    // cf. gemSpec_OID Tab_PKI_402
    constexpr const char* oid_versicherter{"1.2.276.0.76.4.49"};
    constexpr const char* oid_zahnarzt{"1.2.276.0.76.4.31"};

    // policy OIDs
    // cf. gemSpec_OID Tab_PKI_404
    constexpr const char* oid_policy_gem_or_cp{"1.2.276.0.76.4.163"};
    constexpr const char* oid_policy_gem_tsl_signer{"1.2.276.0.76.4.176"};
} // anonymous namespace

class X509CertificateTests
    : public ::testing::Test
{
protected:
    void SetUp () override
    {
        certificate = X509Certificate::createFromBase64(TSL_CA_CERTIFICATE);
    }


    X509Certificate certificate;
};


TEST_F(X509CertificateTests, CertificateCanBeCreatedFromPemString)
{
    EXPECT_TRUE(certificate.hasX509Ptr());
}


TEST_F(X509CertificateTests, FingerprintHasExpectedValue)
{
    const std::string sha1Fingerprint{"0599d66f5f94bb7cbbe9f16add754938fb4ca582"};
    EXPECT_EQ(certificate.getSha1FingerprintHex(), sha1Fingerprint);
}


TEST_F(X509CertificateTests, SubjectKeyIdHasExpectedValue)
{
    const std::string keyId{"38e00cdd21f740ae30e7709173c0ace227f4e79c"};
    EXPECT_EQ(certificate.getSubjectKeyIdentifier(), keyId);
}


TEST_F(X509CertificateTests, AuthorityKeyIdHasExpectedValue)
{
    const std::string keyId{"848420775b658337384d005ca78ea0a0f2f8d235"};
    EXPECT_EQ(certificate.getAuthorityKeyIdentifier(), keyId);
}


TEST_F(X509CertificateTests, CertificateIsCaCertificate)
{
    EXPECT_TRUE(certificate.isCaCert());
}


TEST_F(X509CertificateTests, IssuerHasExpectedValue)
{
    const std::string issuer{
        "/C=DE/O=gematik GmbH/OU=Zentrale Root-CA der Telematikinfrastruktur/CN=GEM.RCA1"};
    EXPECT_EQ(certificate.getIssuer(), issuer);
}


TEST_F(X509CertificateTests, SubjectHasExpectedValue)
{
    const std::string subject{
        "/C=DE/O=gematik GmbH/OU=TSL-Signer-CA der Telematikinfrastruktur/CN=GEM.TSL-CA1"};
    EXPECT_EQ(certificate.getSubject(), subject);
}


TEST_F(X509CertificateTests, CertificateValidity)//NOLINT(readability-function-cognitive-complexity)
{
    // The certificate is valid from 2015-02-19 through 2023-02-17. Test this ...

    EXPECT_FALSE(certificate.checkValidityPeriod(model::Timestamp::fromFhirDateTime("2001-12-24T00:00:00Z").toChronoTimePoint()));
    EXPECT_FALSE(certificate.checkValidityPeriod(model::Timestamp::fromFhirDateTime("2015-02-18T00:00:00Z").toChronoTimePoint()));

    EXPECT_TRUE(certificate.checkValidityPeriod(model::Timestamp::fromFhirDateTime("2015-02-20T00:00:00Z").toChronoTimePoint()));
    EXPECT_TRUE(certificate.checkValidityPeriod(model::Timestamp::fromFhirDateTime("2023-02-16T00:00:00Z").toChronoTimePoint()));

    EXPECT_FALSE(certificate.checkValidityPeriod(model::Timestamp::fromFhirDateTime("2023-03-18T00:00:00Z").toChronoTimePoint()));
    EXPECT_FALSE(certificate.checkValidityPeriod(model::Timestamp::fromFhirDateTime("2123-12-24T00:00:00Z").toChronoTimePoint()));
}


TEST_F(X509CertificateTests, UserInformationFromAktor)
{
    auto userCertificate = X509Certificate::createFromBase64(AKTOR_CERTIFICATE);

    EXPECT_TRUE(userCertificate.hasX509Ptr());

    auto identifierInfo = userCertificate.extractIdentifierInformation();

    EXPECT_EQ(identifierInfo.givenName, "Adelheid Gräfin");
    EXPECT_EQ(identifierInfo.surname, "Popówitsch");
    EXPECT_EQ(identifierInfo.personId, "X110410317");
    EXPECT_EQ(identifierInfo.institutionId, "109500969");
}


TEST_F(X509CertificateTests, UserInformationFromNfc)
{
    auto egkCertificate = X509Certificate::createFromBase64(EGK_CERTIFICATE);
    EXPECT_TRUE(egkCertificate.hasX509Ptr());

    EXPECT_EQ(egkCertificate.getSigningAlgorithm(), SigningAlgorithm::ellipticCurve);

    auto identifierInfo = egkCertificate.extractIdentifierInformation();

    EXPECT_EQ(identifierInfo.personId, "P326161654");
    EXPECT_EQ(identifierInfo.institutionId, "101575519");
}

TEST_F(X509CertificateTests, OCSPUrls)
{
    auto userCertificate = X509Certificate::createFromBase64(AKTOR_CERTIFICATE);

    EXPECT_TRUE(userCertificate.hasX509Ptr());

    auto ocspUrls = userCertificate.getOcspUrls();

    EXPECT_EQ(ocspUrls.size(), 1);
    EXPECT_EQ(ocspUrls.at(0), "http://ehca.gematik.de/ocsp/");
}

TEST_F(X509CertificateTests, Base64Format)
{
    auto cert = X509Certificate::createFromBase64(EGK_CERTIFICATE);
    EXPECT_EQ(cert.toBase64(), EGK_CERTIFICATE);
}


TEST_F(X509CertificateTests, EmptyRoleIdIsNotOk)
{
    const auto egkCertificate = X509Certificate::createFromBase64(EGK_CERTIFICATE);
    EXPECT_FALSE(egkCertificate.checkRoles({}));
}


TEST_F(X509CertificateTests, MalformedRoleIdIsNotOk)
{
    const auto egkCertificate = X509Certificate::createFromBase64(EGK_CERTIFICATE);
    EXPECT_FALSE(egkCertificate.checkRoles({"This is not a role ID"}));
}


TEST_F(X509CertificateTests, RoleIdIsVersicherter)
{
    const auto egkCertificate = X509Certificate::createFromBase64(EGK_CERTIFICATE);
    EXPECT_TRUE(egkCertificate.checkRoles({oid_versicherter}));
}


TEST_F(X509CertificateTests, RoleIdIsNotOidVersicherter)
{
    const auto egkCertificate = X509Certificate::createFromBase64(EGK_CERTIFICATE);
    EXPECT_FALSE(egkCertificate.checkRoles({oid_zahnarzt}));
}


TEST_F(X509CertificateTests, KeyUsageContainsCrlSign)
{
    EXPECT_TRUE(certificate.checkKeyUsage({KeyUsage::cRLSign}));
}


TEST_F(X509CertificateTests, KeyUsageContainsKeyCertSign)
{
    EXPECT_TRUE(certificate.checkKeyUsage({KeyUsage::keyCertSign}));
}


TEST_F(X509CertificateTests, KeyUsageContainsKeyCertSignAndCrlSign)
{
    EXPECT_TRUE(certificate.checkKeyUsage({KeyUsage::keyCertSign, KeyUsage::cRLSign}));
}


TEST_F(X509CertificateTests, KeyUsageDoesNotContainNonRepudiation)
{
    EXPECT_FALSE(certificate.checkKeyUsage({KeyUsage::nonRepudiation}));
    // nonRepudiation combined with contained keyCertSign
    EXPECT_FALSE(certificate.checkKeyUsage({KeyUsage::keyCertSign, KeyUsage::nonRepudiation}));
}

TEST_F(X509CertificateTests, ExtendedKeyUsageContansSSLClient)
{
    const auto egkCertificate = X509Certificate::createFromBase64(EGK_CERTIFICATE);
    EXPECT_TRUE(egkCertificate.checkExtendedKeyUsage({ExtendedKeyUsage::sslClient}));
}

TEST_F(X509CertificateTests, ExtendedKeyUsageIsEmpty)
{
    EXPECT_TRUE(certificate.checkExtendedKeyUsage({}));
}


TEST_F(X509CertificateTests, CertificatePolicyDoesNotContainEmptyOid)
{
    EXPECT_FALSE(certificate.checkCertificatePolicy({}));
}


TEST_F(X509CertificateTests, CertificatePolicyDoesNotContainMalformedOid)
{
    EXPECT_FALSE(certificate.checkCertificatePolicy("This is not an oid"));
}


TEST_F(X509CertificateTests, CertificatePolicyIsGemOrCp)
{
    EXPECT_TRUE(certificate.checkCertificatePolicy(oid_policy_gem_or_cp));
}


TEST_F(X509CertificateTests, CertificatePolicyIsNotTslSigner)
{
    EXPECT_FALSE(certificate.checkCertificatePolicy(oid_policy_gem_tsl_signer));
}


TEST_F(X509CertificateTests, CertificateHasNoRoles)
{
    const std::vector<std::string> roleIds{certificate.getRoles()};

    EXPECT_TRUE(roleIds.empty());
}


TEST_F(X509CertificateTests, EgkCertificateHasRoles)
{
    const auto egkCertificate = X509Certificate::createFromBase64(EGK_CERTIFICATE);
    const std::vector<std::string> roleIds{egkCertificate.getRoles()};

    ASSERT_EQ(roleIds.size(), 1);
    EXPECT_EQ(roleIds[0], oid_versicherter);
}


TEST_F(X509CertificateTests, CertificateFromVauTeeHandshakeCanBeParsed)
{
    const auto vauCertificate{X509Certificate::createFromBase64(VAU_TEE_HANDSHAKE_CERTIFICATE)};

    EXPECT_TRUE(vauCertificate.hasX509Ptr());
    EXPECT_EQ(vauCertificate.getSubject(), "/C=DE/O=mock-certificate-authority/CN=localhost");
    EXPECT_EQ(vauCertificate.getIssuer(), "/C=DE/O=mock-certificate-authority/CN=localhost");
    EXPECT_EQ(vauCertificate.getSigningAlgorithm(), SigningAlgorithm::ellipticCurve);
}


TEST_F(X509CertificateTests, ContainsTelematikId)
{
    const auto telematikIdCertificate{X509Certificate::createFromBase64(TELEMATIK_ID_CERTIFICATE)};

    EXPECT_TRUE(telematikIdCertificate.containsTelematikId("2-2-EPA-AKTOR-ZArzt-01"));
    EXPECT_FALSE(telematikIdCertificate.containsTelematikId("2-2-EPA-AKTOR-ZArzt-01-tampered"));
}


// "X.509-Identitäten für die Erstellung und Prüfung digitaler nicht-qualifizierter elektronischer Signaturen"
// GEMREQ-start GS-A_4357
TEST_F(X509CertificateTests, VerifyRsaSignedCertificate)
{
    // Certificates taken from: https://github.com/chromium/badssl.com/blob/master/certs/sets/prod/pregen/chain/wildcard-rsa2048.pem
    const std::string signedRsaCertificate =
        "MIIGqDCCBZCgAwIBAgIQCvBs2jemC2QTQvCh6x1Z/TANBgkqhkiG9w0BAQsFADBN"
        "MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMScwJQYDVQQDEx5E"
        "aWdpQ2VydCBTSEEyIFNlY3VyZSBTZXJ2ZXIgQ0EwHhcNMjAwMzIzMDAwMDAwWhcN"
        "MjIwNTE3MTIwMDAwWjBuMQswCQYDVQQGEwJVUzETMBEGA1UECBMKQ2FsaWZvcm5p"
        "YTEVMBMGA1UEBxMMV2FsbnV0IENyZWVrMRwwGgYDVQQKExNMdWNhcyBHYXJyb24g"
        "VG9ycmVzMRUwEwYDVQQDDAwqLmJhZHNzbC5jb20wggEiMA0GCSqGSIb3DQEBAQUA"
        "A4IBDwAwggEKAoIBAQDCBOz4jO4EwrPYUNVwWMyTGOtcqGhJsCK1+ZWesSssdj5s"
        "wEtgTEzqsrTAD4C2sPlyyYYC+VxBXRMrf3HES7zplC5QN6ZnHGGM9kFCxUbTFocn"
        "n3TrCp0RUiYhc2yETHlV5NFr6AY9SBVSrbMo26r/bv9glUp3aznxJNExtt1NwMT8"
        "U7ltQq21fP6u9RXSM0jnInHHwhR6bCjqN0rf6my1crR+WqIW3GmxV0TbChKr3sMP"
        "R3RcQSLhmvkbk+atIgYpLrG6SRwMJ56j+4v3QHIArJII2YxXhFOBBcvm/mtUmEAn"
        "hccQu3Nw72kYQQdFVXz5ZD89LMOpfOuTGkyG0cqFAgMBAAGjggNhMIIDXTAfBgNV"
        "HSMEGDAWgBQPgGEcgjFh1S8o541GOLQs4cbZ4jAdBgNVHQ4EFgQUne7Be4ELOkdp"
        "cRh9ETeTvKUbP/swIwYDVR0RBBwwGoIMKi5iYWRzc2wuY29tggpiYWRzc2wuY29t"
        "MA4GA1UdDwEB/wQEAwIFoDAdBgNVHSUEFjAUBggrBgEFBQcDAQYIKwYBBQUHAwIw"
        "awYDVR0fBGQwYjAvoC2gK4YpaHR0cDovL2NybDMuZGlnaWNlcnQuY29tL3NzY2Et"
        "c2hhMi1nNi5jcmwwL6AtoCuGKWh0dHA6Ly9jcmw0LmRpZ2ljZXJ0LmNvbS9zc2Nh"
        "LXNoYTItZzYuY3JsMEwGA1UdIARFMEMwNwYJYIZIAYb9bAEBMCowKAYIKwYBBQUH"
        "AgEWHGh0dHBzOi8vd3d3LmRpZ2ljZXJ0LmNvbS9DUFMwCAYGZ4EMAQIDMHwGCCsG"
        "AQUFBwEBBHAwbjAkBggrBgEFBQcwAYYYaHR0cDovL29jc3AuZGlnaWNlcnQuY29t"
        "MEYGCCsGAQUFBzAChjpodHRwOi8vY2FjZXJ0cy5kaWdpY2VydC5jb20vRGlnaUNl"
        "cnRTSEEyU2VjdXJlU2VydmVyQ0EuY3J0MAwGA1UdEwEB/wQCMAAwggF+BgorBgEE"
        "AdZ5AgQCBIIBbgSCAWoBaAB2ALvZ37wfinG1k5Qjl6qSe0c4V5UKq1LoGpCWZDaO"
        "HtGFAAABcQhGXioAAAQDAEcwRQIgDfWVBXEuUZC2YP4Si3AQDidHC4U9e5XTGyG7"
        "SFNDlRkCIQCzikrA1nf7boAdhvaGu2Vkct3VaI+0y8p3gmonU5d9DwB2ACJFRQdZ"
        "VSRWlj+hL/H3bYbgIyZjrcBLf13Gg1xu4g8CAAABcQhGXlsAAAQDAEcwRQIhAMWi"
        "Vsi2vYdxRCRsu/DMmCyhY0iJPKHE2c6ejPycIbgqAiAs3kSSS0NiUFiHBw7QaQ/s"
        "GO+/lNYvjExlzVUWJbgNLwB2AFGjsPX9AXmcVm24N3iPDKR6zBsny/eeiEKaDf7U"
        "iwXlAAABcQhGXnoAAAQDAEcwRQIgKsntiBqt8Au8DAABFkxISELhP3U/wb5lb76p"
        "vfenWL0CIQDr2kLhCWP/QUNxXqGmvr1GaG9EuokTOLEnGPhGv1cMkDANBgkqhkiG"
        "9w0BAQsFAAOCAQEA0RGxlwy3Tl0lhrUAn2mIi8LcZ9nBUyfAcCXCtYyCdEbjIP64"
        "xgX6pzTt0WJoxzlT+MiK6fc0hECZXqpkTNVTARYtGkJoljlTK2vAdHZ0SOpm9OT4"
        "RLfjGnImY0hiFbZ/LtsvS2Zg7cVJecqnrZe/za/nbDdljnnrll7C8O5naQuKr4te"
        "uice3e8a4TtviFwS/wdDnJ3RrE83b1IljILbU5SV0X1NajyYkUWS7AnOmrFUUByz"
        "MwdGrM6kt0lfJy/gvGVsgIKZocHdedPeECqAtq7FAJYanOsjNN9RbBOGhbwq0/FP"
        "CC01zojqS10nGowxzOiqyB4m6wytmzf0QwjpMw==";
    const std::string issuerCertificateRsa =
        "MIIElDCCA3ygAwIBAgIQAf2j627KdciIQ4tyS8+8kTANBgkqhkiG9w0BAQsFADBh"
        "MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3"
        "d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD"
        "QTAeFw0xMzAzMDgxMjAwMDBaFw0yMzAzMDgxMjAwMDBaME0xCzAJBgNVBAYTAlVT"
        "MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxJzAlBgNVBAMTHkRpZ2lDZXJ0IFNIQTIg"
        "U2VjdXJlIFNlcnZlciBDQTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEB"
        "ANyuWJBNwcQwFZA1W248ghX1LFy949v/cUP6ZCWA1O4Yok3wZtAKc24RmDYXZK83"
        "nf36QYSvx6+M/hpzTc8zl5CilodTgyu5pnVILR1WN3vaMTIa16yrBvSqXUu3R0bd"
        "KpPDkC55gIDvEwRqFDu1m5K+wgdlTvza/P96rtxcflUxDOg5B6TXvi/TC2rSsd9f"
        "/ld0Uzs1gN2ujkSYs58O09rg1/RrKatEp0tYhG2SS4HD2nOLEpdIkARFdRrdNzGX"
        "kujNVA075ME/OV4uuPNcfhCOhkEAjUVmR7ChZc6gqikJTvOX6+guqw9ypzAO+sf0"
        "/RR3w6RbKFfCs/mC/bdFWJsCAwEAAaOCAVowggFWMBIGA1UdEwEB/wQIMAYBAf8C"
        "AQAwDgYDVR0PAQH/BAQDAgGGMDQGCCsGAQUFBwEBBCgwJjAkBggrBgEFBQcwAYYY"
        "aHR0cDovL29jc3AuZGlnaWNlcnQuY29tMHsGA1UdHwR0MHIwN6A1oDOGMWh0dHA6"
        "Ly9jcmwzLmRpZ2ljZXJ0LmNvbS9EaWdpQ2VydEdsb2JhbFJvb3RDQS5jcmwwN6A1"
        "oDOGMWh0dHA6Ly9jcmw0LmRpZ2ljZXJ0LmNvbS9EaWdpQ2VydEdsb2JhbFJvb3RD"
        "QS5jcmwwPQYDVR0gBDYwNDAyBgRVHSAAMCowKAYIKwYBBQUHAgEWHGh0dHBzOi8v"
        "d3d3LmRpZ2ljZXJ0LmNvbS9DUFMwHQYDVR0OBBYEFA+AYRyCMWHVLyjnjUY4tCzh"
        "xtniMB8GA1UdIwQYMBaAFAPeUDVW0Uy7ZvCj4hsbw5eyPdFVMA0GCSqGSIb3DQEB"
        "CwUAA4IBAQAjPt9L0jFCpbZ+QlwaRMxp0Wi0XUvgBCFsS+JtzLHgl4+mUwnNqipl"
        "5TlPHoOlblyYoiQm5vuh7ZPHLgLGTUq/sELfeNqzqPlt/yGFUzZgTHbO7Djc1lGA"
        "8MXW5dRNJ2Srm8c+cftIl7gzbckTB+6WohsYFfZcTEDts8Ls/3HB40f/1LkAtDdC"
        "2iDJ6m6K7hQGrn2iWZiIqBtvLfTyyRRfJs8sjX7tN8Cp1Tm5gr8ZDOo0rwAhaPit"
        "c+LJMto4JQtV05od8GiG7S5BNO98pVAdvzr508EIDObtHopYJeS4d60tbvVS3bR0"
        "j6tJLp07kzQoH3jOlOrHvdPJbRzeXDLz";

    const auto certificateRsa = X509Certificate::createFromBase64(signedRsaCertificate);
    const auto issuerRsa = X509Certificate::createFromBase64(issuerCertificateRsa);
    EXPECT_TRUE(certificateRsa.signatureIsValidAndWasSignedBy(issuerRsa));
    EXPECT_FALSE(issuerRsa.signatureIsValidAndWasSignedBy(certificateRsa));
    EXPECT_FALSE(issuerRsa.signatureIsValidAndWasSignedBy(issuerRsa));
    EXPECT_FALSE(certificateRsa.signatureIsValidAndWasSignedBy(certificateRsa));
}
// GEMREQ-end GS-A_4357


TEST_F(X509CertificateTests, ReadQesCertificate)
{
    const std::string certificatePem =
        FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-C_HP_QES_E256.pem");
    const Certificate cert = Certificate::fromPem(certificatePem);
    const X509Certificate x509Certificate = X509Certificate::createFromBase64(cert.toBase64Der());

    EXPECT_TRUE(x509Certificate.checkQcStatement(TslService::id_etsi_qcs_QcCompliance));

    EXPECT_EQ(std::vector<std::string>{"http://ehca.gematik.de/ocsp/"}, x509Certificate.getOcspUrls());

    const auto roles = x509Certificate.getRoles();
    EXPECT_EQ(std::vector<std::string>{std::string(profession_oid::oid_arzt)}, roles);
}
