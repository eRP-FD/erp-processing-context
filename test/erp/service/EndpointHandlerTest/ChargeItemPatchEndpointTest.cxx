#include "EndpointHandlerTestFixture.hxx"
#include "erp/service/chargeitem/ChargeItemPatchHandler.hxx"
#include "shared/util/Demangle.hxx"
#include "test/util/JwtBuilder.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test/util/StaticData.hxx"
#include "test/util/TestUtils.hxx"


class ChargeItemPatchEndpointTest : public EndpointHandlerTest
{
public:
};

struct ChargeItemPatchEndpointTestParam {
    std::string shiftTo;
    std::optional<ResourceTemplates::Versions::GEM_ERPCHRG> version;
    std::optional<bool> insuranceProviderMarking;
    std::optional<bool> subsidyMarking;
    std::optional<bool> taxOfficeMarking;
    bool expectSuccess;
    friend std::ostream& operator<<(std::ostream& os, const ChargeItemPatchEndpointTestParam& param)
    {
        auto version = param.version?to_string(*param.version):"n/a";
        return os << "shiftTo: " << param.shiftTo << ", version: " << version << ", insuranceProvider:"
                  << (param.insuranceProviderMarking.has_value() ? std::to_string(*param.insuranceProviderMarking)
                                                                  : "nullopt")
                  << ", subsidy:"
                  << (param.subsidyMarking.has_value() ? std::to_string(*param.subsidyMarking) : "nullopt")
                  << ", taxOffice:"
                  << (param.taxOfficeMarking.has_value() ? std::to_string(*param.taxOfficeMarking) : "nullopt")
                  << ", expectSuccess: " << param.expectSuccess;
    }
};

class ChargeItemPatchEndpointTestP : public EndpointHandlerTest,
                                     public testing::WithParamInterface<ChargeItemPatchEndpointTestParam>
{
public:
    void SetUp() override
    {
        EndpointHandlerTest::SetUp();
        if (! GetParam().shiftTo.empty())
        {
            shiftGuard = testutils::ShiftFhirResourceViewsGuard{
                GetParam().shiftTo, floor<std::chrono::days>(model::Timestamp::now().toChronoTimePoint())};
        }
    }

    std::string patchChargeItemJson()
    {
        if (GetParam().version)
        {
            return ResourceTemplates::patchChargeItemJson({
                .version = *GetParam().version,
                .insuranceProviderMarking = GetParam().insuranceProviderMarking,
                .subsidyMarking = GetParam().subsidyMarking,
                .taxOfficeMarking = GetParam().taxOfficeMarking,
            });
        }
        return ResourceTemplates::legacyPatchChargeItemBodyJson({
            .insuranceProviderMarking = GetParam().insuranceProviderMarking,
            .subsidyMarking = GetParam().subsidyMarking,
            .taxOfficeMarking = GetParam().taxOfficeMarking,
        });
    }

    std::optional<testutils::ShiftFhirResourceViewsGuard> shiftGuard;
};

TEST_P(ChargeItemPatchEndpointTestP, ChargeItemPatchEndpointTest)
{
    auto jsonBody = patchChargeItemJson();

    ChargeItemPatchHandler handler{{}};
    auto prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50023);
    Header requestHeader{HttpMethod::PATCH,
                         "/chargeitem/" + prescriptionId.toString(),
                         0,
                         {{Header::ContentType, ContentMimeType::fhirJsonUtf8}},
                         HttpStatus::Unknown};
    ServerRequest serverRequest{std::move(requestHeader)};
    serverRequest.setBody(std::move(jsonBody));
    serverRequest.setPathParameters({"id"}, {prescriptionId.toString()});
    serverRequest.setAccessToken(JwtBuilder::testBuilder().makeJwtVersicherter("X500000056"));
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext(mServiceContext, serverRequest, serverResponse, accessLog);
    ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));

    try
    {
        handler.handleRequest(sessionContext);
        if (! GetParam().expectSuccess)
        {
            ADD_FAILURE() << "Expected ErpException";
        }
        else
        {
            EXPECT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);
            auto chargeItem = model::ChargeItem::fromJson(serverResponse.getBody(), *StaticData::getJsonValidator());
            auto markingFlags = chargeItem.markingFlags();
            ASSERT_TRUE(markingFlags.has_value());
            auto allMarkings = markingFlags->getAllMarkings();
            ASSERT_EQ(allMarkings.size(), 3);
            EXPECT_EQ(allMarkings.at("insuranceProvider"), GetParam().insuranceProviderMarking.value_or(true));
            EXPECT_EQ(allMarkings.at("subsidy"), GetParam().subsidyMarking.value_or(false));
            EXPECT_EQ(allMarkings.at("taxOffice"), GetParam().taxOfficeMarking.value_or(false));
        }
    }
    catch (const ErpException& ex)
    {
        if (! GetParam().expectSuccess)
        {
            EXPECT_EQ(ex.status(), HttpStatus::BadRequest);
        }
        else
        {
            ADD_FAILURE() << "Unexpected ErpException: " << ex.what();
        }
    }
    catch (const std::exception& ex)
    {
        ADD_FAILURE() << "Unexpected exception: " << util::demangle(typeid(ex).name()) << ": " << ex.what();
    }
}

INSTANTIATE_TEST_SUITE_P(
    ChargeItemPatchEndpointTest, ChargeItemPatchEndpointTestP,
    testing::ValuesIn<std::list<ChargeItemPatchEndpointTestParam>>({
        {"", std::nullopt, true, true, true, true},
        {"", std::nullopt, std::nullopt, std::nullopt, std::nullopt, false},
        {"", std::nullopt, false, false, false, true},
        {"", std::nullopt, true, false, std::nullopt, true},
        {"", std::nullopt, true, true, true, true},
        {"", ResourceTemplates::Versions::GEM_ERPCHRG_1_1_0, true, true, true, true},
        {"", ResourceTemplates::Versions::GEM_ERPCHRG_1_1_0, std::nullopt, std::nullopt, std::nullopt, false},
        {"", ResourceTemplates::Versions::GEM_ERPCHRG_1_1_0, false, false, false, true},
        {"", ResourceTemplates::Versions::GEM_ERPCHRG_1_1_0, false, std::nullopt, true, true},
    }));
