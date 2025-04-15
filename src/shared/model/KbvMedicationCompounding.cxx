/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/model/KbvMedicationCompounding.hxx"
#include "shared/model/ResourceNames.hxx"

#include <rapidjson/pointer.h>

namespace model
{

namespace
{
const rapidjson::Pointer itemCodeableConceptSystemPointer("/itemCodeableConcept/coding/0/system");
const rapidjson::Pointer itemCodeableConceptCodePointer("/itemCodeableConcept/coding/0/code");
} // namespace

KbvMedicationCompounding::KbvMedicationCompounding(NumberAsStringParserDocument&& document)
    : MedicationBase<KbvMedicationCompounding>(std::move(document))
{
}

const rapidjson::Value* KbvMedicationCompounding::ingredientArray() const
{
    static const rapidjson::Pointer ingredientArrayPointer(resource::ElementName::path("ingredient"));
    return getValue(ingredientArrayPointer);
}

std::vector<Pzn> KbvMedicationCompounding::ingredientPzns() const
{
    std::vector<Pzn> pzns;
    if (const auto* ingredients = ingredientArray())
    {
        for (auto item = ingredients->Begin(), end = ingredients->End(); item != end; ++item)
        {
            const auto ingredientResource = model::UnspecifiedResource::fromJson(*item);
            auto system = ingredientResource.jsonDocument().getOptionalStringValue(itemCodeableConceptSystemPointer);
            if (system == resource::code_system::pzn)
            {
                const auto codeValue =
                    ingredientResource.jsonDocument().getOptionalStringValue(itemCodeableConceptCodePointer);
                ModelExpect(codeValue.has_value(), "Unexpected missing itemCodeableConcept.coding[0].code");
                pzns.emplace_back(*codeValue);
            }
        }
    }

    return pzns;
}

} // namespace model
