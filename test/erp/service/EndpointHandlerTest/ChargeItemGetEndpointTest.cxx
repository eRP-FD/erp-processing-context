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

TEST_F(ChargeItemGetEndpointTest, SignatureWho)
{
    using namespace std::string_literals;
    const auto* backend = std::addressof(Fhir::instance().backend());
    gsl::not_null defaultView = Fhir::instance().defaultView();
    const auto parse = [&backend](std::string_view expr) {
        return fhirtools::FhirPathParser::parse(backend, expr);
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
    ASSERT_NO_THROW(testutils::validate(unspec.value())) << serverResponse.getBody();
    const auto bundle = model::Bundle::fromJson(std::move(unspec).value().jsonDocument());

    fhirtools::EvaluationContext bundleContext{
        defaultView, std::make_shared<ErpElement>(backend, std::weak_ptr<fhirtools::Element>{}, "Bundle",
                                                  std::addressof(bundle.jsonDocument()))};
    const auto supportingInformationExpr = parse("Bundle.entry.resource.ofType(ChargeItem).supportingInformation");
    const auto supportingInformation = supportingInformationExpr->eval(bundleContext);
    ASSERT_EQ(supportingInformation.collection.size(), 3);
    const auto dispenseItemExpr =
        parse("where(display = '"s.append(model::resource::structure_definition::dispenseItem).append("').resolve()"));
    const auto dispenseItem = dispenseItemExpr->eval(supportingInformation);
    ASSERT_EQ(dispenseItem.collection.size(), 1);
    const auto receiptExpr =
        parse("where(display = '"s.append(model::resource::structure_definition::receipt).append("').resolve()"));
    const auto receipt = receiptExpr->eval(supportingInformation);
    ASSERT_EQ(receipt.collection.size(), 1);

    const auto signatureWhoExpr = parse("signature.who.reference");
    const auto dispenseItemSigner = signatureWhoExpr->eval(dispenseItem).collection;
    ASSERT_EQ(dispenseItemSigner.size(), 1);
    EXPECT_EQ(dispenseItemSigner.front()->asString(), expectFullUrl);

    const auto receiptSigner = signatureWhoExpr->eval(receipt).collection;
    ASSERT_EQ(receiptSigner.size(), 1);
    EXPECT_EQ(receiptSigner.front()->asString(), expectFullUrl);
}
