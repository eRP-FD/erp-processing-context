/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef FHIR_TOOLS_FHIR_INTERNAL_XMLHELPER_HXX
#define FHIR_TOOLS_FHIR_INTERNAL_XMLHELPER_HXX

#include <libxml/xmlstring.h>
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <ranges>
#include <string>
#include <string_view>

class XmlStringView
{
public:
    constexpr XmlStringView(const char* ns, size_t size): mCString(ns), mSize(size){}
    template <size_t size>
    explicit constexpr XmlStringView(const char(&ns)[size]): mCString(static_cast<const char*>(ns)), mSize(size - 1){}

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

    enum class XmlCharType : xmlChar;
    friend bool operator==(const XmlCharType* c, const std::default_sentinel_t&)
    {
        return *c == XmlCharType{0};
    }
    friend bool operator==(const std::default_sentinel_t&, const XmlCharType* c)
    {
        return *c == XmlCharType{0};
    }
    static XmlCharType toXmlCharType(const char c)
    {
        return XmlCharType{static_cast<xmlChar>(c)};
    }
};

static constexpr XmlStringView xhtmlNamespaceUri {"http://www.w3.org/1999/xhtml"};

inline bool XmlStringView::operator==(const xmlChar* otherStr) const
{
    return otherStr &&
           std::ranges::equal(begin(), end(), reinterpret_cast<const XmlCharType*>(otherStr), std::default_sentinel,
                              std::ranges::equal_to{}, &XmlStringView::toXmlCharType);
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

template <std::ranges::contiguous_range T>
inline bool operator == (const XmlStringView& lhs, const T& rhs)
requires requires(const T& rhs) {{*rhs.begin()} -> std::same_as<const char&>;}
{
    return std::string_view{lhs} == std::string_view{rhs.begin(), rhs.end()};
}

template <std::ranges::contiguous_range T>
inline bool operator == (const T& lhs, const XmlStringView& rhs)
requires requires(const T& lhs) {{*lhs.begin()} -> std::same_as<const char&>;}
{
    return std::string_view{lhs.begin(), lhs.end()} == std::string_view{rhs};
}

namespace xmlHelperLiterals {
constexpr XmlStringView operator "" _xs (const char* str, size_t size) { return XmlStringView{str, size}; }
}

inline std::string xmlStringToString(const xmlChar* xmlStr)
{ return std::string(reinterpret_cast<const char*>(xmlStr));}

#endif // FHIR_TOOLS_FHIR_INTERNAL_XMLHELPER_HXX
