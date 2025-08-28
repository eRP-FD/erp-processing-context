/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */


#include "erp/service/consent/ConsentPostHandler.hxx"
#include "fhirtools/converter/FhirConverter.hxx"
#include "test/erp/service/EndpointHandlerTest/EndpointHandlerTestFixture.hxx"
#include "test/util/ErpMacros.hxx"
#include "test/util/JwtBuilder.hxx"
#include "test/util/ResourceTemplates.hxx"

#include <boost/algorithm/string.hpp>

class ConsentPostHandlerTest : public EndpointHandlerTest
{
};

TEST_F(ConsentPostHandlerTest, WrongProfileErp12831)
{
    auto consentJSON = ResourceTemplates::consentJson({
        .consentCategory = model::ConsentType::CHARGCONS,
        .kvnr = "X764228538",
    });
    boost::algorithm::replace_all(consentJSON, model::resource::structure_definition::consent,
                                  "https://gematik.de/fhir/StructureDefinition/ErxConsent");

    Header requestHeader{HttpMethod::POST,
                         "/Consent",
                         0,
                         {{Header::ContentType, ContentMimeType::fhirJsonUtf8}},
                         HttpStatus::Unknown};
    ServerRequest serverRequest{std::move(requestHeader)};
    serverRequest.setBody(consentJSON);
    serverRequest.setAccessToken(mJwtBuilder->makeJwtVersicherter("X764228538"));
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    ConsentPostHandler consentPostHandler({});
    EXPECT_ERP_EXCEPTION_WITH_DIAGNOSTICS(
        consentPostHandler.handleRequest(sessionContext), HttpStatus::BadRequest, "parsing / validation error",
        "Unsupported profile type: https://gematik.de/fhir/StructureDefinition/ErxConsent|" +
            ResourceTemplates::Versions::GEM_ERPCHRG_current().renderVersion());
}


TEST_F(ConsentPostHandlerTest, WrongProfileType)
{
    auto consentJSON = ResourceTemplates::consentJson({
        .consentCategory = model::ConsentType::CHARGCONS,
        .kvnr = "X764228538",
    });
    boost::algorithm::replace_all(consentJSON, model::resource::structure_definition::consent,
                                  model::resource::structure_definition::chargeItem);

    Header requestHeader{HttpMethod::POST,
                         "/Consent",
                         0,
                         {{Header::ContentType, ContentMimeType::fhirJsonUtf8}},
                         HttpStatus::Unknown};
    ServerRequest serverRequest{std::move(requestHeader)};
    serverRequest.setBody(consentJSON);
    serverRequest.setAccessToken(mJwtBuilder->makeJwtVersicherter("X764228538"));
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    ConsentPostHandler consentPostHandler({});
    EXPECT_ERP_EXCEPTION_WITH_DIAGNOSTICS(consentPostHandler.handleRequest(sessionContext), HttpStatus::BadRequest,
                                          "parsing / validation error",
                                          "Profile Not allowed on this endpoint: GEM_ERPCHRG_PR_ChargeItem");
}

TEST_F(ConsentPostHandlerTest, WrongType)
{
    auto chargeItemXML = ResourceTemplates::chargeItemXml({
        .kvnr = model::Kvnr{"X764228538", model::Kvnr::Type::pkv},
        .prescriptionId =
            model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 1000),
        .dispenseBundleBase64 = "SGFsbG8gV2VsdCE=",
        .operation = ResourceTemplates::ChargeItemOptions::OperationType::Post,
    });
    auto chargeItemJSON = Fhir::instance().converter().xmlStringToJson(chargeItemXML);
    Header requestHeader{HttpMethod::POST,
                         "/Consent",
                         0,
                         {{Header::ContentType, ContentMimeType::fhirJsonUtf8}},
                         HttpStatus::Unknown};
    ServerRequest serverRequest{std::move(requestHeader)};
    serverRequest.setBody(chargeItemJSON.serializeToJsonString());
    serverRequest.setAccessToken(mJwtBuilder->makeJwtVersicherter("X764228538"));
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    ConsentPostHandler consentPostHandler({});
    EXPECT_ERP_EXCEPTION_WITH_DIAGNOSTICS(consentPostHandler.handleRequest(sessionContext), HttpStatus::BadRequest,
                                          "parsing / validation error",
                                          "Profile Not allowed on this endpoint: GEM_ERPCHRG_PR_ChargeItem");
}
