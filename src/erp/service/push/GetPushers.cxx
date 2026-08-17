/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/service/push/GetPushers.hxx"
#include "erp/database/push/PushErpDatabase.hxx"
#include "erp/model/push/ErrorResponse.hxx"
#include "erp/server/context/SessionContext.hxx"
#include "fhirtools/model/NumberAsStringParserWriter.hxx"
#include "shared/ErpRequirements.hxx"

Operation GetPushers::getOperation() const
{
    return Operation::GET_PUSHERS;
}

void GetPushers::handlePushersRequest(SessionContext& session)
{
    A_28530.start("App-Registrierungen abrufen - Filter auf KVNR des Versicherten");
    const model::Kvnr kvnr{session.kvnrFromAccessToken()};
    const auto registrations = session.pushDatabase()->getRegistrations(kvnr);

    rapidjson::Document resultDocument;
    resultDocument.SetObject();
    rapidjson::Value pushers(rapidjson::kArrayType);
    pushers.Reserve(gsl::narrow<rapidjson::SizeType>(registrations.size()), resultDocument.GetAllocator());
    for (const auto& registration : registrations)
    {
        rapidjson::Value registrationDocument(rapidjson::kObjectType);
        registration.addTo(registrationDocument, resultDocument.GetAllocator());
        pushers.PushBack(registrationDocument, resultDocument.GetAllocator());
    }
    resultDocument.AddMember("pushers", pushers, resultDocument.GetAllocator());
    rapidjson::StringBuffer buffer;
    rapidjson::Writer writer(buffer);
    Expect3(resultDocument.Accept(writer), "Could not serialize Pusher to payload json", std::logic_error);
    makeResponse(session, HttpStatus::OK, buffer.GetString());
}
