// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "FhirVersion.hxx"

#include <ostream>

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
