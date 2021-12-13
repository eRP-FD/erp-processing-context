/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/model/KbvMedicationFreeText.hxx"

namespace model
{
KbvMedicationFreeText::KbvMedicationFreeText(NumberAsStringParserDocument&& document)
    : Resource<KbvMedicationFreeText, ResourceVersion::KbvItaErp>(std::move(document))
{
}
}
