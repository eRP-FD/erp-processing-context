/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once

#include "shared/model/Parameters.hxx"

namespace model {

class GemErpEuPrMedication : public Parameters<GemErpEuPrMedication>
{
public:
    static constexpr auto profileType = ProfileType::GEM_ERPEU_PR_Medication;
    using Parameters::Parameters;
    friend class Resource;
};

// NOLINTBEGIN(bugprone-exception-escape)
extern template class Resource<GemErpEuPrMedication>;
extern template class Parameters<GemErpEuPrMedication>;
// NOLINTEND(bugprone-exception-escape)

}// model
