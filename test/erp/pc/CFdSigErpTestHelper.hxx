#ifndef ERP_PROCESSING_CONTEXT_TEST_ERP_PC_CFDSIGERPTESTHELPER_HXX
#define ERP_PROCESSING_CONTEXT_TEST_ERP_PC_CFDSIGERPTESTHELPER_HXX

#include "shared/util/FileHelper.hxx"
#include "shared/util/Hash.hxx"
#include "test_config.h"
#include "test/util/ResourceManager.hxx"
#include "mock/util/MockConfiguration.hxx"

#include <unordered_map>
#include <memory>
#include <string>


class CFdSigErpTestHelper
{
public:
    static std::string cFdSigErpSigner()
    {
        return MockConfiguration::instance().getStringValue(MockConfigurationKey::MOCK_ID_FD_SIG_SIGNER_CERT);
    }

    static std::string cFdSigErp()
    {
        return MockConfiguration::instance().getStringValue(MockConfigurationKey::MOCK_ID_FD_SIG_CERT);
    }

    static std::string cFdSigErpPrivateKey()
    {
        return MockConfiguration::instance().getStringValue(MockConfigurationKey::MOCK_ID_FD_SIG_PRIVATE_KEY);
    }

    static std::string cFsSigErpOcspUrl()
    {
        return MockConfiguration::instance().getStringValue(MockConfigurationKey::MOCK_ID_FD_SIG_OCSP_URL);
    }

    template<class RequestSenderMock>
    static std::shared_ptr<RequestSenderMock> createRequestSender()
    {
        const std::string tslContent =
            ResourceManager::instance().getStringResource("test/generated_pki/tsl/TSL_valid.xml");
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
