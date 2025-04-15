/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/common/BoostBeastHeaderConverter.hxx"

Header::keyValueMap_t BoostBeastHeaderConverter::convertHeaderFields(
    const boost::beast::http::fields& inputFields)
{
    Header::keyValueMap_t outputFields;
    for (const auto& entry : inputFields)
    {
        std::string& value = outputFields[std::string(entry.name_string())];
        // If a header occurs twice, concatenate its values, separated by a comma.
        // Reference: https://tools.ietf.org/html/rfc2616#section-4.2
        if (value.empty())
            value = std::string(entry.value());
        else
            value += "," + std::string(entry.value());
    }
    return outputFields;
}
