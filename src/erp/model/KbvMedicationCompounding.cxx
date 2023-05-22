/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/KbvMedicationCompounding.hxx"
#include "erp/model/ResourceNames.hxx"

namespace model
{
KbvMedicationCompounding::KbvMedicationCompounding(NumberAsStringParserDocument&& document)
    : KbvMedicationBase<KbvMedicationCompounding, ResourceVersion::KbvItaErp>(std::move(document))
{
}

const rapidjson::Value* KbvMedicationCompounding::ingredientArray() const
{
    static const rapidjson::Pointer ingredientArrayPointer(resource::ElementName::path("ingredient"));
    return getValue(ingredientArrayPointer);
}

}
