// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#pragma once

#include "extensions/ChargeItemMarkingFlags.hxx"
#include "shared/model/Parameters.hxx"

namespace model {

class GemErpChrgPrParPatchChargeItemInput : public Parameters<GemErpChrgPrParPatchChargeItemInput> {
public:
    static constexpr auto profileType = ProfileType::GEM_ERPCHRG_PR_PAR_Patch_ChargeItem_Input;
    using Parameters::Parameters;
    friend class Resource;

    [[nodiscard]] ChargeItemMarkingFlags createMarkingExtension() const;
};

// NOLINTBEGIN(bugprone-exception-escape)
extern template class Resource<GemErpChrgPrParPatchChargeItemInput>;
extern template class Parameters<GemErpChrgPrParPatchChargeItemInput>;
// NOLINTEND
} // model

