/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_ERP_SERVERINFO
#define ERP_PROCESSING_CONTEXT_ERP_SERVERINFO

#include <string_view>

class ErpServerInfo
{
public:
    static std::string_view BuildVersion();
    static std::string_view BuildType();
    static std::string_view ReleaseVersion();
    static std::string_view ReleaseDate();
};

#endif
