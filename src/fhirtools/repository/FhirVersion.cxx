// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "FhirVersion.hxx"
#include "shared/util/Expect.hxx"

#include <ostream>
#include <regex>

namespace fhirtools
{

FhirVersion::FhirVersion(NotVersioned)
    : mVersion{std::nullopt}
{
}

FhirVersion::FhirVersion(std::string version)
    : mVersion{std::in_place, std::move(version)}
{
}

auto FhirVersion::operator<=>(NotVersioned) const
{
    return mVersion <=> std::nullopt;
}

bool FhirVersion::operator==(NotVersioned) const
{
    return mVersion == std::nullopt;
}

void FhirVersion::verifyStrict() const
{
    ModelExpect(mVersion, "Version is not set");
    ModelExpect(std::regex_match(*mVersion, std::regex(strictPattern)),
                "Version contains invalid characters: not matching pattern " + std::string{strictPattern});
}

std::optional<std::string> FhirVersion::version() const
{
    return mVersion;
}

std::ostream& operator<<(std::ostream& out, const FhirVersion& ver)
{
    if (ver.version())
    {
        out << *ver.version();
    }
    else
    {
        out << FhirVersion::NotVersioned::str;
    }
    return out;
}

std::string to_string(const FhirVersion& ver)
{
    if (! ver.version())
    {
        return std::string{FhirVersion::NotVersioned::str};
    }
    return *ver.version();
}

}// namespace fhirtools
