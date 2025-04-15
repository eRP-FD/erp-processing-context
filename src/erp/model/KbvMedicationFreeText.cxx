/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/KbvMedicationFreeText.hxx"

namespace model
{
KbvMedicationFreeText::KbvMedicationFreeText(NumberAsStringParserDocument&& document)
    : MedicationBase<KbvMedicationFreeText>(std::move(document))
{
}
}
