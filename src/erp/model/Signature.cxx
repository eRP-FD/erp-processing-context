/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/model/Signature.hxx"

#include <rapidjson/pointer.h>
#include <mutex>// for call_once

#include "erp/common/MimeType.hxx"
#include "erp/util/RapidjsonDocument.hxx"


namespace model
{

namespace
{

constexpr std::string_view signature_template = R"--(
{
  "type":[
    {
      "system":"urn:iso-astm:E1762-95:2013",
      "code":"1.2.840.10065.1.12.1.1"
    }
 ],
  "when":"",
  "who":{
    "reference":""
  },
  "sigFormat":"application/pkcs7-mime",
  "data":""
}
)--";

std::once_flag onceFlag;
struct SignatureTemplateMark;
RapidjsonNumberAsStringParserDocument<SignatureTemplateMark> signatureTemplate;

void initTemplates ()
{
    rapidjson::StringStream s(signature_template.data());
    signatureTemplate->ParseStream<rapidjson::kParseNumbersAsStringsFlag, rapidjson::CustomUtf8>(s);
}

// definition of JSON pointers:
const rapidjson::Pointer dataPointer ("/data");
const rapidjson::Pointer whenPointer ("/when");
const rapidjson::Pointer whoPointer ("/who/reference");
const rapidjson::Pointer targetFormatPointer ("/targetFormat");
const rapidjson::Pointer sigFormatPointer ("/sigFormat");
const rapidjson::Pointer typeSystemPointer ("/type/0/system");
const rapidjson::Pointer typeCodePointer ("/type/0/code");

}  // anonymous namespace


Signature::Signature(
    const std::string_view& data,
    const model::Timestamp& when,
    const std::string_view& who)
    : Resource<Signature>()
{
    std::call_once(onceFlag, initTemplates);

    initFromTemplate(*signatureTemplate);
    setValue(dataPointer, data);
    setValue(whenPointer, when.toXsDateTime());
    setValue(whoPointer, who);
}


Signature::Signature (NumberAsStringParserDocument&& jsonTree)
    : Resource<Signature>(std::move(jsonTree))
{
    std::call_once(onceFlag, initTemplates);
}


std::optional<std::string_view> Signature::data () const
{
    return getOptionalStringValue(dataPointer);
}


std::optional<model::Timestamp> Signature::when() const
{
    const auto value = getOptionalStringValue(whenPointer);
    if (value.has_value())
        return model::Timestamp::fromXsDateTime(std::string(value.value()));
    return {};
}


std::optional<std::string_view> Signature::who () const
{
    return getOptionalStringValue(whoPointer);
}


std::optional<MimeType> Signature::sigFormat() const
{
    auto mimeTypeStr = getOptionalStringValue(sigFormatPointer);
    if (mimeTypeStr)
        return MimeType(*mimeTypeStr);
    return std::nullopt;
}


std::optional<MimeType> Signature::targetFormat() const
{
    auto mimeTypeStr = getOptionalStringValue(targetFormatPointer);
    if (mimeTypeStr)
        return MimeType(*mimeTypeStr);
    return std::nullopt;
}


void Signature::setTargetFormat(const MimeType& format)
{
    setValue(targetFormatPointer, format.getMimeType());
}

void Signature::setSigFormat(const MimeType& format)
{
    setValue(sigFormatPointer, format.getMimeType());
}

void Signature::setType(std::string_view system, std::string_view code)
{
    setValue(typeSystemPointer, system);
    setValue(typeCodePointer, code);
}


}  // namespace model
