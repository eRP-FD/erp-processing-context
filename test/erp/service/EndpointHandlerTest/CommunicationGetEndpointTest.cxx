#include "test/erp/service/EndpointHandlerTest/EndpointHandlerTest.hxx"

#include "erp/service/CommunicationGetHandler.hxx"
#include "erp/model/ResourceNames.hxx"
#include "test/util/JsonTestUtils.hxx"
#include "test/util/JwtBuilder.hxx"
#include "test/util/ResourceTemplates.hxx"


class CommunicationGetEndpointTest : public EndpointHandlerTest {

};


TEST_F(CommunicationGetEndpointTest, Erp21377_oldKvnrNamespaceInDB)
{
    const model::Kvnr kvnr{"X500000000", model::Kvnr::Type::gkv};
    const auto prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4711);
    auto jwt = mJwtBuilder->makeJwtVersicherter(kvnr);
    ServerRequest serverRequest{Header{HttpMethod::GET,
                                       "/Communication",
                                       Header::Version_1_0,
                                       {{Header::ContentType, ContentMimeType::fhirJsonUtf8}},
                                       HttpStatus::Unknown}};
    serverRequest.setAccessToken(jwt);
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    auto commStr = CommunicationJsonStringBuilder{model::Communication::MessageType::DispReq,
        ResourceTemplates::Versions::GEM_ERP_1_2}
        .setPrescriptionId(prescriptionId.toString())
        .setSender(ActorRole::Insurant, kvnr.id())
        .setRecipient(ActorRole::Pharmacists, "Pharmacists-X")
        .setTimeSent(model::Timestamp::now().toXsDateTime())
        .setPayload(R"({"version":1,"supplyOptionsType":"onPremise","address":["zu hause"]})")
                        .createJsonString();
    auto pos = std::ranges::search(commStr, model::resource::naming_system::gkvKvid10);
    ASSERT_FALSE(pos.empty());
    commStr.replace(pos.begin(), pos.end(), "http://some/arbitrary/other/system");
    auto comm = model::Communication::fromJsonNoValidation(commStr);
    auto* db = sessionContext.database();
    db->insertCommunication(comm);
    CommunicationGetAllHandler handler{{}};
    ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
    ASSERT_NO_THROW(handler.handleRequest(sessionContext));
}
