/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/beast/BoostBeastHeader.hxx"

#include "erp/ErpConstants.hxx"

#include "erp/common/BoostBeastHttpStatus.hxx"



Header::keyValueMap_t BoostBeastHeader::convertHeaderFields (const boost::beast::http::fields& inputFields)
{
    Header::keyValueMap_t outputFields;
    for (const auto& entry : inputFields)
        outputFields.emplace(std::string(entry.name_string()), std::string(entry.value()));
    return outputFields;
}
