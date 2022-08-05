/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_FHIR_TOOLS_UTIL_VALISTHELPER_HXX
#define ERP_PROCESSING_CONTEXT_SRC_FHIR_TOOLS_UTIL_VALISTHELPER_HXX

#include <string>

namespace fhirtools
{

class VaListHelper
{
public:
    // convert the given va_list to string using vsnprintf
    static std::string vaListToString(const char* msg, va_list args);
};

}// fhirtools

#endif//ERP_PROCESSING_CONTEXT_SRC_FHIR_TOOLS_UTIL_VALISTHELPER_HXX
