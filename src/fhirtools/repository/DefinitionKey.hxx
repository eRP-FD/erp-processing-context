// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#ifndef FHIRTOOLS_DEFINITION_KEY_HXX
#define FHIRTOOLS_DEFINITION_KEY_HXX

#include "FhirVersion.hxx"

#include <iosfwd>

namespace fhirtools
{

struct DefinitionKey {
    explicit DefinitionKey(std::string_view urlWithVersion);
    DefinitionKey(std::string url, std::optional<FhirVersion> version);

    std::string url;
    std::optional<FhirVersion> version;

    std::string_view shortProfile() const;

    DefinitionKey(const DefinitionKey&) = default;
    DefinitionKey(DefinitionKey&&) = default;
    DefinitionKey& operator=(const DefinitionKey&) = default;
    DefinitionKey& operator=(DefinitionKey&&) = default;
    bool operator==(const DefinitionKey&) const = default;
    auto operator<=>(const DefinitionKey&) const = default;// NOLINT(hicpp-use-nullptr,modernize-use-nullptr)
};

std::ostream& operator<<(std::ostream& out, const DefinitionKey& definitionKey);
std::string to_string(const DefinitionKey& definitionKey);

}// namespace fhirtools

template<>
struct std::hash<fhirtools::DefinitionKey> {
    static constexpr std::hash<std::string> urlHash{};
    static constexpr std::hash<std::optional<fhirtools::FhirVersion>> versionHash{};
    size_t operator()(const fhirtools::DefinitionKey& key) const
    {
        return urlHash(key.url) % versionHash(key.version);
    }
};

#endif// FHIRTOOLS_DEFINITION_KEY_HXX
