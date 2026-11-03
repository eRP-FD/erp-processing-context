/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */
#include "DecimalType.hxx"

#include <boost/algorithm/string/trim.hpp>

std::string fhirtools::to_string(const DecimalType& dec)
{
    auto trimmed =
        boost::trim_right_copy_if(dec.str(8, std::ios_base::fixed | std::ios_base::showpoint), [](const auto& c) {
            return c == '0';
        });
    if (trimmed.back() == '.')
    {
        trimmed += '0';
    }
    return trimmed;
}
