/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_ERPCONSTANTS_HXX
#define ERP_PROCESSING_CONTEXT_ERPCONSTANTS_HXX

#include <cstdlib>
#include <cstdint>


class ErpConstants
{
public:
    static constexpr size_t DefaultBufferSize = 8192;
    static constexpr size_t MaxBodySize = 1024 * 1024;
    static constexpr size_t MaxResponseBodySize = 8 * 1024 * 1024;

    static constexpr std::int64_t SocketTimeoutSeconds = 30;

    static constexpr const char* ConfigurationFileNameVariable = "ERP_PROCESSING_CONTEXT_CONFIG";
    static constexpr const char* ConfigurationFileSearchPathDefault = ".";
    static constexpr const char* ConfigurationFileSearchPatternDefault = ".*\\.config\\.json";

};


#endif
