/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/validation/KbvMedicationModelHelper.hxx"

#include "erp/util/String.hxx"

#include "erp/util/Expect.hxx"

namespace model
{

SchemaType KbvMedicationModelHelper::getProfile() const
{
    static const rapidjson::Pointer profilePointer("/meta/profile/0");
    const auto& profileString = getStringValue(profilePointer);
    const auto& parts = String::split(profileString, '|');
    const auto& profileWithoutVersion = parts[0];
    const auto& pathParts = String::split(profileWithoutVersion, '/');
    const auto& profile = pathParts.back();
    const auto& schemaType = magic_enum::enum_cast<SchemaType>(profile);
    ModelExpect(schemaType.has_value(),
                "Could not extract schema type from profile string: " + std::string(profileString));
    return *schemaType;
}
KbvMedicationModelHelper::KbvMedicationModelHelper(NumberAsStringParserDocument&& document)
    : model::Resource<KbvMedicationModelHelper, model::ResourceVersion::KbvItaErp>(std::move(document))
{
}
}
