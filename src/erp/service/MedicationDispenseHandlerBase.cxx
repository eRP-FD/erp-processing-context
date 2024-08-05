/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/service/MedicationDispenseHandlerBase.hxx"

#include "erp/ErpRequirements.hxx"
#include "erp/model/Bundle.hxx"
#include "erp/model/KbvMedicationBase.hxx"
#include "erp/model/MedicationDispense.hxx"
#include "erp/model/MedicationDispenseBundle.hxx"
#include "erp/model/MedicationDispenseId.hxx"
#include "erp/util/Demangle.hxx"

#include "fhirtools/model/erp/ErpElement.hxx"
#include "fhirtools/parser/FhirPathParser.hxx"

MedicationDispenseHandlerBase::MedicationDispenseHandlerBase(
    Operation operation, const std::initializer_list<std::string_view>& allowedProfessionOiDs)
    : ErpRequestHandler{operation, allowedProfessionOiDs}
{
}


std::vector<model::MedicationDispense> MedicationDispenseHandlerBase::medicationDispensesFromBody(PcSessionContext& session)
{
    try
    {
        A_22069.start("Detect input resource type: Bundle or MedicationDispense");
        auto unspec = createResourceFactory<model::UnspecifiedResource>(session);
        const auto resourceType = unspec.getResourceType();
        ErpExpect(resourceType == model::Bundle::resourceTypeName ||
        resourceType == model::MedicationDispense::resourceTypeName,
        HttpStatus::BadRequest, "Unsupported resource type in request body: " + std::string(resourceType));
        A_22069.finish();

        if (resourceType == model::Bundle::resourceTypeName)
        {
            auto bundleFactory = for_resource<model::MedicationDispenseBundle>(std::move(unspec));
            auto bundle = std::move(bundleFactory).getValidated(model::ProfileType::MedicationDispenseBundle);
            return bundle.getResourcesByType<model::MedicationDispense>();
        }
        else
        {
            auto dispenseFactory = for_resource<model::MedicationDispense>(std::move(unspec));
            std::vector<model::MedicationDispense> result;
            result.emplace_back(std::move(dispenseFactory).getValidated(model::ProfileType::Gem_erxMedicationDispense));
            return result;
        }
    }
    catch(const model::ModelException& e)
    {
        ErpFailWithDiagnostics(HttpStatus::BadRequest, "Error during request parsing.", e.what());
    }
}

void MedicationDispenseHandlerBase::checkMedicationDispenses(std::vector<model::MedicationDispense>& medicationDispenses,
                                const model::PrescriptionId& prescriptionId, const model::Kvnr& kvnr,
                                const std::string& telematikIdFromAccessToken)
{
    for (size_t i = 0, end = medicationDispenses.size(); i < end; ++i)
    {
        auto& medicationDispense = medicationDispenses[i];
        A_19248_02.start("Check provided MedicationDispense object, especially PrescriptionID, KVNR and TelematikID");

        // See https://simplifier.net/erezept-workflow/GemerxMedicationDispense/~details
        // The logical id of the resource, as used in the URL for the resource. Once assigned, this value never changes.
        // The only time that a resource does not have an id is when it is being submitted to the server using a create operation.
        // As medication dispenses are queried by "GET /MedicationDispense/{Id}" where Id equals the TaskId (= prescriptionId)
        // the Id of the medication dispense resource is set to the prescriptionId.
        // Check for correct MedicationDispense.identifier
        try
        {
            ErpExpect(medicationDispense.prescriptionId() == prescriptionId, HttpStatus::BadRequest,
                      "Prescription ID in MedicationDispense does not match the one in the task");
        }
        catch (const model::ModelException& exc)
        {
            TVLOG(1) << "ModelException: " << exc.what();
            ErpFail(HttpStatus::BadRequest, "Invalid Prescription ID in MedicationDispense (wrong content)");
        }
        medicationDispense.setId({prescriptionId, i});

        ErpExpect(medicationDispense.kvnr() == kvnr, HttpStatus::BadRequest,
                  "KVNR in MedicationDispense does not match the one in the task");

        ErpExpect(medicationDispense.telematikId() == telematikIdFromAccessToken, HttpStatus::BadRequest,
                  "Telematik-ID in MedicationDispense does not match the one in the access token.");
        A_19248_02.finish();
    }
}


model::Bundle MedicationDispenseHandlerBase::createBundle(
    const std::vector<model::MedicationDispense>& medicationDispenses)
{
    const std::string linkBase = getLinkBase() + "/MedicationDispense";
    model::Bundle bundle(model::BundleType::searchset, ::model::FhirResourceBase::NoProfile);
    for (const auto& medicationDispense : medicationDispenses)
    {
        const std::string urn = linkBase + "/" + medicationDispense.id().toString();
        bundle.addResource(
            urn,
            {},
            model::Bundle::SearchMode::match,
            medicationDispense.jsonDocument());
    }
    return bundle;
}
