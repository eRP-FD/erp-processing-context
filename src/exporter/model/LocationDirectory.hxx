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

class LocationDirectory : public Resource<LocationDirectory>
{
public:
    static constexpr auto resourceTypeName = "Location";
    static constexpr auto profileType = ProfileType::LocationDirectory;
    std::optional<Timestamp> getValidationReferenceTimestamp() const override;

    LocationDirectory();

    [[nodiscard]] std::optional<std::string_view> getAddress() const;
    [[nodiscard]] const rapidjson::Value* addressRaw() const;

private:
    friend Resource<LocationDirectory>;
    explicit LocationDirectory(NumberAsStringParserDocument&& jsonTree);
};

extern template class Resource<LocationDirectory>;

}
