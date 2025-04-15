/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/model/Signature.hxx"

#include <rapidjson/pointer.h>
#include <mutex>// for call_once

#include "shared/network/message/MimeType.hxx"
#include "shared/model/RapidjsonDocument.hxx"


namespace model
{

namespace
{

constexpr const auto* signature_template = R"--(
{
  "type":[
    {
      "system":"urn:iso-astm:E1762-95:2013",
      "code":"1.2.840.10065.1.12.1.1"
    }
 ],
  "when":"",
  "who":{
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
    rapidjson::StringStream s(signature_template);
    signatureTemplate->ParseStream<rapidjson::kParseNumbersAsStringsFlag, rapidjson::CustomUtf8>(s);
}

// definition of JSON pointers:
const rapidjson::Pointer dataPointer ("/data");
const rapidjson::Pointer whenPointer ("/when");
const rapidjson::Pointer whoReferencePointer("/who/reference");
const rapidjson::Pointer whoDisplayPointer("/who/display");
const rapidjson::Pointer targetFormatPointer ("/targetFormat");
const rapidjson::Pointer sigFormatPointer ("/sigFormat");
const rapidjson::Pointer typeSystemPointer ("/type/0/system");
const rapidjson::Pointer typeCodePointer ("/type/0/code");

}  // anonymous namespace


Signature::Signature(const std::string_view& data, const model::Timestamp& when, const std::string_view& whoReference)
    : Signature(data, when, whoReference, std::nullopt)
{
}

Signature::Signature(const std::string_view& data, const Timestamp& when,
                     const std::optional<std::string_view> whoReference,
                     const std::optional<std::string_view> whoDisplay)
    : Resource<Signature>(FhirResourceBase::NoProfile,
                          []() {
                              std::call_once(onceFlag, initTemplates);
                              return signatureTemplate;
                          }()
                              .instance())
{
    setValue(dataPointer, data);
    setValue(whenPointer, when.toXsDateTime());
    Expect3(whoReference.has_value() || whoDisplay.has_value(), "Either whoReference or whoDisplay must be given",
            std::logic_error);
    if (whoReference.has_value())
    {
        setValue(whoReferencePointer, *whoReference);
    }
    if (whoDisplay.has_value())
    {
        setValue(whoDisplayPointer, *whoDisplay);
    }
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


std::optional<std::string_view> Signature::whoReference() const
{
    return getOptionalStringValue(whoReferencePointer);
}

std::optional<std::string_view> Signature::whoDisplay() const
{
    return getOptionalStringValue(whoDisplayPointer);
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
