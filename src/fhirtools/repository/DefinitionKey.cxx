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
    std::string result{definitionKey.url};
    if (definitionKey.version)
    {
        result.append(1, '|').append(to_string(*definitionKey.version));
    }
    return result;
}
