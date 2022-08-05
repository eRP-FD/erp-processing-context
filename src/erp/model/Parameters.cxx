/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/model/Parameters.hxx"

#include <libxml/xpath.h>
#include <magic_enum.hpp>

#include <tuple>

namespace {

rapidjson::Pointer resourceTypePointer{"/resourceType"};
rapidjson::Pointer parameterPointer{"/parameter"};
rapidjson::Pointer namePointer{"/name"};
rapidjson::Pointer resourcePointer{"/resource"};
rapidjson::Pointer partPointer{"/part"};
rapidjson::Pointer partValueCodePointer{"/valueCode"};
rapidjson::Pointer partValueStringPointer{"/valueString"};
rapidjson::Pointer partValueBooleanPointer{"/valueBoolean"};
rapidjson::Pointer workFlowTypePointer("/parameter/0/valueCoding/code");

}


namespace model
{


Parameters::MarkingFlag::MarkingFlag()
: insuranceProvider{},
  subsidy{},
  taxOffice{}
{
    insuranceProvider.url = "ChargeItem.extension('https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_EX_MarkingFlag').extension('insuranceProvider')";
    subsidy.url = "ChargeItem.extension('https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_EX_MarkingFlag').extension('subsidy')";
    taxOffice.url = "ChargeItem.extension('https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_EX_MarkingFlag').extension('taxOffice')";
}

std::optional<std::reference_wrapper<Parameters::MarkingFlag::Element>> Parameters::MarkingFlag::findByUrl(const std::string_view& url)
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

std::string Parameters::MarkingFlag::toExtensionJson() const
{
    std::string result = R"({"url": "https://gematik.de/fhir/StructureDefinition/MarkingFlag","extension": [)";

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

Parameters::Parameters(NumberAsStringParserDocument&& document)
    : Resource(std::move(document))
{
}

size_t Parameters::count() const
{
    return valueSize(parameterPointer);
}

const rapidjson::Value* Parameters::findResourceParameter(const std::string_view& name) const
{
    auto result = findMemberInArray(parameterPointer, namePointer, name, resourcePointer);
    if (!result)
    {
        return nullptr;
    }
    return std::get<0>(result.value());
}

std::optional<model::PrescriptionType> Parameters::getPrescriptionType() const
{
    const auto stringValue = getStringValue(workFlowTypePointer);
    const uint8_t numeric = static_cast<uint8_t>(std::stoi(std::string{stringValue}));
    return magic_enum::enum_cast<model::PrescriptionType>(numeric);
}

std::optional<model::ChargeItemMarkingFlag> Parameters::getChargeItemMarkingFlag() const
{
    const auto resourceType = getStringValue(resourceTypePointer);
    if (resourceType != "Parameters")
    {
        return std::nullopt;
    }

    const auto* parameterElem = getValue(parameterPointer);
    if (!parameterElem || !parameterElem->IsArray())
    {
        return std::nullopt;
    }

    MarkingFlag result{};
    for (const auto& actualParameter : parameterElem->GetArray())
    {
        const auto nameValue = getOptionalStringValue(actualParameter, namePointer);
        if (!nameValue.has_value() || *nameValue != "operation")
        {
            continue;
        }

        const auto* partElem = getValue(actualParameter, partPointer);
        if (!partElem || !partElem->IsArray())
        {
            continue;
        }

        updateMarkingFlagFromPart(partElem->GetArray(), result);
    }

    auto extensionJson = result.toExtensionJson();
    auto extensionDocument = NumberAsStringParserDocument::fromJson(std::move(extensionJson));
    return ChargeItemMarkingFlag{std::move(extensionDocument)};
}

void Parameters::updateMarkingFlagFromPart( //NOLINT(readability-function-cognitive-complexity)
    const NumberAsStringParserDocument::ConstArray& partArray,
    MarkingFlag& result) const
{
    std::map<std::string, bool> isValid{};
    std::optional<bool> resultElementValue{};
    std::optional<std::reference_wrapper<MarkingFlag::Element>> resultElement{};

    auto parsePartNamesTypePathAndName = [&, this](const auto& name, const auto& part) mutable
    {
        const auto pointer = (name == "path" || name == "name") ? partValueStringPointer : partValueCodePointer;
        const auto value = getOptionalStringValue(part, pointer);
        if (!value.has_value())
        {
            return false;
        }

        if (name == "type")
        {
            if (*value == "add")
            {
                isValid["type"] = true;
            }
        }
        else if (name == "path")
        {
            resultElement = result.findByUrl(*value);
            if (resultElement.has_value())
            {
                isValid["path"] = true;
            }
        }
        else if (name == "name")
        {
            if (*value == "valueBoolean")
            {
                isValid["name"] = true;
            }
        }

        return true;
    };

    auto parsePartNameValue = [&, this](const auto& part) mutable
    {
        const auto* valueBooleanValue = partValueBooleanPointer.Get(part);
        if (!valueBooleanValue || !valueBooleanValue->IsBool())
        {
            return false;
        }

        isValid["value"] = true;
        resultElementValue = valueBooleanValue->GetBool();

        return true;
    };

    for (const auto& actualPart : partArray)
    {
        const auto partNameValue = getOptionalStringValue(actualPart, namePointer);
        if (!partNameValue.has_value())
        {
            break;
        }

        if ((*partNameValue == "type" || *partNameValue == "path" || *partNameValue == "name") &&
            (!parsePartNamesTypePathAndName(*partNameValue, actualPart)))
        {
            break;
        }

        if (*partNameValue == "value" && !parsePartNameValue(actualPart))
        {
            break;
        }
    }

    if (isValid.count("type") && isValid.count("path") && isValid.count("name") && isValid.count("value"))
    {
        if (!resultElementValue.has_value() || !resultElement.has_value())
        {
            Fail2("Part is valid but missing values", std::logic_error);
        }

        resultElement->get().value = *resultElementValue;
    }
}

}
