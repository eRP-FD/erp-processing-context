/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/model/GemErpEuPrMedicationDispense.hxx"
#include "shared/model/Resource.txx"
#include "shared/model/TelematikId.hxx"

namespace model
{
model::GemErpEuPrMedicationDispense::GemErpEuPrMedicationDispense(model::NumberAsStringParserDocument&& jsonTree)
    : MedicationDispense{jsonTree}
{
}

template GemErpEuPrMedicationDispense Resource<GemErpEuPrMedicationDispense>::fromJson(NumberAsStringParserDocument&&);
}
