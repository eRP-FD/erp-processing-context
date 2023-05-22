/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_REVERSEINCLUDEARGUMENT_HXX
#define ERP_PROCESSING_CONTEXT_REVERSEINCLUDEARGUMENT_HXX

#include <string>

class ReverseIncludeArgument
{
public:
    static constexpr std::string_view revIncludeKey = "_revinclude";
    static constexpr std::string_view revIncludeAuditEventKey = "AuditEvent:entity.what";
};


#endif//ERP_PROCESSING_CONTEXT_REVERSEINCLUDEARGUMENT_HXX
