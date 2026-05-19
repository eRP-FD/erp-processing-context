// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "DefinitionKey.hxx"

#include <ostream>

std::string_view fhirtools::DefinitionKey::shortProfile() const
{
    std::string_view s{std::string_view{url}.substr(url.find_last_of('/'))};
    if (!s.empty())
    {
        return s.substr(1);
    }
    return {};
}

std::ostream& fhirtools::operator<<(std::ostream& out, const DefinitionKey& definitionKey)
{
    out << definitionKey.url;
    if (definitionKey.version)
    {
        out << '|' << *definitionKey.version;
    }
    return out;
}

std::string fhirtools::to_string(const DefinitionKey& definitionKey)
{
    return fmt::format("{}", definitionKey);
}

//NOLINTNEXTLINE(readability-convert-member-functions-to-static)
fmt::format_context::iterator fmt::formatter<fhirtools::DefinitionKey>::format(const fhirtools::DefinitionKey& key,
                                                                               format_context& ctx) const
{
    fmt::format_to(ctx.out(), "{}", key.url);
    if (key.version)
    {
        fmt::format_to(ctx.out(), "|{}", *key.version);
    }
    return ctx.out();
}
