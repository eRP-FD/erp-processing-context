/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/beast/BoostBeastHeader.hxx"
#include "erp/ErpConstants.hxx"


Header::keyValueMap_t BoostBeastHeader::convertHeaderFields (const boost::beast::http::fields& inputFields)
{
    Header::keyValueMap_t outputFields;
    for (const auto& entry : inputFields)
        outputFields.emplace(std::string(entry.name_string()), std::string(entry.value()));
    return outputFields;
}
