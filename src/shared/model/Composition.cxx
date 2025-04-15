/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/model/Composition.hxx"
#include "shared/model/RapidjsonDocument.hxx"
#include "shared/model/ResourceNames.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/Uuid.hxx"

#include <rapidjson/pointer.h>
#include <mutex>// for call_once

namespace model
{

namespace
{

const std::string composition_template = R"--(
 {
  "resourceType": "Composition",
  "id": "",
  "meta":{
    "profile":[
      ""
    ]
  },
  "extension":[
    {
      "url":"https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_EX_Beneficiary",
      "valueIdentifier":{
         "system":"https://gematik.de/fhir/sid/telematik-id",
         "value":""
       }
     }
  ],
  "status": "final",
  "type":{
    "coding":[
      {
        "system":"https://gematik.de/fhir/erp/CodeSystem/GEM_ERP_CS_DocumentType",
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
const rapidjson::Pointer telematikIdPointer("/extension/0/valueIdentifier/value");
const rapidjson::Pointer telematikIdSystemPointer("/extension/0/valueIdentifier/system");
const rapidjson::Pointer datePointer ("/date");
const rapidjson::Pointer eventPeriodStartPointer ("/event/0/period/start");
const rapidjson::Pointer eventPeriodEndPointer ("/event/0/period/end");
const rapidjson::Pointer authorArrayPointer ("/author");
const rapidjson::Pointer authorRelSystemPointer ("/identifier/system");
const rapidjson::Pointer authorPointer ("/author/0/reference");
const rapidjson::Pointer extensionPointer ("/extension");
const rapidjson::Pointer beneficiaryExtensionUrlPointer ("/extension/0/url");
const rapidjson::Pointer receiptCodingSystemPointer ("/type/coding/0/system");
const rapidjson::Pointer valueCodingCodeRelPointer ("/valueCoding/code");
const rapidjson::Pointer urlRelPointer("/url");
const rapidjson::Pointer section0Entry0ReferencePointer("/section/0/entry/0/reference");
}  // anonymous namespace


Composition::Composition(const std::string_view& telematicId, const model::Timestamp& start,
                         const model::Timestamp& end, const std::string_view& author,
                         const std::string_view& prescriptionDigestIdentifier)
    : Resource<Composition>(profileType,
                            []() {
                                std::call_once(onceFlag, initTemplates);
                                return compositionTemplate;
                            }()
                                .instance())
{
    setValue(idPointer, Uuid().toString());
    setValue(telematikIdPointer, telematicId);
    setValue(datePointer, end.toXsDateTime());
    setValue(eventPeriodStartPointer, start.toXsDateTime());
    setValue(eventPeriodEndPointer, end.toXsDateTime());
    setValue(authorPointer, author);
    setValue(section0Entry0ReferencePointer, prescriptionDigestIdentifier);
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


std::optional<std::string_view> Composition::telematikId() const
{
    return getOptionalStringValue(telematikIdPointer);
}


std::optional<model::Timestamp> Composition::date() const
{
    const auto value = getOptionalStringValue(datePointer);
    if (value.has_value())
        return model::Timestamp::fromFhirDateTime(std::string(value.value()));
    return {};
}


std::optional<model::Timestamp> Composition::periodStart() const
{
    const auto value = getOptionalStringValue(eventPeriodStartPointer);
    if (value.has_value())
        return model::Timestamp::fromFhirDateTime(std::string(value.value()));
    return {};
}


std::optional<model::Timestamp> Composition::periodEnd() const
{
    const auto value = getOptionalStringValue(eventPeriodEndPointer);
    if (value.has_value())
        return model::Timestamp::fromFhirDateTime(std::string(value.value()));
    return {};
}


std::optional<std::string_view> Composition::author () const
{
    return getOptionalStringValue(authorPointer);
}


std::optional<std::string_view> Composition::prescriptionDigestIdentifier() const
{
    return getOptionalStringValue(section0Entry0ReferencePointer);
}


std::optional<std::string_view> Composition::authorIdentifierSystem(size_t idx) const
{
    const auto* author = getMemberInArray(authorArrayPointer, idx);
    if (author)
    {
        return this->getOptionalStringValue(*author, authorRelSystemPointer);
    }
    return std::nullopt;
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
