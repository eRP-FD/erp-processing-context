#ifndef ERP_PROCESSING_CONTEXT_TEST_ERP_PC_CFDSIGERPTESTHELPER_HXX
#define ERP_PROCESSING_CONTEXT_TEST_ERP_PC_CFDSIGERPTESTHELPER_HXX

#include "erp/util/FileHelper.hxx"
#include "erp/util/Hash.hxx"
#include "test_config.h"

#include <unordered_map>
#include <memory>
#include <string>


class CFdSigErpTestHelper
{
public:
    static constexpr char cFdSigErpSigner[] =
        "-----BEGIN CERTIFICATE-----\n"
        "MIIDGjCCAr+gAwIBAgIBFzAKBggqhkjOPQQDAjCBgTELMAkGA1UEBhMCREUxHzAd\n"
        "BgNVBAoMFmdlbWF0aWsgR21iSCBOT1QtVkFMSUQxNDAyBgNVBAsMK1plbnRyYWxl\n"
        "IFJvb3QtQ0EgZGVyIFRlbGVtYXRpa2luZnJhc3RydWt0dXIxGzAZBgNVBAMMEkdF\n"
        "TS5SQ0EzIFRFU1QtT05MWTAeFw0xNzA4MzAxMTM2MjJaFw0yNTA4MjgxMTM2MjFa\n"
        "MIGEMQswCQYDVQQGEwJERTEfMB0GA1UECgwWZ2VtYXRpayBHbWJIIE5PVC1WQUxJ\n"
        "RDEyMDAGA1UECwwpS29tcG9uZW50ZW4tQ0EgZGVyIFRlbGVtYXRpa2luZnJhc3Ry\n"
        "dWt0dXIxIDAeBgNVBAMMF0dFTS5LT01QLUNBMTAgVEVTVC1PTkxZMFowFAYHKoZI\n"
        "zj0CAQYJKyQDAwIIAQEHA0IABDFinQgzfsT1CN0QWwdm7e2JiaDYHocCiy1TWpOP\n"
        "yHwoPC54RULeUIBJeX199Qm1FFpgeIRP1E8cjbHGNsRbju6jggEgMIIBHDAdBgNV\n"
        "HQ4EFgQUKPD45qnId8xDRduartc6g6wOD6gwHwYDVR0jBBgwFoAUB5AzLXVTXn/4\n"
        "yDe/fskmV2jfONIwQgYIKwYBBQUHAQEENjA0MDIGCCsGAQUFBzABhiZodHRwOi8v\n"
        "b2NzcC5yb290LWNhLnRpLWRpZW5zdGUuZGUvb2NzcDASBgNVHRMBAf8ECDAGAQH/\n"
        "AgEAMA4GA1UdDwEB/wQEAwIBBjAVBgNVHSAEDjAMMAoGCCqCFABMBIEjMFsGA1Ud\n"
        "EQRUMFKgUAYDVQQKoEkMR2dlbWF0aWsgR2VzZWxsc2NoYWZ0IGbDvHIgVGVsZW1h\n"
        "dGlrYW53ZW5kdW5nZW4gZGVyIEdlc3VuZGhlaXRza2FydGUgbWJIMAoGCCqGSM49\n"
        "BAMCA0kAMEYCIQCprLtIIRx1Y4mKHlNngOVAf6D7rkYSa723oRyX7J2qwgIhAKPi\n"
        "9GSJyYp4gMTFeZkqvj8pcAqxNR9UKV7UYBlHrdxC\n"
        "-----END CERTIFICATE-----";

    static constexpr char cFdSigErp[] =
        "-----BEGIN CERTIFICATE-----\n"
        "MIICxDCCAmugAwIBAgIHAPPxMc6DxzAKBggqhkjOPQQDAjCBhDELMAkGA1UEBhMC\n"
        "REUxHzAdBgNVBAoMFmdlbWF0aWsgR21iSCBOT1QtVkFMSUQxMjAwBgNVBAsMKUtv\n"
        "bXBvbmVudGVuLUNBIGRlciBUZWxlbWF0aWtpbmZyYXN0cnVrdHVyMSAwHgYDVQQD\n"
        "DBdHRU0uS09NUC1DQTEwIFRFU1QtT05MWTAeFw0yMDEwMDcwMDAwMDBaFw0yNTA4\n"
        "MDcwMDAwMDBaMF4xCzAJBgNVBAYTAkRFMSYwJAYDVQQKDB1nZW1hdGlrIFRFU1Qt\n"
        "T05MWSAtIE5PVC1WQUxJRDEnMCUGA1UEAwweRVJQIFJlZmVyZW56ZW50d2lja2x1\n"
        "bmcgRkQgU2lnMFowFAYHKoZIzj0CAQYJKyQDAwIIAQEHA0IABEldYn6CK9ft8L8H\n"
        "MpJBRLSG852LwqbmFUkhbdsK1G4oBDYhAqB0IMyo+PJ3fUluggoAOHRDTPT0GR0W\n"
        "hqTFkFujgeswgegwOAYIKwYBBQUHAQEELDAqMCgGCCsGAQUFBzABhhxodHRwOi8v\n"
        "ZWhjYS5nZW1hdGlrLmRlL29jc3AvMA4GA1UdDwEB/wQEAwIHgDAhBgNVHSAEGjAY\n"
        "MAoGCCqCFABMBIFLMAoGCCqCFABMBIEjMB8GA1UdIwQYMBaAFCjw+OapyHfMQ0Xb\n"
        "mq7XOoOsDg+oMB0GA1UdDgQWBBTFOF7jC4He9P41tN2Egygarc9zrzAMBgNVHRMB\n"
        "Af8EAjAAMCsGBSskCAMDBCIwIDAeMBwwGjAYMAoMCEUtUmV6ZXB0MAoGCCqCFABM\n"
        "BIIDMAoGCCqGSM49BAMCA0cAMEQCIC3GhnWAejWO73WwEKDJXtuMCQNQemw/KljS\n"
        "eABzn0HdAiB+wSFkBccEME3+QjZEM0SJbQ02n+2KdU4piu2o0T35aA==\n"
        "-----END CERTIFICATE-----\n";


    static constexpr char cFdSigErpPrivateKey[] =
        "-----BEGIN PRIVATE KEY-----\n"
        "MIGVAgEAMBQGByqGSM49AgEGCSskAwMCCAEBBwR6MHgCAQEEIKd3WtkUFC5Z0NZl\n"
        "4MAjxjh69DnnYV5e03WbLFQoq9qDoAsGCSskAwMCCAEBB6FEA0IABEldYn6CK9ft\n"
        "8L8HMpJBRLSG852LwqbmFUkhbdsK1G4oBDYhAqB0IMyo+PJ3fUluggoAOHRDTPT0\n"
        "GR0WhqTFkFs=\n"
        "-----END PRIVATE KEY-----\n";


    static constexpr char cFsSigErpOcspUrl[] =
        "http://ehca-testref.sig-test.telematik-test:8080/status/ecc-ocsp";

    template<class RequestSenderMock>
    static std::shared_ptr<RequestSenderMock> createRequestSender()
    {
        const std::string tslContent = FileHelper::readFileAsString(std::string{TEST_DATA_DIR} + "/tsl/TSL_valid.xml");
        const std::string bnaContent = FileHelper::readFileAsString(std::string{TEST_DATA_DIR} + "/tsl/BNA_valid.xml");
        return std::make_shared<RequestSenderMock>(
            std::unordered_map<std::string, std::string>{
                {"http://download-ref.tsl.telematik-test:80/ECC/ECC-RSA_TSL-ref.xml", tslContent},
                {"https://download-ref.tsl.telematik-test:443/ECC/ECC-RSA_TSL-ref.sha2", Hash::sha256(tslContent)},
                {"https://download-testref.bnetzavl.telematik-test:443/BNA-TSL.xml", bnaContent},
                {"https://download-testref.bnetzavl.telematik-test:443/BNA-TSL.sha2", Hash::sha256(bnaContent)}});
    }
};

#endif
