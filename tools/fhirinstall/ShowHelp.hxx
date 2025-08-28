/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */
#ifndef ERP_TOOLS_FHIRINSTALL_SHOWHELP_HXX
#define ERP_TOOLS_FHIRINSTALL_SHOWHELP_HXX

#include <stdexcept>

class ShowHelp : public std::runtime_error
{
    using runtime_error::runtime_error;
};


#endif// ERP_TOOLS_FHIRINSTALL_SHOWHELP_HXX
