/*
* (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once

#include "shared/model/Extension.hxx"
#include "shared/model/Parameters.hxx"

namespace model {

class GemErpEuPrTaskInput : public model::Parameters<GemErpEuPrTaskInput>
{
public:
    static constexpr auto profileType = ProfileType::GEM_ERPEU_PR_PAR_PATCH_Task_Input;
    using Parameters::Parameters;
    friend class Resource;

    bool isEuRedeemableByPatientAuthorization() const;
    std::optional<Timestamp> getValidationReferenceTimestamp() const override;

};

// NOLINTBEGIN(bugprone-exception-escape)
extern template class Resource<GemErpEuPrTaskInput>;
extern template class Parameters<GemErpEuPrTaskInput>;
// NOLINTEND(bugprone-exception-escape)

}// model
