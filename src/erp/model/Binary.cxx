/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/model/Binary.hxx"
#include "erp/util/RapidjsonDocument.hxx"

#include <rapidjson/pointer.h>
#include <mutex> // for call_once


namespace model {
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


void initTemplates ()
{
    rapidjson::StringStream s(binary_template.data());
    BinaryTemplate->ParseStream<rapidjson::kParseNumbersAsStringsFlag, rapidjson::CustomUtf8>(s);
}

// definition of JSON pointers:
rapidjson::Pointer idPointer("/id");
rapidjson::Pointer contentTypePointer("/contentType");
rapidjson::Pointer dataPointer("/data");

}  // anonymous namespace


Binary::Binary(std::string_view id, std::string_view data, const Type type)
    : Resource<Binary>(
          [type]() {
              switch (type)
              {
                  case Type::PKCS7:
                      return "https://gematik.de/fhir/StructureDefinition/ErxBinary";
                      break;
                  case Type::Base64:
                      return "http://hl7.org/fhir/StructureDefinition/Binary";
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
        case Type::Base64:
            setValue(contentTypePointer, "application/octet-stream");
            break;
    }
}


Binary::Binary (NumberAsStringParserDocument&& jsonTree)
    : Resource<Binary>(std::move(jsonTree))
{
    std::call_once(onceFlag, initTemplates);
}

std::optional<std::string_view> Binary::id () const
{
    return getOptionalStringValue(idPointer);
}


std::optional<std::string_view> Binary::data () const
{
    return getOptionalStringValue(dataPointer);
}

}  // namespace model