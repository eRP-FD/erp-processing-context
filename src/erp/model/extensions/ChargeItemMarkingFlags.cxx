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
