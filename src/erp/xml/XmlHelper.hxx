/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_FHIR_INTERNAL_XMLHELPER_HXX
#define ERP_PROCESSING_CONTEXT_FHIR_INTERNAL_XMLHELPER_HXX

#include "libxml/xmlstring.h"
#include <cstddef>
#include <cstring>
#include <string>
#include <string_view>

class XmlStringView
{
public:
    constexpr XmlStringView(const char* ns, size_t size): mCString(ns), mSize(size){}
    template <size_t size>
    explicit constexpr XmlStringView(const char(&ns)[size]): mCString(ns), mSize(size - 1){}

    explicit XmlStringView(const xmlChar* xmlStr): mCString(reinterpret_cast<const char*>(xmlStr)), mSize(std::strlen(mCString)){}
    explicit XmlStringView(const std::string& xmlStr): mCString(xmlStr.data()), mSize(xmlStr.size()){}

    bool operator == (const xmlChar* otherStr) const;

    operator std::string_view() const { return std::string_view{mCString, mSize};}

    size_t size() const { return mSize; }
    const char* begin() const { return mCString; }
    const char* end() const { return mCString + mSize; }

    const xmlChar* xs_str() const { return reinterpret_cast<const xmlChar*>(mCString);}

private:
    const char* mCString;
    size_t mSize;
};

static constexpr XmlStringView xhtmlNamespaceUri {"http://www.w3.org/1999/xhtml"};

inline bool XmlStringView::operator==(const xmlChar* otherStr) const
{
    return otherStr && std::strncmp(mCString, reinterpret_cast<const char*>(otherStr), mSize) == 0;
}

inline bool operator == (const xmlChar* lhs, const XmlStringView& rhs)
{
    return rhs.operator ==(lhs);
}

inline bool operator != (const xmlChar* lhs, const XmlStringView& rhs)
{
    return not(lhs == rhs);
}

inline bool operator != (const XmlStringView& lhs, const xmlChar* rhs)
{
    return not(lhs == rhs);
}

namespace xmlHelperLiterals {
constexpr XmlStringView operator "" _xs (const char* str, size_t size) { return XmlStringView{str, size}; }
}

inline std::string xmlStringToString(const xmlChar* xmlStr)
{ return std::string(reinterpret_cast<const char*>(xmlStr));}

#endif // ERP_PROCESSING_CONTEXT_FHIR_INTERNAL_XMLHELPER_HXX
