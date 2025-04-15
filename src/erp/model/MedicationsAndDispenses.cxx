#include "MedicationsAndDispenses.hxx"

#include "shared/model/MedicationDispense.hxx"
#include "shared/model/MedicationDispenseBundle.hxx"
#include "shared/model/MedicationDispenseId.hxx"
#include "shared/util/Expect.hxx"

void model::MedicationsAndDispenses::addFromBundle(const MedicationDispenseBundle& bundle)
{
    using namespace model::resource;
    static const rapidjson::Pointer resourcePointer{ElementName::path(elements::resource)};
    static const rapidjson::Pointer resourceTypePointer{ElementName::path(elements::resourceType)};
    for (const auto& entry : bundle.getEntries())
    {
        const auto* resource = resourcePointer.Get(entry);
        ModelExpect(resource != nullptr, "missing resource in Bundle.entry");
        const auto* resourceTypeValue = resourceTypePointer.Get(*resource);
        ModelExpect(resourceTypeValue != nullptr, "missing resourceType in Bundle.entry.resource");
        const auto& resourceType = NumberAsStringParserDocument::getStringValueFromValue(resourceTypeValue);
        if (resourceType == model::MedicationDispense::resourceTypeName)
        {
            medicationDispenses.emplace_back(model::MedicationDispense::fromJson(*resource));
        }
        else if(resourceType == model::GemErpPrMedication::resourceTypeName)
        {
            medications.emplace_back(model::GemErpPrMedication::fromJson(*resource));
        }
        else
        {
            ModelFail("Unexpected resource type in MedicationDispenseBundle: " + std::string(resourceType));
        }
    }
}

model::MedicationsAndDispenses model::MedicationsAndDispenses::filter(const MedicationDispenseId& medicationDispenseId) &&
{
    static constexpr std::string_view urnUuid{"urn:uuid:"};
    model::MedicationsAndDispenses result;
    auto dispenseIt = std::ranges::find(medicationDispenses, medicationDispenseId.toString(), &model::MedicationDispense::getId);
    if (dispenseIt == medicationDispenses.end())
    {
        return result;
    }
    const auto& dispense = result.medicationDispenses.emplace_back(std::move(*dispenseIt));
    if (medications.empty())
    {
        return result;
    }
    auto medicationReference = dispense.medicationReference();
    ModelExpect(medicationReference.starts_with(urnUuid), "medicationReference is not urn:uuid");
    auto medicationIt =
        std::ranges::find(medications, medicationReference.substr(urnUuid.size()), &model::GemErpPrMedication::getId);
    ModelExpect(medicationIt != medications.end(), "Missing medication for MedicationDispense");
    medications.emplace_back(std::move(*medicationIt));
    return result;
}
