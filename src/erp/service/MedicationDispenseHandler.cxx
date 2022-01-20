/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/service/MedicationDispenseHandler.hxx"

#include "erp/database/Database.hxx"
#include "erp/model/Bundle.hxx"
#include "erp/model/MedicationDispense.hxx"
#include "erp/server/context/ServiceContext.hxx"
#include "erp/server/context/SessionContext.hxx"
#include "erp/server/request/ServerRequest.hxx"
#include "erp/server/response/ServerResponse.hxx"
#include "erp/util/search/UrlArguments.hxx"
#include "erp/util/TLog.hxx"
#include "erp/common/MimeType.hxx"
#include "erp/ErpRequirements.hxx"
#include "erp/service/ErpRequestHandler.hxx"


using namespace model;


GetAllMedicationDispenseHandler::GetAllMedicationDispenseHandler(
    const std::initializer_list<std::string_view>& allowedProfessionOiDs) :
    ErpRequestHandler(Operation::GET_MedicationDispense, allowedProfessionOiDs)
{
}

void GetAllMedicationDispenseHandler::handleRequest(PcSessionContext& session)
{
    TVLOG(1) << name() << ": processing request to " << session.request.header().target();

    A_19140.start("Retrieve dispensing information from the insured");

    const auto& accessToken = session.request.getAccessToken();
    const auto kvnr = accessToken.stringForClaim(JWT::idNumberClaim);
    ErpExpect(kvnr.has_value(), HttpStatus::BadRequest, "KvNr not contained in JWT");

    A_19518.start("Search parameters for MedicationDispense");
    auto arguments = std::optional<UrlArguments>(
        std::in_place,
        std::vector<SearchParameter>
    {
        { "performer", "performer", SearchParameter::Type::HashedIdentity },
        { "whenhandedover", "when_handed_over", SearchParameter::Type::Date },
        { "whenprepared", "when_prepared", SearchParameter::Type::Date }
    });
    arguments->parse(session.request, session.serviceContext.getKeyDerivation());

    A_19406.start("Filter MedicationDispense on KVNR of the insured");
    auto databaseHandle = session.database();
    const auto medicationDispenses =
        databaseHandle->retrieveAllMedicationDispenses(kvnr.value(), {}, arguments);
    A_19406.finish();
    A_19518.finish();

    std::size_t totalSearchMatches =
        responseIsPartOfMultiplePages(arguments->pagingArgument(), medicationDispenses.size()) ?
        databaseHandle->countAllMedicationDispenses(kvnr.value(), arguments) : medicationDispenses.size();

    auto bundle = createBundle(medicationDispenses);
    bundle.setTotalSearchMatches(totalSearchMatches);    
    const auto links = arguments->getBundleLinks(getLinkBase(), "/MedicationDispense", totalSearchMatches);
    for (const auto& link : links)
    {
        bundle.setLink(link.first, link.second);
    }

    makeResponse(session, HttpStatus::OK, &bundle);

    // Collect Audit data
    session.auditDataCollector()
        .setInsurantKvnr(*kvnr)
        .setAction(model::AuditEvent::Action::read)
        .setEventId(model::AuditEventId::GET_MedicationDispense);

    A_19140.finish();
}

Bundle GetAllMedicationDispenseHandler::createBundle(
    const std::vector<MedicationDispense>& medicationDispenses)
{
    const std::string linkBase = getLinkBase() + "/MedicationDispense";
    Bundle bundle(BundleType::searchset, ::model::ResourceBase::NoProfile);
    for (const auto& medicationDispense : medicationDispenses)
    {
        std::optional<PrescriptionId> id = medicationDispense.id();
        ModelExpect(id.has_value(), "No Id assigned to medication dispense");
        bundle.addResource(
            linkBase + "/" + id.value().toString(),
            {},
            Bundle::SearchMode::match,
            medicationDispense.jsonDocument());
    }
    return bundle;
}


GetMedicationDispenseHandler::GetMedicationDispenseHandler(
    const std::initializer_list<std::string_view>& allowedProfessionOiDs) :
    ErpRequestHandler(Operation::GET_MedicationDispense_id, allowedProfessionOiDs)
{
}

void GetMedicationDispenseHandler::handleRequest(PcSessionContext& session)
{
    TVLOG(1) << name() << ": processing request to " << session.request.header().target();

    A_19141.start("Retrieve dispensing information for a single e-recipe");

    const auto& accessToken = session.request.getAccessToken();
    const auto kvnr = accessToken.stringForClaim(JWT::idNumberClaim);
    ErpExpect(kvnr.has_value(), HttpStatus::BadRequest, "KvNr not contained in JWT");

    const std::optional<std::string> pathId = session.request.getPathParameter("id");
    ErpExpect(pathId.has_value(), HttpStatus::BadRequest, "id path parameter is missing");
    PrescriptionId prescriptionId = PrescriptionId::fromString(pathId.value());

    session.accessLog.prescriptionId(pathId.value());

    A_19406.start("Filter MedicationDispense on KVNR of the insured");
    // No additional search parameters for GetMedicationDispenseById
    auto databaseHandle = session.database();
    const auto medicationDispenses =
        databaseHandle->retrieveAllMedicationDispenses(kvnr.value(), prescriptionId, {});
    A_19406.finish();

    // Please note that "retrieveAllMedicationDispenses" already throws an exception if more
    // than one result row is retrieved from the database.
    ErpExpect(!medicationDispenses.empty(), HttpStatus::NotFound,
              "No Medication Dispense found for Prescription Id " + pathId.value());
    makeResponse(session, HttpStatus::OK, &medicationDispenses.front());

    // Collect Audit data
    session.auditDataCollector()
        .setInsurantKvnr(*kvnr)
        .setAction(model::AuditEvent::Action::read)
        .setEventId(model::AuditEventId::GET_MedicationDispense_id)
        .setPrescriptionId(prescriptionId);

    A_19141.finish();
}
