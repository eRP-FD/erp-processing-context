// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#pragma once

#include "fhirtools/model/NumberAsStringParserDocument.hxx"
#include "fhirtools/repository/views/FhirStructureRepositoryView.hxx"

namespace model
{
class FhirResourceBase;
}
namespace fhirtools
{
struct DefinitionKey;
struct ValueMapping;
}
class TransformerBase
{
public:
    struct FixedValue {
        rapidjson::Pointer ptr;
        std::string value;
    };

protected:
    static model::NumberAsStringParserDocument
    transformResource(const fhirtools::DefinitionKey& targetProfileKey, const model::FhirResourceBase& sourceResource,
                      const std::vector<fhirtools::ValueMapping>& valueMappings,
                      const std::vector<FixedValue>& fixedValues);

    static void checkSetAbsentReason(model::NumberAsStringParserDocument& document, const std::string& memberPath,
                                     std::string_view valueCode);

    static std::shared_ptr<const fhirtools::FhirStructureRepositoryView>
    getRepo(const fhirtools::DefinitionKey& profileKey);
    static void remove(rapidjson::Value& from, const rapidjson::Pointer& toBeRemoved);
};
