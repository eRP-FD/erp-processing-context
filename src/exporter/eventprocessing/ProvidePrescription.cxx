/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/eventprocessing/ProvidePrescription.hxx"
#include "exporter/Epa4AllTransformer.hxx"
#include "exporter/client/EpaMedicationClient.hxx"
#include "exporter/model/EpaErrorType.hxx"
#include "exporter/model/EpaOpRxPrescriptionErpOutputParameters.hxx"
#include "exporter/model/TaskEvent.hxx"
#include "shared/model/Coding.hxx"
#include "shared/model/Parameters.hxx"

#include <utility>

namespace eventprocessing
{

Outcome eventprocessing::ProvidePrescription::process(gsl::not_null<IEpaMedicationClient*> client,
                                                      const model::ProvidePrescriptionTaskEvent& erpEvent)
{
    return ProvidePrescription{std::move(client)}.doProcess(erpEvent);
}

ProvidePrescription::ProvidePrescription(gsl::not_null<IEpaMedicationClient*> medicationClient)
    : EventProcessingBase(std::move(medicationClient))
{
}

Outcome ProvidePrescription::doProcess(const model::ProvidePrescriptionTaskEvent& taskEvent) const
{
    const auto& kbvBundle = taskEvent.getKbvBundle();
    const auto& telematikIdFromQes = taskEvent.getQesDoctorId();
    const auto& telematikIdFromJwt = taskEvent.getJwtDoctorId();
    const auto& organizationNameFromJwt = taskEvent.getJwtDoctorOrganizationName();
    const auto& organizationProfessionOidFromJwt = taskEvent.getJwtDoctorProfessionOid();
    auto providePrescriptionErpOp = Epa4AllTransformer::transformPrescription(
        kbvBundle, telematikIdFromQes, telematikIdFromJwt, organizationNameFromJwt, organizationProfessionOidFromJwt);

    auto response = mMedicationClient->sendProvidePrescription(taskEvent.getKvnr(),
                                                               providePrescriptionErpOp.serializeToJsonString());

    Outcome outcome = fromHttpStatus(response.httpStatus);

    switch (outcome)
    {
        case Outcome::Success: {
            model::EPAOpRxPrescriptionERPOutputParameters responseParameters(std::move(response.body));
            const auto issue = responseParameters.getOperationOutcomeIssue();
            switch (issue.detailsCode)
            {
                using enum model::EPAOperationOutcome::EPAOperationOutcomeCS;
                case MEDICATIONSVC_OPERATION_SUCCESS:
                    break;
                case MEDICATIONSVC_PRESCRIPTION_DUPLICATE:
                case MEDICATIONSVC_PRESCRIPTION_STATUS:
                    logWarning(taskEvent)
                        .keyValue("event", "Processing task event: Unexpected operation outcome")
                        .keyValue("reason",
                                  "EPAOperationOutcome code: " + std::string(magic_enum::enum_name(issue.detailsCode)));
                    break;
                case SVC_IDENTITY_MISMATCH:
                case MEDICATIONSVC_NO_VALID_STRUCTURE:
                case MEDICATIONSVC_PRESCRIPTION_NO_EXIST:
                case MEDICATIONSVC_PARAMETERS_REFERENCE_NO_EXIST:
                case MEDICATIONSVC_MEDICATIONPLAN_NO_EXIST:
                case MEDICATIONSCV_MEDICINAL_PRODUCT_PACKAGE_NOT_ALLOWED:
                case MEDICATIONSVC_DISPENSATION_NO_EXIST:
                case MEDICATIONSVC_DISPENSATION_STATUS:
                case GENERIC_OPERATION_OUTCOME_CODE:
                    logError(taskEvent)
                        .keyValue("event", "Processing task event: Unexpected error. Adding to deadletter queue.")
                        .keyValue("reason", "unexpected operation outcome from Medication Client: " +
                                                std::string(magic_enum::enum_name(issue.detailsCode)));
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
