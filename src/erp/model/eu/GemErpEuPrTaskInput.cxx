/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/eu/GemErpEuPrTaskInput.hxx"

namespace model {

std::optional<model::Timestamp> GemErpEuPrTaskInput::getValidationReferenceTimestamp() const
{
    return Timestamp::now();
}

bool GemErpEuPrTaskInput::isEuRedeemableByPatientAuthorization() const
{
    const auto *const paramName = "eu-isRedeemableByPatientAuthorization";

    const auto *const parameter = findParameter(paramName);
    ModelExpect(parameter, "could not find isEuRedeemableByPatientAuthorization parameter");

    const auto *const value = partValueBooleanPointer.Get(*parameter);
    ModelExpect(value, "invalid parameter access.");
    ModelExpect(value->IsBool(), "expected boolean type not found.");
    return value->GetBool();
}

template class Parameters<GemErpEuPrTaskInput>;

}
