/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/KbvMedicationFreeText.hxx"

namespace model
{
KbvMedicationFreeText::KbvMedicationFreeText(NumberAsStringParserDocument&& document)
    : KbvMedicationBase<KbvMedicationFreeText, ResourceVersion::KbvItaErp>(std::move(document))
{
}
}
