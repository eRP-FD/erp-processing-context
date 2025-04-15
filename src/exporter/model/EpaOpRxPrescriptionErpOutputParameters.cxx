/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#include "EpaOpRxPrescriptionErpOutputParameters.hxx"

namespace model
{

EPAOperationOutcome::Issue EPAOpRxPrescriptionERPOutputParameters::getOperationOutcomeIssue() const
{
    const auto* parameter = findParameter("rxPrescription");
    ModelExpect(parameter, "invalid EPAOpRxPrescriptionERPOutputParameters: rxPrescription not found");
    const auto* ooPart = findPart(*parameter, "operationOutcome");
    ModelExpect(ooPart, "invalid EPAOpRxPrescriptionERPOutputParameters: operationOutcome not found");
    model::EPAOperationOutcome operationOutcome{model::OperationOutcome::fromJson(getResourceDoc(*ooPart))};
    ModelExpect(operationOutcome.getIssues().size() == 1, "expected exactly one EPAMSOperationOutcome.issue but got: " +
                                                              std::to_string(operationOutcome.getIssues().size()));
    return operationOutcome.getIssues()[0];
}

template class Parameters<EPAOpRxPrescriptionERPOutputParameters>;

}
