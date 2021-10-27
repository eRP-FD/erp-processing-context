/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include <utility>

#include "erp/common/MimeType.hxx"

#include "erp/util/String.hxx"
#include "erp/util/Expect.hxx"

const std::string Encoding::utf8 = "utf-8";

const MimeType MimeType::xml("application/xml");
const MimeType MimeType::html("text/html");
const MimeType MimeType::binary("application/octet-stream");
const MimeType MimeType::json("application/json");
const MimeType MimeType::jose("application/jose");
const MimeType MimeType::fhirJson("application/fhir+json");
const MimeType MimeType::fhirXml("application/fhir+xml");

const ContentMimeType ContentMimeType::fhirJsonUtf8("application/fhir+json;charset=utf-8");
const ContentMimeType ContentMimeType::fhirXmlUtf8("application/fhir+xml;charset=utf-8");

MimeType::MimeType(std::string_view stringRepresentation)
    :  mMimeType(), mMimeSubtype(), mOptions()
{
    // e.g.: application/xml;q=0.9;x=y
    const auto splitBySlash = String::split(stringRepresentation, '/');
    ErpExpect(splitBySlash.size() == 2, HttpStatus::BadRequest,
           "wrong format of MimeType: " + std::string(stringRepresentation));
    mMimeType = String::toLower(String::trim(splitBySlash[0]));
    ErpExpect(! mMimeType.empty(), HttpStatus::BadRequest,
              "wrong format of MimeType: " + std::string(stringRepresentation));

    // e.g.: xml;q=0.9;x=y
    const auto splitBySemicolon = String::split(String::trim(splitBySlash[1]), ';');
    Expect3(!splitBySemicolon.empty(), "String::split function defect", std::logic_error);

    mMimeSubtype = String::toLower(String::trim(splitBySemicolon[0]));
    ErpExpect(! mMimeSubtype.empty(), HttpStatus::BadRequest,
              "wrong format of MimeType: " + std::string(stringRepresentation));

    if (splitBySemicolon.size() > 1)
    {
        // e.g.: q=0.9;x=y;charset=utf8
        for (size_t i = 1; i < splitBySemicolon.size(); ++i)
        {
            const auto splitByEqualSign = String::split(splitBySemicolon[i], '=');
            ErpExpect(splitByEqualSign.size() == 2, HttpStatus::BadRequest,
                      "wrong format of MimeType: " + std::string(stringRepresentation));
            mOptions.emplace(String::trim(String::toLower(splitByEqualSign[0])), String::trim(splitByEqualSign[1]));
        }
    }
}

AcceptMimeType::AcceptMimeType(std::string_view stringRepresentation)
    : MimeType(stringRepresentation), mQFactorWeight(1.f)
{
    auto qOption = getOption("q");
    if (qOption)
    {
        try
        {
            mQFactorWeight = std::stof(qOption.value());
        }
        catch (const std::exception&)
        {
            ErpFail(HttpStatus::BadRequest, "wrong format of MimeType: " + std::string(stringRepresentation));
        }
        ErpExpect(mQFactorWeight >= 0.0f && mQFactorWeight <= 1.0f, HttpStatus::BadRequest,
               "q-factor out of range [0,1]: " + qOption.value());
    }
}

std::string MimeType::getMimeType() const
{
    return mMimeType + "/" + mMimeSubtype;
}

MimeType::operator std::string() const
{
    return getMimeType();
}

std::optional<std::string> MimeType::getOption(std::string_view name) const
{
    auto found = mOptions.find(String::toLower(std::string(name)));
    if (found != mOptions.end())
    {
        return found->second;
    }
    return {};
}

bool operator==(const std::string& lhs, const MimeType& rhs)
{
    return lhs == rhs.getMimeType();
}
bool operator==(const MimeType& lhs, const std::string& rhs)
{
    return operator==(rhs, lhs);
}

float AcceptMimeType::getQFactorWeight() const
{
    return mQFactorWeight;
}


ContentMimeType::ContentMimeType(std::string_view stringRepresentation)
    : MimeType(stringRepresentation)
{
}

std::optional<std::string> ContentMimeType::getEncoding() const
{
    auto encoding = getOption("charset");
    if (encoding)
        return String::toLower(encoding.value());
    return {};
}

ContentMimeType::operator std::string() const
{
    auto encoding = getEncoding();
    if (encoding)
        return getMimeType() + ";charset=" + encoding.value();
    return getMimeType();
}
