/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/Binary.hxx"
#include "shared/model/ResourceNames.hxx"
#include "shared/model/RapidjsonDocument.hxx"

#include <rapidjson/pointer.h>
#include <mutex>// for call_once


namespace model
{
using namespace std::string_literals;

const std::string binary_template = R"--(
{
  "resourceType": "Binary",
  "id": "",
  "meta": {
    "profile": [
      ""
    ]
  },
  "contentType": "",
  "data" : ""
}
)--";

namespace
{

std::once_flag onceFlag;
struct BinaryTemplateMark;
RapidjsonNumberAsStringParserDocument<BinaryTemplateMark> BinaryTemplate;


void initTemplates()
{
    rapidjson::StringStream s(binary_template.data());
    BinaryTemplate->ParseStream<rapidjson::kParseNumbersAsStringsFlag, rapidjson::CustomUtf8>(s);
}

// definition of JSON pointers:
rapidjson::Pointer idPointer("/id");
rapidjson::Pointer contentTypePointer("/contentType");
rapidjson::Pointer dataPointer("/data");
rapidjson::Pointer metaVersionIdPointer{"/meta/versionId"};

}// anonymous namespace

Binary::Binary(std::string_view id, std::string_view data, const Type type,
               const std::optional<std::string_view>& metaVersionId)
    : Binary{id, data,
             [type]() -> Resource::Profile {
                 switch (type)
                 {
                     case Type::PKCS7:
                         return ProfileType::Gem_erxBinary;
                     case Type::Digest:
                         return ProfileType::Gem_erxDigest;
                 }
                 Fail("Unhandled type value.");
                 return Resource::NoProfile;
             }(),
             type, metaVersionId}
{
}

model::Binary::Binary(std::string_view id, std::string_view data, const Profile& profile, const Type type,
                      const std::optional<std::string_view>& metaVersionId)
    : Resource<Binary>(profile,
                       []() {
                           std::call_once(onceFlag, initTemplates);
                           return BinaryTemplate;
                       }()
                           .instance())
{
    setValue(idPointer, id);
    setValue(dataPointer, data);
    switch (type)
    {
        case Type::PKCS7:
            setValue(contentTypePointer, "application/pkcs7-mime");
            break;
        case Type::Digest:
            setValue(contentTypePointer, "application/octet-stream");
            break;
    }
    if (metaVersionId)
    {
        setValue(metaVersionIdPointer, *metaVersionId);
    }
}


Binary::Binary(NumberAsStringParserDocument&& jsonTree)
    : Resource<Binary>(std::move(jsonTree))
{
    std::call_once(onceFlag, initTemplates);
}

std::optional<std::string_view> Binary::id() const
{
    return getOptionalStringValue(idPointer);
}


std::optional<std::string_view> Binary::data() const
{
    return getOptionalStringValue(dataPointer);
}

}// namespace model
