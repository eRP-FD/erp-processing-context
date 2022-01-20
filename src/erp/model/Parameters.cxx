/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/model/Parameters.hxx"
#include "erp/util/Expect.hxx"

#include <libxml/xpath.h>
#include <magic_enum.hpp>
#include <tuple>

namespace {
rapidjson::Pointer parameterPointer{"/parameter"};
rapidjson::Pointer namePointer{"/name"};
rapidjson::Pointer resourcePointer{"/resource"};
rapidjson::Pointer workFlowTypePointer("/parameter/0/valueCoding/code");
}



namespace model
{


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

}
