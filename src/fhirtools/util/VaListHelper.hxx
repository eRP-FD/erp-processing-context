/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
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
