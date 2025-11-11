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

private:
    explicit GemErpEuPrMedicationDispense(model::NumberAsStringParserDocument&& jsonTree);
    friend Resource<GemErpEuPrMedicationDispense>;
};

// NOLINTBEGIN(bugprone-exception-escape)
extern template GemErpEuPrMedicationDispense
Resource<GemErpEuPrMedicationDispense>::fromJson(NumberAsStringParserDocument&&);
// NOLINTEND(bugprone-exception-escape)

}// model
