/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/eventprocessing/CancelPrescription.hxx"
#include "exporter/client/EpaMedicationClient.hxx"
#include "exporter/model/EpaOpCancelPrescriptionErpInputParameters.hxx"
#include "exporter/model/EpaOpRxPrescriptionErpOutputParameters.hxx"
#include "exporter/model/TaskEvent.hxx"

#include <utility>

namespace eventprocessing
{

Outcome CancelPrescription::process(gsl::not_null<IEpaMedicationClient*> client, const model::CancelPrescriptionTaskEvent& erpEvent)
{
    return CancelPrescription{std::move(client)}.doProcess(erpEvent);
}

CancelPrescription::CancelPrescription(gsl::not_null<IEpaMedicationClient*> medicationClient)
    : EventProcessingBase(std::move(medicationClient))
{
}

Outcome CancelPrescription::doProcess(const model::CancelPrescriptionTaskEvent& erpEvent)
{
    model::EPAOpCancelPrescriptionERPInputParameters params{erpEvent.getPrescriptionId(), erpEvent.getMedicationRequestAuthoredOn()};
    auto response = mMedicationClient->sendCancelPrescription(erpEvent.getXRequestId(), erpEvent.getKvnr(),
                                                              params.serializeToJsonString());
    Outcome outcome = fromHttpStatus(response.httpStatus);
    switch (outcome)
    {
        case Outcome::Success:
        case Outcome::SuccessAuditFail: {
            model::EPAOpRxPrescriptionERPOutputParameters responseParameters(std::move(response.body));
            const auto issue = responseParameters.getOperationOutcomeIssue();
            switch (issue.detailsCode)
            {
                using enum model::EPAOperationOutcome::EPAOperationOutcomeCS;
                case MEDICATIONSVC_OPERATION_SUCCESS:
                case MEDICATIONSVC_PRESCRIPTION_NO_EXIST:
                    break;
                case MEDICATIONSVC_PRESCRIPTION_STATUS:
                    logWarning(erpEvent)
                        << KeyValue("event", "Processing task event: Unexpected operation outcome")
                        << KeyValue("reason",
                                  "EPAOperationOutcome code: " + std::string(magic_enum::enum_name(issue.detailsCode)));
                    break;
                case SVC_IDENTITY_MISMATCH:
                case MEDICATIONSVC_NO_VALID_STRUCTURE:
                case MEDICATIONSVC_PRESCRIPTION_DUPLICATE:
                case MEDICATIONSVC_DISPENSATION_STATUS:
                case MEDICATIONSVC_DISPENSATION_NO_EXIST:
                case MEDICATIONSVC_PARAMETERS_REFERENCE_NO_EXIST:
                case MEDICATIONSVC_MEDICATIONPLAN_NO_EXIST:
                case MEDICATIONSCV_MEDICINAL_PRODUCT_PACKAGE_NOT_ALLOWED:
                case GENERIC_OPERATION_OUTCOME_CODE:
                    logError(erpEvent)
                        << KeyValue("event", "Processing task event: Unexpected error. Adding to deadletter queue.")
                        << KeyValue("reason", "unexpected operation outcome from Medication Client: " +
                                                std::string(magic_enum::enum_name(issue.detailsCode)));
                    outcome = Outcome::DeadLetter;
                    break;
            }
            break;
        }
        case Outcome::DeadLetter:
        case Outcome::Retry:
        case Outcome::Conflict:
        case Outcome::ConsentRevoked:
            logFailure(response.httpStatus, std::move(response.body));
            break;
    }
    return outcome;
}

}
