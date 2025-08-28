/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/extensions/ChargeItemMarkingFlags.hxx"
#include "shared/model/ResourceNames.hxx"
#include "shared/model/Extension.txx"

#include <rapidjson/pointer.h>


namespace model
{

using namespace resource;

ChargeItemMarkingFlags::ChargeItemMarkingFlags(std::optional<bool> insuranceProvider, std::optional<bool> subsidy,
                                               std::optional<bool> taxOffice)
{
    using namespace resource::elements;
    setValue(rapidjson::Pointer{resource::ElementName::path(resource::elements::url)}, url);
    static const rapidjson::Pointer extensionPtr{ElementName::path(elements::extension)};
    if (insuranceProvider.has_value())
    {
        addToArray(extensionPtr,
                   copyValue(ChargeItemMarkingFlag{"insuranceProvider", insuranceProvider.value()}.jsonDocument()));
    }
    if (subsidy.has_value())
    {
        addToArray(extensionPtr, copyValue(ChargeItemMarkingFlag{"subsidy", subsidy.value()}.jsonDocument()));
    }
    if (taxOffice.has_value())
    {
        addToArray(extensionPtr, copyValue(ChargeItemMarkingFlag{"taxOffice", taxOffice.value()}.jsonDocument()));
    }
}

ChargeItemMarkingFlags::MarkingContainer ChargeItemMarkingFlags::getAllMarkings() const
{
    static const rapidjson::Pointer extensionArrayPointer(ElementName::path(elements::extension));
    const auto* extensionElem = getValue(extensionArrayPointer);
    std::map<std::string, bool> results;
    if(extensionElem)
    {
        ModelExpect(extensionElem->IsArray(), ResourceBase::pointerToString(extensionArrayPointer) + ": element must be array");
        for(const auto& elem: extensionElem->GetArray())
        {
            static const rapidjson::Pointer urlPointer(ElementName::path(elements::url));
            const auto urlValue = getStringValue(elem, urlPointer);

            bool markingFlag = false;
            static const rapidjson::Pointer markingPointer(ElementName::path(elements::valueBoolean));
            const auto* markingValue = getValue(elem, markingPointer);
            if(markingValue)
            {
                ModelExpect(markingValue->IsBool(), ResourceBase::pointerToString(markingPointer) + ": element must be of type boolean");
                markingFlag = markingValue->GetBool();
            }
            results.emplace(urlValue, markingFlag);
        }
    }
    return results;
}

//static
bool ChargeItemMarkingFlags::isMarked(const MarkingContainer& markings)
{
    return std::ranges::find_if(markings,
                        [](const auto& elem){ return elem.second; }) != markings.end();
}


ChargeItemMarkingFlag::ChargeItemMarkingFlag(const std::string_view url, bool value)
    : Extension(NoProfile)
{
    setValue(rapidjson::Pointer{ElementName::path(resource::elements::url)}, url);
    setValue(rapidjson::Pointer{ElementName::path(resource::elements::valueBoolean)}, rapidjson::Value(value));
}

template class Extension<ChargeItemMarkingFlag>;
template class Extension<ChargeItemMarkingFlags>;

}// namespace model
