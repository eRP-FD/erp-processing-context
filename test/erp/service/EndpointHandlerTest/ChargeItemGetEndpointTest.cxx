#include "EndpointHandlerTest.hxx"
#include "erp/service/chargeitem/ChargeItemGetHandler.hxx"
#include "erp/util/Demangle.hxx"
#include "fhirtools/model/Collection.hxx"
#include "fhirtools/model/erp/ErpElement.hxx"
#include "fhirtools/parser/FhirPathParser.hxx"
#include "test/util/StaticData.hxx"

class ChargeItemGetEndpointTest : public EndpointHandlerTest {

    void SetUp() override
    {
        if (deprecatedBundle(model::ResourceVersion::currentBundle()))
        {
            GTEST_SKIP();
        }
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
    const auto& repo = Fhir::instance().structureRepository(model::ResourceVersion::currentBundle());
    const auto parse = [&repo](std::string_view expr) {
        return fhirtools::FhirPathParser::parse(&repo, expr);
    };
    using BundleFactory = model::ResourceFactory<model::Bundle>;
    BundleFactory::Options factoryOptions{.validatorOptions = fhirtools::ValidatorOptions{
                                              .allowNonLiteralAuthorReference = true,
                                          }};
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
    std::optional<model::Bundle> bundle;
    ASSERT_NO_THROW(auto bundleFactory = BundleFactory::fromJson(serverResponse.getBody(),
                                                                 *StaticData::getJsonValidator(), factoryOptions);
                    bundle.emplace(std::move(bundleFactory)
                                       .getValidated(SchemaType::fhir, *StaticData::getXmlValidator(),
                                                     *StaticData::getInCodeValidator()));)
        << serverResponse.getBody();
    fhirtools::Collection bundleCollection = {std::make_shared<ErpElement>(
        &repo, std::weak_ptr<fhirtools::Element>{}, "Bundle", std::addressof(bundle->jsonDocument()))};
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
