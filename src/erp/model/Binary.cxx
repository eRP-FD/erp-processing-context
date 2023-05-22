/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/Binary.hxx"
#include "erp/model/ResourceNames.hxx"
#include "erp/util/RapidjsonDocument.hxx"

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
    "versionId": "1",
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

}// anonymous namespace


Binary::Binary(std::string_view id, std::string_view data, const Type type,
               ResourceVersion::DeGematikErezeptWorkflowR4 profileVersion)
    : Resource<Binary>(
          [type, profileVersion]() -> std::string_view {
              switch (type)
              {
                  case Type::PKCS7:
                      return ResourceVersion::deprecatedProfile(profileVersion)
                                 ? resource::structure_definition::deprecated::binary
                                 : resource::structure_definition::binary;
                      break;
                  case Type::Digest:
                      return ResourceVersion::deprecatedProfile(profileVersion)
                                 ? resource::structure_definition::deprecated::digest
                                 : resource::structure_definition::digest;
                      break;
              }

              Fail("Unhandled type value.");
              return "";
          }(),
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
