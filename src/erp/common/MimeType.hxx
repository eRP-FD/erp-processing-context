/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef E_LIBRARY_COMMON_MIMETYPE_HXX
#define E_LIBRARY_COMMON_MIMETYPE_HXX

#include <string>
#include <optional>
#include <unordered_map>

class Encoding
{
public:
    static const std::string utf8;
};

class MimeType
{
public:
    static const MimeType xml;
    static const MimeType html;
    static const MimeType binary; // octet-stream
    static const MimeType json;
    static const MimeType jose;
    static const MimeType fhirJson;
    static const MimeType fhirXml;


    explicit MimeType(std::string_view stringRepresentation);

    /// @return the full mime type without weight in the form MimeType/MimeSubtype
    std::string getMimeType() const;

    virtual operator std::string() const; // NOLINT google-explicit-constructor

    virtual ~MimeType() = default;

protected:
    std::optional<std::string> getOption(std::string_view name) const;

private:
    std::string mMimeType;
    std::string mMimeSubtype;
    std::unordered_map<std::string, std::string> mOptions;
};

bool operator==(const std::string& lhs, const MimeType& rhs);
bool operator==(const MimeType& lhs, const std::string& rhs);

/// @brief the MimeType used in the AcceptHeader of the HTTP-Request
class AcceptMimeType : public MimeType
{
public:
    /// @param stringRepresentation the string representation of the mime type, e.g. "text/html" or "*/*; q=0.8; x=y"
    explicit AcceptMimeType(std::string_view stringRepresentation);

    /// @return q-factor weight in the range [0,1] where 0 means not acceptable
    float getQFactorWeight() const;

private:
    float mQFactorWeight;
};

class ContentMimeType : public MimeType
{
public:
    static const ContentMimeType fhirJsonUtf8;
    static const ContentMimeType fhirXmlUtf8;


    /// @param stringRepresentation the string representation of the mime type, e.g. "text/html;charset=utf-8"
    explicit ContentMimeType(std::string_view stringRepresentation);

    operator std::string() const override; // NOLINT google-explicit-constructor

    /// @return the encoding in lower case, if present. E.g. utf-8
    std::optional<std::string> getEncoding() const;
};

#endif
