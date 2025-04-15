/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#include "ConsentDecisionsResponseType.hxx"
#include "shared/util/Expect.hxx"

#include <rapidjson/document.h>
#include <rapidjson/pointer.h>

namespace model
{

ConsentDecisionsResponseType::ConsentDecisionsResponseType(std::string_view json)
{
    rapidjson::Document rjDoc;
    rjDoc.Parse(json.data(), json.size());
    ModelExpect(rjDoc.IsObject(), "invalid json: could not parse input");
    static const rapidjson::Pointer functionsArrayPtr{"/data"};
    const auto* arr = functionsArrayPtr.Get(rjDoc);
    ModelExpect(arr && arr->IsArray(), "invalid json: /data is not an array");
    for (const auto& item : arr->GetArray())
    {
        static const rapidjson::Pointer functionIdPtr{"/functionId"};
        static const rapidjson::Pointer decisionPtr{"/decision"};
        const auto* functionId = functionIdPtr.Get(item);
        ModelExpect(functionId && functionId->IsString(), "invalid json: /data/*/functionId");
        const auto* decision = decisionPtr.Get(item);
        ModelExpect(decision && decision->IsString(), "invalid json: /data/*/decision");
        auto decisionEnum = magic_enum::enum_cast<Decision>(decision->GetString());
        ModelExpect(decisionEnum.has_value(),
                    "invalid json: invalid decision value: " + std::string{decision->GetString()});
        auto [_, inserted] = mFunctions.emplace(functionId->GetString(), *decisionEnum);
        ModelExpect(inserted, "invalid json: duplicate functionId " + std::string{functionId->GetString()});
    }
}

std::optional<ConsentDecisionsResponseType::Decision>
ConsentDecisionsResponseType::getDecisionFor(const ConsentDecisionsResponseType::FunctionIdType& functionId) const
{
    if (mFunctions.contains(functionId))
    {
        return mFunctions.at(functionId);
    }
    return std::nullopt;
}

}
