#include "DefinitionKey.hxx"

#include <ostream>

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
