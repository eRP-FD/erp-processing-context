#ifndef ERP_PROCESSING_CONTEXT_UTIL_JSONXMLPARSERHELPER_HXX
#define ERP_PROCESSING_CONTEXT_UTIL_JSONXMLPARSERHELPER_HXX

#include <rapidjson/document.h>

class JsonXmlParserHelper
{
public:
    static rapidjson::Document xmlStr2JsonDoc(const std::string& xmlStr);
};

#endif
