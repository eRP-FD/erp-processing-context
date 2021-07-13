#include "erp/model/Composition.hxx"

#include <rapidjson/pointer.h>
#include <mutex>// for call_once

#include "ResourceNames.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/RapidjsonDocument.hxx"


namespace model
{

namespace
{

constexpr std::string_view composition_template = R"--(
 {
  "resourceType": "Composition",
  "id": "",
  "meta":{
    "profile":[
      "https://gematik.de/fhir/StructureDefinition/ErxComposition"
    ]
  },
  "extension":[
    {
      "url":"https://gematik.de/fhir/StructureDefinition/BeneficiaryExtension",
      "valueIdentifier":{
         "system":"https://gematik.de/fhir/NamingSystem/TelematikID",
         "value":""
       }
     }
  ],
  "status": "final",
  "type":{
    "coding":[
      {
        "system":"https://gematik.de/fhir/CodeSystem/Documenttype",
        "code":"3",
        "display":"Receipt"
      }
    ]
  },
  "date":"",
  "author":[
    {
      "reference":""
    }
  ],
  "title":"Quittung",
  "event": [
    {
      "period": {
        "start": "",
        "end": ""
      }
    }
  ]
}
)--";

std::once_flag onceFlag;
struct CompositionTemplateMark;
RapidjsonNumberAsStringParserDocument<CompositionTemplateMark> compositionTemplate;

void initTemplates ()
{
    rapidjson::StringStream s(composition_template.data());
    compositionTemplate->ParseStream<rapidjson::kParseNumbersAsStringsFlag, rapidjson::CustomUtf8>(s);
}

// definition of JSON pointers:
const rapidjson::Pointer idPointer ("/id");
const rapidjson::Pointer telematicIdPointer ("/extension/0/valueIdentifier/value");
const rapidjson::Pointer datePointer ("/date");
const rapidjson::Pointer eventPeriodStartPointer ("/event/0/period/start");
const rapidjson::Pointer eventPeriodEndPointer ("/event/0/period/end");
const rapidjson::Pointer authorPointer ("/author/0/reference");
const rapidjson::Pointer extensionPointer ("/extension");
const rapidjson::Pointer valueCodingCodeRelPointer ("/valueCoding/code");
const rapidjson::Pointer valueCodingSystemRelPointer("/valueCoding/system");
const rapidjson::Pointer urlRelPointer("/url");
}  // anonymous namespace


Composition::Composition(
    const std::string_view& telematicId,
    const model::Timestamp& start,
    const model::Timestamp& end,
    const std::string_view& author)
    : Resource<Composition>()
{
    std::call_once(onceFlag, initTemplates);

    initFromTemplate(*compositionTemplate);

    setValue(idPointer, Uuid().toString());
    setValue(telematicIdPointer, telematicId);
    setValue(datePointer, end.toXsDateTime());
    setValue(eventPeriodStartPointer, start.toXsDateTime());
    setValue(eventPeriodEndPointer, end.toXsDateTime());
    setValue(authorPointer, author);
}


Composition::Composition (NumberAsStringParserDocument&& jsonTree)
    : Resource<Composition>(std::move(jsonTree))
{
    std::call_once(onceFlag, initTemplates);
}


std::string_view Composition::id() const
{
    return getStringValue(idPointer);
}


std::optional<std::string_view> Composition::telematicId () const
{
    return getOptionalStringValue(telematicIdPointer);
}


std::optional<model::Timestamp> Composition::date() const
{
    const auto value = getOptionalStringValue(datePointer);
    if (value.has_value())
        return model::Timestamp::fromXsDateTime(std::string(value.value()));
    return {};
}


std::optional<model::Timestamp> Composition::periodStart() const
{
    const auto value = getOptionalStringValue(eventPeriodStartPointer);
    if (value.has_value())
        return model::Timestamp::fromXsDateTime(std::string(value.value()));
    return {};
}


std::optional<model::Timestamp> Composition::periodEnd() const
{
    const auto value = getOptionalStringValue(eventPeriodEndPointer);
    if (value.has_value())
        return model::Timestamp::fromXsDateTime(std::string(value.value()));
    return {};
}


std::optional<std::string_view> Composition::author () const
{
    return getOptionalStringValue(authorPointer);
}


std::optional<KbvStatusKennzeichen> Composition::legalBasisCode() const
{
    auto numericString(findStringInArray(extensionPointer, urlRelPointer,
                                         resource::structure_definition::kbcExForLegalBasis,
                                         valueCodingCodeRelPointer));
    if (! numericString.has_value())
        return std::nullopt;
    size_t idx = 0;
    auto numeric = std::stoi(std::string(*numericString), &idx, 10);
    ModelExpect(idx == numericString->size(), "not all characters processed by std::stoi");
    auto enumVal = magic_enum::enum_cast<KbvStatusKennzeichen>(numeric);
    ModelExpect(enumVal.has_value(), "Could not convert " + std::string(*numericString) + " to KbvStatusKennzeichen");
    return enumVal;
}


}  // namespace model
