/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#include "erp/model/extensions/ChargeItemMarkingFlag.hxx"
#include "erp/model/ResourceNames.hxx"

#include <rapidjson/pointer.h>


namespace model
{

using namespace resource;

ChargeItemMarkingFlag::MarkingContainer ChargeItemMarkingFlag::getAllMarkings() const
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
bool ChargeItemMarkingFlag::isMarked(const MarkingContainer& markings)
{
    return std::find_if(markings.begin(), markings.end(),
                        [](const auto& elem){ return elem.second; }) != markings.end();
}



} // namespace model