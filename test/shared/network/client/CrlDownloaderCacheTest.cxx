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
MIIDTjCCAjagAwIBAgIUUXD1qfK7ya/AljTdjkBFCp0Nn00wDQYJKoZIhvcNAQEL
BQAwEzERMA8GA1UEAwwITXlUZXN0Q0EwHhcNMjYwMzExMTUzNzE4WhcNMjcwMzEx
MTUzNzE4WjATMREwDwYDVQQDDAhNeVRlc3RDQTCCASIwDQYJKoZIhvcNAQEBBQAD
ggEPADCCAQoCggEBAJ3bBGuuQobjVX0E5XBg6CxwXbU82mfw6cY11v70nuwebMtV
rbPTYXfnZdv8eeeC7Jvp2pdkYJfKehRm4o1sVwodmvUhUK/bs9jUCGf5mtULL7/W
UZhM29SxgzvMGK3X5R+UBHb6VWdDCmtbDFXkXAGzdXMYs5FlJVMi4PJV8LM2vztt
OmaOwjgFimiD/s2r+WuZY/7POMNsirMmVEVFyffvTpAoiaKKCuWlfgrQua7UqLTV
hX7tcAIzPR3OWZm1b2rlmqcDxF184PaVGIOUiCW4wJvaeyBSLe2MVNkFmBRFhRDO
mAHg9wC46BcNnHi7J1sScdKD94uySDgES1w1fgECAwEAAaOBmTCBljAdBgNVHQ4E
FgQUAf/nK9PPUC7p+3hrg4qvlzfaAhEwHwYDVR0jBBgwFoAUAf/nK9PPUC7p+3hr
g4qvlzfaAhEwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMCAQYwMwYDVR0f
BCwwKjAooCagJIYiaHR0cDovL2NybC5leGFtcGxlLmNvbTo4MDAwL2NhLmNybDAN
BgkqhkiG9w0BAQsFAAOCAQEAIldOhY/9Qi6q0IUYi2o95+IAWRlNs5mlGKfX6pWW
g7Ph6x3YzqphcoxlU7eXk2nXGhxGQUxdZfAmFfAfFZ645/fz0jz7NslJZZiT4ueR
BjEp90pXgC368G8D6Y/HNnhvKKhfaxaBv/m7Nz6mrYfBWhga/YDF0vVagAAbS3UZ
6K4VtHJbo9qOZcvdfv/YXulCJOBbJMjI3owYhEmyIMsIWLncPwomg0p4iCD1dIOr
3oepkr1bkhX6kYehsFqjswO7BWqDLgIMBinnznOpnVDZZtiMILhJ6llxEq70p56W
NDFLcUEGJoWIasn2pJ4fAG7luY5CgzgA8y9NxHE+DR2uKA==
-----END CERTIFICATE-----)";
    const std::string pemCrl = R"(-----BEGIN X509 CRL-----
MIIBbjBYAgEBMA0GCSqGSIb3DQEBCwUAMBMxETAPBgNVBAMMCE15VGVzdENBFw0y
NjAzMTExNTM3MThaGA8yMTI2MDIxNTE1MzcxOFqgDzANMAsGA1UdFAQEAgIQBzAN
BgkqhkiG9w0BAQsFAAOCAQEAjbvjDonPD9Nh0naFNBPfKG2zzKrqb+0p086BNLNe
pDn/U5af4WuYz7NVUfehmDUkI1YIrtO1WAdOk9PHIDdGjKtVzf+GpPU9nexvH7dS
DPBZqbSG5teLZJsYoKqM1XL5jC/PKCazbWhaz+dqa2+C5lTMiqwPlKniyzxKZKHf
qLLMZ410lExLuqzRynhhcmmGLfPF9WQtoVMPYPPHzhmRQmoY0qbLbAhOpBh+BCHZ
ywnbW7sJaso5pBK72+YXdOmo/rdDj2uuzPjOX1dKX1ze8qWL0TMrx6QauU9qpc4T
6VV5kbkRQ7U3Q/K9K1h4SCcQdAopuy0nx9vW8hr+9Xecpw==
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
