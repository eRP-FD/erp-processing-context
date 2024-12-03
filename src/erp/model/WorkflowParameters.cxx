/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/WorkflowParameters.hxx"
#include "shared/model/ResourceNames.hxx"

#include <libxml/xpath.h>
#include <magic_enum/magic_enum.hpp>

#include <tuple>


namespace model
{

// GEMREQ-start A_22878#MarkingFlag
PatchChargeItemParameters::MarkingFlag::MarkingFlag()
    : insuranceProvider{}
    , subsidy{}
    , taxOffice{}
{
    insuranceProvider.url = "ChargeItem.extension('https://gematik.de/fhir/erpchrg/StructureDefinition/GEM_ERPCHRG_EX_MarkingFlag').extension('insuranceProvider')";
    subsidy.url = "ChargeItem.extension('https://gematik.de/fhir/erpchrg/StructureDefinition/GEM_ERPCHRG_EX_MarkingFlag').extension('subsidy')";
    taxOffice.url = "ChargeItem.extension('https://gematik.de/fhir/erpchrg/StructureDefinition/GEM_ERPCHRG_EX_MarkingFlag').extension('taxOffice')";
}

std::optional<std::reference_wrapper<PatchChargeItemParameters::MarkingFlag::Element>>
PatchChargeItemParameters::MarkingFlag::findByUrl(const std::string_view& url)
{
    if (insuranceProvider.url == url)
    {
        return std::ref(insuranceProvider);
    }

    if (subsidy.url == url)
    {
        return std::ref(subsidy);
    }

    if (taxOffice.url == url)
    {
        return std::ref(taxOffice);
    }

    return std::nullopt;
}
// GEMREQ-end A_22878#MarkingFlag

std::string PatchChargeItemParameters::MarkingFlag::toExtensionJson() const
{
    std::string result = R"({"url": "https://gematik.de/fhir/erpchrg/StructureDefinition/GEM_ERPCHRG_EX_MarkingFlag","extension": [)";

    if (insuranceProvider.value.has_value())
    {
        result += R"({"url": "insuranceProvider", "valueBoolean": )";
        result += (*insuranceProvider.value ? "true" : "false");
        result += "},";
    }

    if (subsidy.value.has_value())
    {
        result += R"({"url": "subsidy", "valueBoolean": )";
        result += (*subsidy.value ? "true" : "false");
        result += "},";
    }

    if (taxOffice.value.has_value())
    {
        result += R"({"url": "taxOffice", "valueBoolean": )";
        result += (*taxOffice.value ? "true" : "false");
        result += "},";
    }

    if (result.back() == ',')
    {
        result.pop_back();
    }

    result += "]}";

    return result;
}

std::optional<model::PrescriptionType> CreateTaskParameters::getPrescriptionType() const
{
    const auto stringValue = getStringValue(workFlowTypePointer);
    const uint8_t numeric = static_cast<uint8_t>(std::stoi(std::string{stringValue}));
    return magic_enum::enum_cast<model::PrescriptionType>(numeric);
}

// GEMREQ-start A_22878#getChargeItemMarkingFlag
model::ChargeItemMarkingFlags PatchChargeItemParameters::getChargeItemMarkingFlags() const
{
    const auto resourceType = getStringValue(resourceTypePointer);
    ModelExpect(resourceType == "Parameters", "Invalid resource type");

    const auto* parameterElem = getValue(parameterArrayPointer);
    ModelExpect(parameterElem, "Missing element " + Resource::pointerToString(parameterArrayPointer));
    ModelExpect(parameterElem->IsArray(), "Element must be array: " + Resource::pointerToString(parameterArrayPointer));

    const auto paramArray = parameterElem->GetArray();
    ModelExpect(paramArray.Size() >= 1 && paramArray.Size() <= 3, "Unexpected number of parameters");

    MarkingFlag result{};
    for (const auto& actualParameter : paramArray)
    {
        const auto nameValue = getStringValue(actualParameter, namePointer);
        ModelExpect(nameValue == "operation", "Invalid value for parameter element " + Resource::pointerToString(namePointer));

        const auto* partElem = getValue(actualParameter, partArrayPointer);
        ModelExpect(partElem, "Missing parameter element " + Resource::pointerToString(partArrayPointer));
        ModelExpect(partElem->IsArray(), "Element must be array: " + Resource::pointerToString(partArrayPointer));

        updateMarkingFlagFromPartArray(partElem->GetArray(), result);
    }

    auto extensionJson = result.toExtensionJson();
    auto extensionDocument = NumberAsStringParserDocument::fromJson(std::move(extensionJson));
    return ChargeItemMarkingFlags{std::move(extensionDocument)};
}
// GEMREQ-end A_22878#getChargeItemMarkingFlag

// GEMREQ-start A_22878#updateMarkingFlagFromPartArray
void PatchChargeItemParameters::updateMarkingFlagFromPartArray(//NOLINT(readability-function-cognitive-complexity)
    const NumberAsStringParserDocument::ConstArray& partArray, MarkingFlag& result) const
{
    using namespace resource;
    ModelExpect(partArray.Size() == 4,
                "'Array element " + Resource::pointerToString(partArrayPointer) + " must contain exactly four sub-elements");

    std::map<std::string_view, bool> isValid{};
    bool resultElementValue = false;
    std::optional<std::reference_wrapper<MarkingFlag::Element>> resultElement{};

    for (const auto& actualPart : partArray)
    {
        ModelExpect(actualPart.MemberCount() == 2, "'part' element must have two sub-elements");
        const auto partNameValue = getStringValue(actualPart, namePointer);
        if (partNameValue == elements::path)
        {
            const auto value = getStringValue(actualPart, partValueStringPointer);
            resultElement = result.findByUrl(value);
            ModelExpect(resultElement.has_value(),
                        "Invalid value of element " + Resource::pointerToString(partValueStringPointer) + " for part element with name=" +
                        std::string(elements::path) + ", it is not allowed to patch any fields except the marking flags");
        }
        else if (partNameValue == elements::type)
        {
            const auto value = getStringValue(actualPart, partValueCodePointer);
            ModelExpect(value == "add",
                        "Invalid value of element " + Resource::pointerToString(partValueCodePointer) +
                        " for part element with name=" + std::string(elements::type));
        }
        else if (partNameValue == elements::name)
        {
            const auto value = getStringValue(actualPart, partValueStringPointer);
            ModelExpect(value == elements::valueBoolean,
                        "Invalid value of element " + Resource::pointerToString(partValueStringPointer) +
                        " for part element with name=" + std::string(elements::name));
        }
        else if (partNameValue == elements::value)
        {
            const auto* valueBooleanValue = partValueBooleanPointer.Get(actualPart);
            ModelExpect(valueBooleanValue,
                        "Missing part element " + Resource::pointerToString(partValueBooleanPointer) + " with name=" +
                        std::string(elements::value));
            ModelExpect(valueBooleanValue->IsBool(),
                        "Element " + Resource::pointerToString(partValueBooleanPointer) + " must be of type Boolean");
            resultElementValue = valueBooleanValue->GetBool();
        }
        else
            ModelFail("Invalid value for part element " + Resource::pointerToString(namePointer));

        ModelExpect(isValid.count(partNameValue) == 0, "Multiple occurences of part element with name=" + std::string(partNameValue));
        isValid[partNameValue] = true;
    }

    ModelExpect(!resultElement->get().value.has_value(),
                "Multiple occurrences of parameter with path value " + std::string(resultElement->get().url));
    resultElement->get().value = resultElementValue;
}
// GEMREQ-end A_22878#updateMarkingFlagFromPartArray


template class Parameters<CreateTaskParameters>;
template class Parameters<ActivateTaskParameters>;
template class Parameters<PatchChargeItemParameters>;


std::optional<model::Timestamp> CreateTaskParameters::getValidationReferenceTimestamp() const
{
    return Timestamp::now();
}

std::optional<model::Timestamp> ActivateTaskParameters::getValidationReferenceTimestamp() const
{
    return Timestamp::now();
}

std::optional<model::Timestamp> PatchChargeItemParameters::getValidationReferenceTimestamp() const
{
    return Timestamp::now();
}
} // namespace model
