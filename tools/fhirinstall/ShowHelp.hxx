#ifndef ERP_TOOLS_FHIRINSTALL_SHOWHELP_HXX
#define ERP_TOOLS_FHIRINSTALL_SHOWHELP_HXX

#include <stdexcept>

class ShowHelp : public std::runtime_error
{
    using runtime_error::runtime_error;
};


#endif// ERP_TOOLS_FHIRINSTALL_SHOWHELP_HXX
