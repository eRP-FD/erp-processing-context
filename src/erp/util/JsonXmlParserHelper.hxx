/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_UTIL_JSONXMLPARSERHELPER_HXX
#define ERP_PROCESSING_CONTEXT_UTIL_JSONXMLPARSERHELPER_HXX

#include <rapidjson/document.h>

class JsonXmlParserHelper
{
public:
    static rapidjson::Document xmlStr2JsonDoc(const std::string& xmlStr);
};

#endif
