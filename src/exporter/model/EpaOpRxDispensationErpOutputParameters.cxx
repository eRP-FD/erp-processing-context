/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/model/EpaOpRxDispensationErpOutputParameters.hxx"

namespace model
{

EPAOperationOutcome::Issue EPAOpRxDispensationERPOutputParameters::getOperationOutcomeIssue() const
{
    const auto* parameter = findParameter("rxDispensation");
    ModelExpect(parameter, "invalid EPAOpRxDispensationERPOutputParameters: rxDispensation not found");
    const auto* ooPart = findPart(*parameter, "operationOutcome");
    ModelExpect(ooPart, "invalid EPAOpRxDispensationERPOutputParameters: operationOutcome not found");
    model::EPAOperationOutcome operationOutcome{model::OperationOutcome::fromJson(getResourceDoc(*ooPart))};
    ModelExpect(operationOutcome.getIssues().size() == 1, "expected exactly one EPAMSOperationOutcome.issue but got: " +
                                                              std::to_string(operationOutcome.getIssues().size()));
    return operationOutcome.getIssues()[0];
}

template class Parameters<EPAOpRxDispensationERPOutputParameters>;
}
