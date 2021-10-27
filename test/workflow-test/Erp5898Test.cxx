/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/client/request/ClientRequestWriter.hxx"
#include "erp/client/request/ValidatedClientRequest.hxx"
#include "erp/crypto/AesGcm.hxx"
#include "erp/crypto/SecureRandomGenerator.hxx"
#include "erp/util/ByteHelper.hxx"
#include "test/workflow-test/ErpWorkflowTestFixture.hxx"

class Erp5898Test : public ErpWorkflowTest
{
public:
    enum class mode
    {
        mode1,
        mode2
    };
    mode mOde = mode::mode1;

    // inject a invalid/missing JWT
    std::string creatTeeRequest(const Certificate& serverPublicKeyCertificate, const ClientRequest& request,
                                const JWT&) override
    {
        std::string jwt;
        switch (mOde)
        {
            case mode::mode1:
                jwt = " ";
                break;
            case mode::mode2:
                break;
        }
        auto mRequestIdInRequest = String::toLower(ByteHelper::toHex(SecureRandomGenerator::generate(128 / 8)));
        auto mResponseSymmetricKey = SecureRandomGenerator::generate(AesGcm128::KeyLength);
        const auto aesKey = String::toLower(ByteHelper::toHex(mResponseSymmetricKey));
        SafeString p("1 " + jwt + mRequestIdInRequest + " " + aesKey + " " +
                     ClientRequestWriter(ValidatedClientRequest(request)).toString());
        return teeProtocol.encryptInnerRequest(serverPublicKeyCertificate, p);
    }
};

TEST_F(Erp5898Test, run)
{
    mOde = Erp5898Test::mode::mode1;
    taskCreate(HttpStatus::BadRequest, HttpStatus::Unknown);
    mOde = Erp5898Test::mode::mode2;
    taskCreate(HttpStatus::Unauthorized, HttpStatus::Unknown);
}
