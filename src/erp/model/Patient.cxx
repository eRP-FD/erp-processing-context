/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/model/Patient.hxx"
#include "erp/model/ResourceNames.hxx"

#include "erp/util/Expect.hxx"

#include <rapidjson/pointer.h>
#include <utility>

namespace model
{

namespace
{
rapidjson::Pointer identifierPointer("/identifier");
rapidjson::Pointer typePointer("/system");
rapidjson::Pointer kvnrPointer("/value");
}

Patient::Patient (NumberAsStringParserDocument&& document)
    : Resource<Patient>(std::move(document))
{
}

std::string Patient::getKvnr () const
{
    const auto* pointerValue = getValue(identifierPointer);
    ModelExpect(pointerValue && pointerValue->IsArray(), "identifier not present or not an array.");
    const auto array = pointerValue->GetArray();
    for (auto item = array.Begin(), end = array.End(); item != end; ++item)
    {
        const auto* typePointerValue = typePointer.Get(*item);
        ModelExpect(typePointerValue && typePointerValue->IsString(), "missing identifier/system in identifier array entry");
        if (NumberAsStringParserDocument::getStringValueFromValue(typePointerValue) == resource::naming_system::gkvKvid10)
        {
            const auto* kvnrPointerValue = kvnrPointer.Get(*item);
            ModelExpect(kvnrPointerValue && kvnrPointerValue->IsString(), "missing identifier/value in identifier array entry");
            return std::string(NumberAsStringParserDocument::getStringValueFromValue(kvnrPointerValue));
        }
    }
    ModelFail("KVNR not found in identifier array.");
}

}

