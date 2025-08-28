/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
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

// this is a wrapper around c-string representation. We cannot avoid using it.
//NOLINTBEGIN(cppcoreguidelines-avoid-c-arrays, modernize-avoid-c-arrays)

class XmlStringView;
namespace xml_string_literal
{
constexpr XmlStringView operator""_xs(const char* xmlStr, std::size_t size);
}

class XmlStringView : public std::string_view
{
public:
    template<size_t strSize>
    explicit consteval XmlStringView(const char (&str)[strSize]);

    explicit XmlStringView(const xmlChar* xmlStr);
    explicit XmlStringView(const std::string& xmlStr)
        : basic_string_view{xmlStr.data(), xmlStr.size()}
    {
    }

    bool operator == (const xmlChar* otherStr) const;

    //NOLINTNEXTLINE(readability-identifier-naming) keep similarity to std::basic_string::c_str()
    const xmlChar* xs_str() const;

private:
    // this one is private to avoid non-null-terminated construction
    explicit constexpr XmlStringView(const char* str, std::size_t size)
        : basic_string_view{str, size} {};

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

    friend constexpr XmlStringView xml_string_literal::operator""_xs(const char* xmlStr, std::size_t size);
};

template<size_t strSize>
consteval XmlStringView::XmlStringView(const char (&str)[strSize])
    : basic_string_view{str, strSize - 1}
{
    if (strSize == 0 || str[strSize - 1] != 0)
    {
        // if str is not null terminated, you get a compiletime error here,
        // because the constructor is consteval and throw is not allowed there
        throw std::logic_error("string must be null terminated");
    }
}


static constexpr XmlStringView xhtmlNamespaceUri{"http://www.w3.org/1999/xhtml"};

namespace xml_string_literal
{
constexpr XmlStringView operator""_xs(const char* xmlStr, std::size_t size)
{
    return XmlStringView{xmlStr, size};
}
}

//NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast)
inline XmlStringView::XmlStringView(const xmlChar* xmlStr)
    : basic_string_view{reinterpret_cast<const char*>(xmlStr)}
{
}

inline const xmlChar* XmlStringView::xs_str() const
{
    return reinterpret_cast<const xmlChar*>(data());
}

inline bool XmlStringView::operator==(const xmlChar* otherStr) const
{
    return otherStr &&
           std::ranges::equal(begin(), end(), reinterpret_cast<const XmlCharType*>(otherStr), std::default_sentinel,
                              std::ranges::equal_to{}, &XmlStringView::toXmlCharType);
}

template<>
struct std::hash<XmlStringView> {
    std::size_t operator()(const XmlStringView& xstr) const
    {
        return hash<std::string_view>()(xstr);
    }
};

//NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)

//NOLINTEND(cppcoreguidelines-avoid-c-arrays, modernize-avoid-c-arrays)

#endif // FHIR_TOOLS_FHIR_INTERNAL_XMLHELPER_HXX
