// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH
#ifndef FHIRTOOLS_FHIRVERSION
#define FHIRTOOLS_FHIRVERSION

#include "shared/util/Version.hxx"

#include <iosfwd>
#include <optional>

namespace fhirtools
{
class FhirVersion
{
public:
    struct NotVersioned {
        static constexpr std::string_view str{"<none>"};
    };
    static constexpr NotVersioned notVersioned{};
    FhirVersion(NotVersioned);
    explicit FhirVersion(std::string version);

    std::optional<std::string> version() const;

    FhirVersion(const FhirVersion&) = default;
    FhirVersion(FhirVersion&&) = default;
    FhirVersion& operator=(const FhirVersion&) = default;
    FhirVersion& operator=(FhirVersion&&) = default;
    auto operator<=>(const FhirVersion&) const = default;//NOLINT(hicpp-use-nullptr,modernize-use-nullptr)
    bool operator==(const FhirVersion&) const = default;
    auto operator<=>(NotVersioned) const;
    bool operator==(NotVersioned) const;

private:
    std::optional<Version> mVersion;

    friend class ::std::hash<FhirVersion>;
};

std::ostream& operator<<(std::ostream& out, const FhirVersion& ver);
std::string to_string(const FhirVersion& ver);

namespace version_literal
{
inline FhirVersion operator""_ver(const char* ver, size_t)
{
    return FhirVersion{std::string{ver}};
}
}

}// namespace fhirtools

template<>
struct std::hash<fhirtools::FhirVersion> {
    static constexpr std::hash<std::optional<Version>> h{};
    std::size_t operator()(const fhirtools::FhirVersion& ver) const
    {
        return h(ver.mVersion);
    }
};


#endif// FHIRTOOLS_FHIRVERSION
