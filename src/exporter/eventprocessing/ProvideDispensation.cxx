/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/eventprocessing/ProvideDispensation.hxx"
#include "exporter/Epa4AllTransformer.hxx"
#include "exporter/client/EpaMedicationClient.hxx"
#include "exporter/model/EpaOpProvideDispensationErpInputParameters.hxx"
#include "exporter/model/EpaOpRxDispensationErpOutputParameters.hxx"
#include "exporter/model/TaskEvent.hxx"
#include "shared/model/TelematikId.hxx"

#include <utility>

namespace eventprocessing
{

Outcome ProvideDispensation::process(gsl::not_null<IEpaMedicationClient*> client,
                                     const model::ProvideDispensationTaskEvent& erpEvent)
{
    return ProvideDispensation{std::move(client)}.doProcess(erpEvent);
}

ProvideDispensation::ProvideDispensation(gsl::not_null<IEpaMedicationClient*> medicationClient)
    : EventProcessingBase(std::move(medicationClient))
{
}

Outcome ProvideDispensation::doProcess(const model::ProvideDispensationTaskEvent& taskEvent)
{
    const auto& medicationDispenseBundle = taskEvent.getMedicationDispenseBundle();
    const auto& telematikIdFromJwt = taskEvent.getJwtPharmacyId();
    const auto& organizationNameFromJwt = taskEvent.getJwtPharmacyOrganizationName();
    const auto& organizationProfessionOidFromJwt = taskEvent.getJwtPharmacyProfessionOid();

    const auto& firstResource = medicationDispenseBundle.getResource(0);
    auto profileString = model::NumberAsStringParserDocument::getOptionalStringValue(
        firstResource, rapidjson::Pointer{model::resource::ElementName::path(model::resource::elements::meta,
                                                                             model::resource::elements::profile, 0)});
    Expect(profileString.has_value(), "missing meta/profile in MedicationDispenseBundle[0]");
    fhirtools::DefinitionKey definitionKey{*profileString};
    Expect(definitionKey.version.has_value(), "missing version in meta/profile in MedicationDispenseBundle[0]");

    const bool version14 = (*definitionKey.version >= fhirtools::FhirVersion{"1.4"});
    model::EPAOpProvideDispensationERPInputParameters providePrescriptionErpOp =
        version14
            ? Epa4AllTransformer::transformMedicationDispense(
                  medicationDispenseBundle, taskEvent.getPrescriptionId(), taskEvent.getMedicationRequestAuthoredOn(),
                  telematikIdFromJwt, organizationNameFromJwt, organizationProfessionOidFromJwt)
            : Epa4AllTransformer::transformMedicationDispenseWithKBVMedication(
                  medicationDispenseBundle, taskEvent.getPrescriptionId(), taskEvent.getMedicationRequestAuthoredOn(),
                  telematikIdFromJwt, organizationNameFromJwt, organizationProfessionOidFromJwt);

    auto response = mMedicationClient->sendProvideDispensation(taskEvent.getKvnr(),
                                                               providePrescriptionErpOp.serializeToJsonString());

    Outcome outcome = fromHttpStatus(response.httpStatus);

    switch (outcome)
    {
        case Outcome::Success: {
            model::EPAOpRxDispensationERPOutputParameters responseParameters(std::move(response.body));
            const auto issue = responseParameters.getOperationOutcomeIssue();
            switch (issue.detailsCode)
            {
                using enum model::EPAOperationOutcome::EPAOperationOutcomeCS;
                case MEDICATIONSVC_OPERATION_SUCCESS:
                    break;
                case MEDICATIONSVC_PRESCRIPTION_STATUS:
                case MEDICATIONSVC_PRESCRIPTION_NO_EXIST:
                    TLOG(WARNING) << "EPAOperationOutcome code: " << magic_enum::enum_name(issue.detailsCode);
                    break;
                case SVC_IDENTITY_MISMATCH:
                case MEDICATIONSVC_PRESCRIPTION_DUPLICATE:
                case MEDICATIONSVC_NO_VALID_STRUCTURE:
                case MEDICATIONSVC_MEDICATIONPLAN_NO_EXIST:
                case MEDICATIONSVC_PARAMETERS_REFERENCE_NO_EXIST:
                case MEDICATIONSCV_MEDICINAL_PRODUCT_PACKAGE_NOT_ALLOWED:
                case MEDICATIONSVC_DISPENSATION_NO_EXIST:
                case MEDICATIONSVC_DISPENSATION_STATUS:
                case GENERIC_OPERATION_OUTCOME_CODE:
                    TLOG(ERROR) << "unexpected operation outcome from Medication Client: "
                                << magic_enum::enum_name(issue.detailsCode);
                    outcome = Outcome::DeadLetter;
                    break;
            }

            break;
        }
        case Outcome::DeadLetter:
        case Outcome::Retry:
            // no audit event
            logFailure(response.httpStatus, std::move(response.body));
            break;
    }
    return outcome;
}

}
