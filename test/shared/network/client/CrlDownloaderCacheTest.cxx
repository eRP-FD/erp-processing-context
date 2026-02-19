#include "mock/tsl/UrlRequestSenderMock.hxx"
#include "shared/network/client/CrlDownloadCache.hxx"
#include "shared/tsl/X509Certificate.hxx"
#include "shared/util/TLog.hxx"

#include <gtest/gtest.h>

class CrlDownloadCacheTest : public testing::Test
{
public:
    std::string pemToDer(const std::string& pem)
    {
        auto bio = BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size()));
        auto crl = shared_X509_CRL::make(PEM_read_bio_X509_CRL(bio, nullptr, nullptr, nullptr));

        unsigned char* derData = nullptr;
        int derLen = i2d_X509_CRL(crl.get(), &derData);

        std::string result(reinterpret_cast<char*>(derData), static_cast<size_t>(derLen));
        OPENSSL_free(derData);
        OPENSSL_free(bio);
        return result;
    }
};

TEST_F(CrlDownloadCacheTest, FollowsRedirectAndReturnsCrl)
{
    const std::string pemCert = R"(-----BEGIN CERTIFICATE-----
MIIDRDCCAiygAwIBAgIUdcuw5qgz7IHkVWGHdk0xOG2xnJcwDQYJKoZIhvcNAQEL
BQAwEzERMA8GA1UEAwwITXlUZXN0Q0EwHhcNMjYwMjA5MTMxOTQyWhcNMjcwMjA5
MTMxOTQyWjAUMRIwEAYDVQQDDAlsb2NhbGhvc3QwggEiMA0GCSqGSIb3DQEBAQUA
A4IBDwAwggEKAoIBAQDwmaooaPt1bw02KdoTSWZO34veG09bSYN9nagi0zd/Ra/r
uaVG4qUg7g8ROkbDeY+6Afi5b4y5UezwRSYlHRB6JDMUY2RBvu5v3ledXZ+NLXW4
Gcs0fWLErAqhcjC0osPVBHhrB4SYrmNUIEtuOo/99CBxzBNNDxZcReQSORVQZgcJ
QSMjX6g6M//X+x8fNkWyXcfNS6AIvBvcqkRLbND5PJPXnxUhbZqXtuAhhqVB3iAm
MBZusswsUESl00PIl1rnw6nug0LEKewkh7s1+5Bwpr4xPPUX2t0C9hr9dO9WzJNN
LeIEwnds51/pCB8F/40ZIy2DpErEMCvZ/r7kPK0/AgMBAAGjgY4wgYswFAYDVR0R
BA0wC4IJbG9jYWxob3N0MDMGA1UdHwQsMCowKKAmoCSGImh0dHA6Ly9jcmwuZXhh
bXBsZS5jb206ODAwMC9jYS5jcmwwHQYDVR0OBBYEFKRKbtNic1YJqF+jQpwlfu1L
ZnyoMB8GA1UdIwQYMBaAFBMV/DENDhvTo5nnQsry2K743EYbMA0GCSqGSIb3DQEB
CwUAA4IBAQAzs8gU2VhqCV3CoWgLublWsRQvTaVeTugQLNJgUDumvSAXUvtqJar5
avhSe46TNAqxBb6SUSLcFCPmRqOHhIQBPo5W0RtKvUnLqaLPodKfX2uCSyvU1o8+
SkLeE5/ZEci4AB4qijO9/PAsnkHyRMupEK0KKG+ACeIOZ+K47gw2l6WMw5oBsyiX
K8+iQTSFv6LvayAeeQXT0NuRZsKrYskh9xcF6SC9WcL3SI9sPC3yLN+MDQl6CcNi
vJbR2CQd///S7MJ9ORYJWQU6YB4b/NBmjiCMkZgSzOAnL7BqsYse25Ml0qe/k/0N
+RJw6JIexfcC4EB5YFu0PNX8QRKjYGpo
-----END CERTIFICATE-----
)";
    const std::string pemCrl = R"(-----BEGIN X509 CRL-----
MIIBbDBWAgEBMA0GCSqGSIb3DQEBCwUAMBMxETAPBgNVBAMMCE15VGVzdENBFw0y
NjAyMDkxMzE5NDJaFw0yNjAzMTExMzE5NDJaoA8wDTALBgNVHRQEBAICEAUwDQYJ
KoZIhvcNAQELBQADggEBABL6TA7FH5PqqDATRvRsMNabDqmH/Nn+7K+Hrt/AcDqO
NHOpQnP9tdh2hT3O1808vfuTo4WJ20+dz5N3n9YMYJBbkry52EPG5fS7i1YcNsbY
jixTS8jYbQOU/A9L3vrCioAVhP804m3iU24aRZXViVryVQF/zyfCRy2tZs4jY78j
SWJHtofZLw+eyKlgRJPF49h+kG/ILGQ3+Xsz9psI16kdLoMSVeeNvutaV5bAwVs+
34R6ok26i0uiJy51AfUqrN89K1//sfiyW31JDx2hHqiU8aFcZN7lUnGgi5KjjrCK
+oneAoJFJifF+bp6+4lxV2ImbzOoi8d5yTMEbS326Cg=
-----END X509 CRL-----)";
    const std::string derData = pemToDer(pemCrl);
    const std::string originalUrl = "http://crl.example.com:8000/ca.crl";
    const std::string followingUrl = "http://crl2.example.com:8000/ca.crl";
    auto mockSender =
        std::make_shared<UrlRequestSenderMock>(std::unordered_map<std::string, std::string>{{originalUrl, ""}});
    mockSender->setFollowRedirects(true);
    bool followed = false;
    mockSender->setUrlHandler(originalUrl, [&followingUrl](const std::string&) -> ClientResponse {
        Header header;
        header.setStatus(HttpStatus::Found);
        header.addHeaderField("Location", followingUrl);
        header.setContentLength(0);
        return {header, ""};
    });
    mockSender->setUrlHandler(followingUrl, [&derData, &followed](const std::string&) -> ClientResponse {
        Header header;
        header.setStatus(HttpStatus::OK);
        header.setContentLength(derData.size());
        followed = true;
        return {header, derData};
    });

    CrlDownloadCache crlDownloader(mockSender);

    const auto cert = X509Certificate::createFromPem(pemCert);

    EXPECT_NO_THROW(crlDownloader.getCrl(cert));
    EXPECT_TRUE(followed);
}
