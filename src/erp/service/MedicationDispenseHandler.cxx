/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/service/MedicationDispenseHandler.hxx"
#include "erp/database/Database.hxx"
#include "erp/model/MedicationDispense.hxx"
#include "erp/model/MedicationDispenseBundle.hxx"
#include "erp/model/MedicationDispenseId.hxx"
#include "erp/model/MedicationsAndDispenses.hxx"
#include "erp/server/context/SessionContext.hxx"
#include "erp/service/ErpRequestHandler.hxx"
#include "erp/service/MedicationDispenseHandlerBase.hxx"
#include "erp/util/search/UrlArguments.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/model/Bundle.hxx"
#include "shared/network/message/MimeType.hxx"
#include "shared/server/request/ServerRequest.hxx"
#include "shared/server/response/ServerResponse.hxx"
#include "shared/util/TLog.hxx"


using namespace model;


GetAllMedicationDispenseHandler::GetAllMedicationDispenseHandler(
    const std::initializer_list<std::string_view>& allowedProfessionOiDs) :
    MedicationDispenseHandlerBase(Operation::GET_MedicationDispense, allowedProfessionOiDs)
{
}

void GetAllMedicationDispenseHandler::handleRequest(PcSessionContext& session)
{
    TVLOG(1) << name() << ": processing request to " << session.request.header().target();

    A_19140.start("Retrieve dispensing information from the insured");

    // GEMREQ-start A_19406-01#getAll-1
    const auto& accessToken = session.request.getAccessToken();
    const auto kvnrClaim = accessToken.stringForClaim(JWT::idNumberClaim);
    ErpExpect(kvnrClaim.has_value(), HttpStatus::BadRequest, "KvNr not contained in JWT");
    const model::Kvnr kvnr{*kvnrClaim};
    // GEMREQ-end A_19406-01#getAll-1

    // GEMREQ-start A_19406-01#getAll-2
    A_19518.start("Search parameters for MedicationDispense");
    A_22070_02.start("Search parameter PrescriptionID for multiple medication dispenses per task");
    A_24438.start("Set whenhandedover as default sort argument.");
    auto arguments = std::optional<UrlArguments>(
        std::in_place,
        std::vector<SearchParameter>
    {
        { "performer", "performer", SearchParameter::Type::HashedIdentity },
        { "whenhandedover", "when_handed_over", SearchParameter::Type::Date },
        { "whenprepared", "when_prepared", SearchParameter::Type::Date },
        { "identifier", "prescription_id", SearchParameter::Type::PrescriptionId}
    }, "whenhandedover");
    A_24438.finish();
    arguments->parse(session.request, session.serviceContext.getKeyDerivation());
    // GEMREQ-end A_19406-01#getAll-2
    const auto identifierSearchArgument = arguments->getSearchArgument("identifier");

    A_24443.start("No pagination");
    arguments->disablePagingArgument();
    A_24443.finish();

    // GEMREQ-start A_19406-01#getAll-3
    A_19406_01.start("Filter MedicationDispense on KVNR of the insured");
    auto* databaseHandle = session.database();
    const auto medicationDispenses = databaseHandle->retrieveAllMedicationDispenses(kvnr, arguments);
    A_19406_01.finish();
    // GEMREQ-end A_19406-01#getAll-3
    A_22070_02.finish();
    A_19518.finish();

    // GEMREQ-start A_19406-01#getAll-4
    auto bundle = MedicationDispenseHandlerBase::createBundle(medicationDispenses);

    makeResponse(session, HttpStatus::OK, &bundle);
    // GEMREQ-end A_19406-01#getAll-4

    // Collect Audit data
    session.auditDataCollector()
        // leave the kvnr type unspecified, as there are multiple dispenses, the kvnr type might be inconsistent(?)
        .setInsurantKvnr(kvnr)
        .setAction(model::AuditEvent::Action::read)
        .setEventId(model::AuditEventId::GET_MedicationDispense);

    if (identifierSearchArgument.has_value() && identifierSearchArgument->valuesCount() == 1)
    {
        auto prescriptionId = identifierSearchArgument->valueAsPrescriptionId(0);
        session.auditDataCollector().setPrescriptionId(prescriptionId);
        session.accessLog.prescriptionId(prescriptionId);
    }
    A_19140.finish();
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

    // GEMREQ-start A_19406-01#getId-1
    const auto& accessToken = session.request.getAccessToken();
    const auto kvnrClaim = accessToken.stringForClaim(JWT::idNumberClaim);
    ErpExpect(kvnrClaim.has_value(), HttpStatus::BadRequest, "KvNr not contained in JWT");
    const model::Kvnr kvnr{*kvnrClaim};
    // GEMREQ-end A_19406-01#getId-1

    // GEMREQ-start A_19406-01#getId-2
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
    session.accessLog.prescriptionId(prescriptionId);
    // GEMREQ-end A_19406-01#getId-2

    // GEMREQ-start A_19406-01#getId-3
    A_19406_01.start("Filter MedicationDispense on KVNR of the insured");
    // No additional search parameters for GetMedicationDispenseById
    auto* databaseHandle = session.database();
    const auto [medicationDispenses, medications] = databaseHandle->retrieveMedicationDispense(kvnr, medicationDispenseId);
    A_19406_01.finish();

    ErpExpect(!medicationDispenses.empty(), HttpStatus::NotFound, "No Medication Dispense found for Id " + pathId.value());
    if (medications.empty())
    {
        makeResponse(session, HttpStatus::OK, &medicationDispenses.front());
    }
    else
    {
        model::MedicationDispenseBundle responseBundle{getLinkBase(), medicationDispenses, medications};
        makeResponse(session, HttpStatus::OK, &responseBundle);
    }
    // GEMREQ-end A_19406-01#getId-3

    // Collect Audit data
    session.auditDataCollector()
        .setInsurantKvnr(kvnr)
        .setAction(model::AuditEvent::Action::read)
        .setEventId(model::AuditEventId::GET_MedicationDispense_id)
        .setPrescriptionId(medicationDispenses.front().prescriptionId());

    A_19141.finish();
}
