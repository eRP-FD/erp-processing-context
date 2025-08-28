/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once

#include "MedicationDispense.hxx"

namespace model
{

class GemErpEuPrMedicationDispense : public MedicationDispense
{
public:
    static constexpr auto profileType = ProfileType::GEM_ERPEU_PR_MedicationDispense;
};

// NOLINTBEGIN(bugprone-exception-escape)
extern template class Resource<GemErpEuPrMedicationDispense>;
// NOLINTEND(bugprone-exception-escape)

}// model
