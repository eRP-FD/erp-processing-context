/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/ErxReceipt.hxx"
#include "erp/model/MedicationsAndDispenses.hxx"
#include "erp/model/EuMedicationDispenseInfos.hxx"
#include "erp/model/eu/EuAccessPermission.hxx"
#include "erp/model/eu/GemErpEuPrParCloseOperationInput.hxx"
#include "erp/service/task/CloseTaskHandler.hxx"
#include "erp/service/task/DispenseTaskHandler.hxx"
#include "erp/service/task/eu/EuCloseTaskHandler.hxx"
#include "fhirtools/model/erp/ErpElement.hxx"
#include "fhirtools/parser/FhirPathParser.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/crypto/CadesBesSignature.hxx"
#include "shared/crypto/Sha256.hxx"
#include "shared/erp-serverinfo.hxx"
#include "shared/util/Base64.hxx"
#include "shared/util/ByteHelper.hxx"
#include "test/erp/service/EndpointHandlerTest/MedicationDispenseFixture.hxx"
#include "test/mock/MockDatabase.hxx"
#include "test/util/CertificateDirLoader.h"
#include "test/util/CryptoHelper.hxx"
#include "test/util/ErpMacros.hxx"
#include "test/util/JwtBuilder.hxx"
#include "test/util/ResourceManager.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test/util/StaticData.hxx"
#include "test/util/TestUtils.hxx"

class EuCloseTaskTest : public MedicationDispenseFixture
{
protected:
    // no need to shift. We are addressing the correct view by passing whenHandedOver
    testutils::ShiftFhirResourceViewsGuard shift{testutils::ShiftFhirResourceViewsGuard::asConfigured};
};

TEST_F(EuCloseTaskTest, test1)
{
    EnvironmentVariableGuard featureToggleGuard("ERP_FEATURE_EU", "true");


    eu::EuCloseTaskHandler handler({});

    const Header requestHeader{HttpMethod::POST,
                         "/Task/" + prescriptionId.toString() + "/$eu-close/",
                         0,
                         {{Header::ContentType, ContentMimeType::fhirXmlUtf8}},
                         HttpStatus::Unknown};

    const auto jwt = mJwtBuilder->makeJwtNcpeh();
    const auto accessCode = "ABC123";

    ServerRequest serverRequest{requestHeader};
    serverRequest.setAccessToken(jwt);
    serverRequest.setQueryParameters({{"secret", "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"}});
    serverRequest.setPathParameters({"id"}, {prescriptionId.toString()});

    auto xmlStr = ResourceTemplates::euCloseTaskXml({
        .kvnr = "X234567891",
        .prescriptionId = "160.000.000.004.715.74",
        .whenHandedOver = "2025-10-01",
        .accessCode = accessCode,
        .countryCode = "BE",
        .medicationDispenseId = "Example"
    });
    std::optional<model::GemErpEuPrParCloseOperationInput> resource;
    ASSERT_NO_THROW(resource =
                        model::GemErpEuPrParCloseOperationInput::fromXml(xmlStr, *StaticData::getXmlValidator()));
    ASSERT_TRUE(resource);

    serverRequest.setAccessToken(jwt);
    serverRequest.setBody(xmlStr);

    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    {
        auto database = mServiceContext.databaseFactory();
        const auto kvnr = resource->kvnr();
        database->storeConsent(
            model::Consent{model::ConsentType::EUDISPCONS, model::Kvnr{kvnr}, model::Timestamp::now()});
        database->commitTransaction();
    }

    {
        auto database = mServiceContext.databaseFactory();
        const auto kvnr = resource->kvnr();
        auto accessCode = resource->accessCode();
        auto cc = resource->countryCode();
        database->createEuAccessPermission(
            model::Kvnr{kvnr},
            model::EuAccessPermission{model::EuAccessCode{SafeString{std::move(accessCode.toString())}},
                                      model::CountryCode{cc}});
        database->commitTransaction();
    }

    ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
    ASSERT_NO_THROW(handler.handleRequest(sessionContext)) << serverRequest.getBody();
    ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);

    {
        const auto kvnr = resource->kvnr();
        const auto medicationsAndDispenses = sessionContext.database()->retrieveAllMedicationDispenses(model::Kvnr{"X234567891"}, std::nullopt);

        EXPECT_TRUE(std::get<0>(medicationsAndDispenses).medicationDispenses.size());
        const auto& md = std::get<0>(medicationsAndDispenses).medicationDispenses.front();

        ASSERT_TRUE(md.performerReference().has_value());
        const auto& ref = std::string{md.performerReference().value()};

        const Uuid urn{ref};
        const auto& id = urn.toString();

        EXPECT_EQ(std::get<1>(medicationsAndDispenses).size(), 1);

        const model::EuMedicationDispenseInfos& euInfos = std::get<1>(medicationsAndDispenses).back();

        EXPECT_STREQ(std::string{euInfos.practitionerRole().getId().value()}.c_str(), id.c_str());

        const Uuid practitionerUrn{euInfos.practitionerRole().practitionerReference()};
        EXPECT_STREQ(std::string{euInfos.practitioner().getId().value()}.c_str(), practitionerUrn.toString().c_str());

        const Uuid organizationUrn{euInfos.practitionerRole().organizationReference()};
        EXPECT_STREQ(std::string{euInfos.organization().getId().value()}.c_str(), organizationUrn.toString().c_str());
    }
}
