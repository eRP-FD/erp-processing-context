/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/model/GemErpEuPrPractitioner.hxx"

namespace model
{

GemErpEuPrPractitioner::GemErpEuPrPractitioner(NumberAsStringParserDocument&& jsonTree)
    : Resource(std::move(jsonTree))
{
}

std::string_view GemErpEuPrPractitioner::practitionerId() const
{
    using namespace model::resource;

    const rapidjson::Pointer ptr(ElementName::path(elements::identifier, 0, elements::value));
    return getStringValue(ptr);
}

}
