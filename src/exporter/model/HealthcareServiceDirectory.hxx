/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#pragma once

#include "shared/model/Extension.hxx"
#include "shared/model/ResourceNames.hxx"

namespace model
{

class HealthcareServiceDirectory : public Resource<HealthcareServiceDirectory>
{
public:
    static constexpr auto resourceTypeName = "HealthcareService";
    static constexpr auto profileType = ProfileType::HealthcareServiceDirectory;
    std::optional<Timestamp> getValidationReferenceTimestamp() const override;

    HealthcareServiceDirectory();

    [[nodiscard]] std::string_view getOrganizationReference() const;
    [[nodiscard]] std::optional<std::string_view> getLocationReference() const;
    [[nodiscard]] const rapidjson::Value* telecomRaw() const;

private:
    friend Resource<HealthcareServiceDirectory>;
    explicit HealthcareServiceDirectory(NumberAsStringParserDocument&& jsonTree);
};

extern template class Resource<HealthcareServiceDirectory>;

}
