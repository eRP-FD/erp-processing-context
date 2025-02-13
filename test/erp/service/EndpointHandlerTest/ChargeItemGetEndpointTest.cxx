#include "EndpointHandlerTestFixture.hxx"
#include "erp/service/chargeitem/ChargeItemGetHandler.hxx"
#include "shared/util/Demangle.hxx"
#include "fhirtools/model/Collection.hxx"
#include "fhirtools/model/erp/ErpElement.hxx"
#include "fhirtools/parser/FhirPathParser.hxx"
#include "test/util/JwtBuilder.hxx"
#include "test/util/StaticData.hxx"
#include "test/util/TestUtils.hxx"

class ChargeItemGetEndpointTest : public EndpointHandlerTest {

    void SetUp() override
    {
        EndpointHandlerTest::SetUp();
    }
};

TEST_F(ChargeItemGetEndpointTest, GetByIdNonPkvFails)
{
    ChargeItemGetByIdHandler handler{{}};
    auto prescriptionId = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4711);
    Header requestHeader{HttpMethod::GET, "/chargeitem/" + prescriptionId.toString(), 0, {}, HttpStatus::Unknown};
    ServerRequest serverRequest{std::move(requestHeader)};
    serverRequest.setPathParameters({"id"}, {prescriptionId.toString()});
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext(mServiceContext, serverRequest, serverResponse, accessLog);
    ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
    try
    {
        handler.handleRequest(sessionContext);
        ADD_FAILURE() << "Expected ErpException";
    }
    catch (const ErpException& ex)
    {
        EXPECT_EQ(ex.status(), HttpStatus::BadRequest);
        EXPECT_STREQ(ex.what(), "Attempt to access charge item for non-PKV Prescription.");
    }
    catch (const std::exception& ex)
    {
        ADD_FAILURE() << "Unexpected exception: " << util::demangle(typeid(ex).name()) << ": " << ex.what();
    }

}

TEST_F(ChargeItemGetEndpointTest, SignatureWho)
{
    using namespace std::string_literals;
    gsl::not_null defaultView = Fhir::instance().backend().defaultView();
    const auto parse = [&defaultView](std::string_view expr) {
        return fhirtools::FhirPathParser::parse(defaultView.get().get(), expr);
    };
    auto linkBase = Configuration::instance().getStringValue(ConfigurationKey::PUBLIC_E_PRESCRIPTION_SERVICE_URL);
    auto expectFullUrl = linkBase + "/Device/1";
    ChargeItemGetByIdHandler handler{{}};
    auto prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50000);
    Header requestHeader{HttpMethod::GET, "/chargeitem/" + prescriptionId.toString(), 0, {}, HttpStatus::Unknown};
    ServerRequest serverRequest{std::move(requestHeader)};
    serverRequest.setPathParameters({"id"}, {prescriptionId.toString()});
    serverRequest.setAccessToken(JwtBuilder::testBuilder().makeJwtVersicherter("X500000056"));
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext(mServiceContext, serverRequest, serverResponse, accessLog);
    ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
    ASSERT_NO_THROW(handler.handleRequest(sessionContext));
    ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);
    std::optional<model::UnspecifiedResource> unspec;
    ASSERT_NO_THROW(unspec.emplace(model::UnspecifiedResource::fromJsonNoValidation(serverResponse.getBody()))) << serverResponse.getBody();
    ASSERT_NO_THROW(testutils::bestEffortValidate(unspec.value())) << serverResponse.getBody();
    const auto bundle = model::Bundle::fromJson(std::move(unspec).value().jsonDocument());

    fhirtools::Collection bundleCollection = {std::make_shared<ErpElement>(
        defaultView, std::weak_ptr<fhirtools::Element>{}, "Bundle", std::addressof(bundle.jsonDocument()))};
    const auto supportingInformationExpr = parse("Bundle.entry.resource.ofType(ChargeItem).supportingInformation");
    const auto supportingInformation = supportingInformationExpr->eval(bundleCollection);
    ASSERT_EQ(supportingInformation.size(), 3);
    const auto dispenseItemExpr =
        parse("where(display = '"s.append(model::resource::structure_definition::dispenseItem).append("').resolve()"));
    const auto dispenseItem = dispenseItemExpr->eval(supportingInformation);
    ASSERT_EQ(dispenseItem.size(), 1);
    const auto receiptExpr =
        parse("where(display = '"s.append(model::resource::structure_definition::receipt).append("').resolve()"));
    const auto receipt = receiptExpr->eval(supportingInformation);
    ASSERT_EQ(receipt.size(), 1);

    const auto signatureWhoExpr = parse("signature.who.reference");
    const auto dispenseItemSigner = signatureWhoExpr->eval(dispenseItem);
    ASSERT_EQ(dispenseItemSigner.size(), 1);
    EXPECT_EQ(dispenseItemSigner.front()->asString(), expectFullUrl);

    const auto receiptSigner = signatureWhoExpr->eval(receipt);
    ASSERT_EQ(receiptSigner.size(), 1);
    EXPECT_EQ(receiptSigner.front()->asString(), expectFullUrl);
}
