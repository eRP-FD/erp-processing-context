/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/model/GemErpEuPrOrganization.hxx"

namespace model
{

GemErpEuPrOrganization::GemErpEuPrOrganization(NumberAsStringParserDocument&& jsonTree)
    : Resource(std::move(jsonTree))
{
}

}
