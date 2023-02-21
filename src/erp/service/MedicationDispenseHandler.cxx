/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/service/MedicationDispenseHandler.hxx"

#include "erp/database/Database.hxx"
#include "erp/model/Bundle.hxx"
#include "erp/model/MedicationDispense.hxx"
#include "erp/model/MedicationDispenseId.hxx"
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
    const auto kvnrClaim = accessToken.stringForClaim(JWT::idNumberClaim);
    ErpExpect(kvnrClaim.has_value(), HttpStatus::BadRequest, "KvNr not contained in JWT");
    const model::Kvnr kvnr{*kvnrClaim};

    A_19518.start("Search parameters for MedicationDispense");
    A_22070.start("Search parameter PrescriptionID for multiple medication dispenses per task");
    auto arguments = std::optional<UrlArguments>(
        std::in_place,
        std::vector<SearchParameter>
    {
        { "performer", "performer", SearchParameter::Type::HashedIdentity },
        { "whenhandedover", "when_handed_over", SearchParameter::Type::Date },
        { "whenprepared", "when_prepared", SearchParameter::Type::Date },
        { "identifier", "prescription_id", SearchParameter::Type::PrescriptionId}
    });
    arguments->parse(session.request, session.serviceContext.getKeyDerivation());
    const auto identifierSearchArgument = arguments->getSearchArgument("identifier");
    if (identifierSearchArgument.has_value())
    {
        for (size_t i = 0; i < identifierSearchArgument->valuesCount(); ++i)
        {
            checkFeatureWf200(identifierSearchArgument->valueAsPrescriptionId(i).type());
        }
    }

    A_19406.start("Filter MedicationDispense on KVNR of the insured");
    auto* databaseHandle = session.database();
    const auto [medicationDispenses, hasNextPage] = databaseHandle->retrieveAllMedicationDispenses(kvnr, arguments);
    A_19406.finish();
    A_22070.finish();
    A_19518.finish();

    auto bundle = createBundle(medicationDispenses);

    // We do no longer fill the Bundle.total field, because we cannot calculate it accurately due to possibly
    // multiple medication dispenses per task. The Bundle.total is optional,
    // see https://www.hl7.org/fhir/search.html#total

    const auto links = arguments->getBundleLinks(hasNextPage, getLinkBase(), "/MedicationDispense");
    for (const auto& link : links)
    {
        bundle.setLink(link.first, link.second);
    }

    makeResponse(session, HttpStatus::OK, &bundle);

    // Collect Audit data
    session.auditDataCollector()
        // leave the kvnr type unspecified, as there are multiple dispenses, the kvnr type might be inconsistent(?)
        .setInsurantKvnr(kvnr)
        .setAction(model::AuditEvent::Action::read)
        .setEventId(model::AuditEventId::GET_MedicationDispense);

    if (identifierSearchArgument.has_value() && identifierSearchArgument->valuesCount() == 1)
    {
        session.auditDataCollector().setPrescriptionId(identifierSearchArgument->valueAsPrescriptionId(0));
    }
    A_19140.finish();
}

Bundle GetAllMedicationDispenseHandler::createBundle(
    const std::vector<MedicationDispense>& medicationDispenses)
{
    const std::string linkBase = getLinkBase() + "/MedicationDispense";
    Bundle bundle(BundleType::searchset, ::model::ResourceBase::NoProfile);
    for (const auto& medicationDispense : medicationDispenses)
    {
        const std::string urn = linkBase + "/" + medicationDispense.id().toString();
        bundle.addResource(
            urn,
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
    const auto kvnrClaim = accessToken.stringForClaim(JWT::idNumberClaim);
    ErpExpect(kvnrClaim.has_value(), HttpStatus::BadRequest, "KvNr not contained in JWT");
    const model::Kvnr kvnr{*kvnrClaim};

    const std::optional<std::string> pathId = session.request.getPathParameter("id");
    ErpExpect(pathId.has_value(), HttpStatus::BadRequest, "id path parameter is missing");

    const auto medicationDispenseId = [&pathId] {
        try
        {
            return model::MedicationDispenseId::fromString(pathId.value());
        }
        catch (const ModelException& modelException)
        {
            ErpFailWithDiagnostics(HttpStatus::BadRequest, "bad path-id parameter", modelException.what());
        }
    }();

    const auto& prescriptionId = medicationDispenseId.getPrescriptionId();
    checkFeatureWf200(prescriptionId.type());
    session.accessLog.prescriptionId(prescriptionId.toString());

    A_19406.start("Filter MedicationDispense on KVNR of the insured");
    // No additional search parameters for GetMedicationDispenseById
    auto* databaseHandle = session.database();
    const auto medicationDispense = databaseHandle->retrieveMedicationDispense(kvnr, medicationDispenseId);
    A_19406.finish();

    ErpExpect(medicationDispense.has_value(), HttpStatus::NotFound,
              "No Medication Dispense found for Id " + pathId.value());
    makeResponse(session, HttpStatus::OK, &medicationDispense.value());

    // Collect Audit data
    session.auditDataCollector()
        .setInsurantKvnr(kvnr)
        .setAction(model::AuditEvent::Action::read)
        .setEventId(model::AuditEventId::GET_MedicationDispense_id)
        .setPrescriptionId(medicationDispense->prescriptionId());

    A_19141.finish();
}
