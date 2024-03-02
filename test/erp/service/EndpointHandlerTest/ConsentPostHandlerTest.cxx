/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */


#include "erp/service/consent/ConsentPostHandler.hxx"
#include "test/erp/service/EndpointHandlerTest/EndpointHandlerTest.hxx"

class ConsentPostHandlerTest : public EndpointHandlerTest
{
};

TEST_F(ConsentPostHandlerTest, WrongProfileErp12831)
{
    const auto* resource = R"__({
"resourceType": "Consent",
"meta": {
  "profile": [
    "https://gematik.de/fhir/StructureDefinition/ErxConsent"
  ]
},
"status": "active",
"scope": {
  "coding": [
    {
      "code": "patient-privacy",
      "system": "http://terminology.hl7.org/CodeSystem/consentscope",
      "display": "Privacy Consent"
    }
  ]
},
"category": [
  {
    "coding": [
      {
        "code": "CHARGCONS",
        "system": "https://gematik.de/fhir/CodeSystem/Consenttype"
      }
    ]
  }
],
"patient": {
  "identifier": {
    "system": "http://fhir.de/sid/pkv/kvid-10",
    "value": "X764228538"
  }
},
"dateTime": "2023-01-30T14:18:58.004"
})__";


    Header requestHeader{HttpMethod::POST,
                         "/Consent",
                         0,
                         {{Header::ContentType, ContentMimeType::fhirJsonUtf8}},
                         HttpStatus::Unknown};
    ServerRequest serverRequest{std::move(requestHeader)};
    serverRequest.setBody(resource);
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    ConsentPostHandler consentPostHandler({});
    EXPECT_ERP_EXCEPTION(consentPostHandler.handleRequest(sessionContext), HttpStatus::BadRequest);
}
