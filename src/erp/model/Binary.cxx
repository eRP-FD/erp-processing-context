/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/model/Binary.hxx"
#include "erp/util/RapidjsonDocument.hxx"

#include <rapidjson/pointer.h>
#include <mutex> // for call_once


namespace model {

constexpr std::string_view binary_template = R"--(
{
  "resourceType": "Binary",
  "id": "",
  "meta": {
    "versionId": "1",
    "profile": [
      "https://gematik.de/fhir/StructureDefinition/ErxBinary"
    ]
  },
  "contentType": "application/pkcs7-mime",
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
rapidjson::Pointer idPointer ("/id");
rapidjson::Pointer dataPointer ("/data");

}  // anonymous namespace


Binary::Binary(std::string_view id, std::string_view data)
    : Resource<Binary>()
{
    std::call_once(onceFlag, initTemplates);

    initFromTemplate(*BinaryTemplate);
    setValue(idPointer, id);
    setValue(dataPointer, data);
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