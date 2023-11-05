/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_UTIL_CONFIGURATIONFORMATTER_HXX
#define ERP_PROCESSING_CONTEXT_UTIL_CONFIGURATIONFORMATTER_HXX

#include "erp/util/Configuration.hxx"

#include <string>

class ConfigurationFormatter
{
public:
    static std::string formatAsJson(const Configuration& config, int flags);
};

#endif