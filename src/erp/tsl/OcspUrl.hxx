/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_TSL_OCSPURL_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_TSL_OCSPURL_HXX

#include <string>

class OcspUrl
{
public:
    std::string url;
    bool isRequestData{false};
};
#endif
