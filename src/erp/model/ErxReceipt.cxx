/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/model/ErxReceipt.hxx"
#include "erp/model/NumberAsStringParserDocument.hxx"

#include "erp/fhir/Fhir.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/RapidjsonDocument.hxx"

#include <rapidjson/pointer.h>
#include <mutex> // for call_once


namespace model
{

namespace
{

constexpr std::string_view profile_template = R"--(
{
  "profile":[
    "https://gematik.de/fhir/StructureDefinition/ErxReceipt"
  ]
}
)--";

constexpr std::string_view prescriptionid_template = R"--(
{
  "system":"https://gematik.de/fhir/NamingSystem/PrescriptionID",
  "value":""
}
)--";


std::once_flag onceFlag;
struct ProfileTemplateMark;
RapidjsonNumberAsStringParserDocument<ProfileTemplateMark> profileTemplate;
struct PrescriptionIdTemplateMark;
RapidjsonNumberAsStringParserDocument<PrescriptionIdTemplateMark> prescriptionIdTemplate;


void initTemplates ()
{
    rapidjson::StringStream s1(profile_template.data());
    profileTemplate->ParseStream<rapidjson::kParseNumbersAsStringsFlag, rapidjson::CustomUtf8>(s1);

    rapidjson::StringStream s2(prescriptionid_template.data());
    prescriptionIdTemplate->ParseStream<rapidjson::kParseNumbersAsStringsFlag, rapidjson::CustomUtf8>(s2);
}

constexpr std::string_view resourceNameComposition ("Composition");
constexpr std::string_view resourceNameDevice ("Device");

// definition of JSON pointers:
const rapidjson::Pointer metaPointer("/meta");
const rapidjson::Pointer identifierPointer("/identifier");
const rapidjson::Pointer prescriptionIdPointer ("/identifier/value");

}  // anonymous namespace


ErxReceipt::ErxReceipt(
    const Uuid& bundleId,
    const std::string& selfLink,
    const model::PrescriptionId& prescriptionId,
    const model::Composition& composition,
    const std::string& deviceIdentifier,
    const model::Device& device)
    : Bundle(Type::document, bundleId)
{
    setLink(Link::Type::Self, selfLink);
    std::call_once(onceFlag, initTemplates);
    {
        auto entry = copyValue(*profileTemplate);
        setValue(metaPointer, entry);
    }
    {
        auto entry = copyValue(*prescriptionIdTemplate);
        setValue(identifierPointer, entry);
        setValue(prescriptionIdPointer, prescriptionId.toString());
    }
    addResource("urn:uuid:" + std::string{composition.id()}, {}, {}, composition.jsonDocument());
    addResource(deviceIdentifier, {}, {}, device.jsonDocument());
}


ErxReceipt::ErxReceipt(NumberAsStringParserDocument&& jsonTree)
    : Bundle(std::move(jsonTree))
{
}


ErxReceipt ErxReceipt::fromJson(const std::string_view& jsonStr)
{
    NumberAsStringParserDocument document;
    rapidjson::StringStream s(jsonStr.data());
    document.ParseStream<rapidjson::kParseNumbersAsStringsFlag, rapidjson::CustomUtf8>(s);
    ModelExpect(!document.HasParseError(), "can not parse json string");
    return ErxReceipt(std::move(document));
}

ErxReceipt ErxReceipt::fromJson(const rapidjson::Value& json)
{
    NumberAsStringParserDocument document;
    document.CopyFrom(json, document.GetAllocator());
    return ErxReceipt(std::move(document));
}

ErxReceipt ErxReceipt::fromXml(const std::string& xmlStr)
{
    try
    {
        return ErxReceipt{Fhir::instance().converter().xmlStringToJson(xmlStr)};
    }
    catch (const std::runtime_error& e)
    {
        std::throw_with_nested(ModelException(e.what()));
    }
}


model::PrescriptionId ErxReceipt::prescriptionId() const
{
    return model::PrescriptionId::fromString(getStringValue(prescriptionIdPointer));
}


model::Composition ErxReceipt::composition() const
{
    auto resources = getResourcesByType<model::Composition>(resourceNameComposition);
    ModelExpect(resources.size() == 1, "Exactly one composition resource expected");
    return std::move(resources.front());
}


model::Device ErxReceipt::device() const
{
    auto resources = getResourcesByType<model::Device>(resourceNameDevice);
    ModelExpect(resources.size() == 1, "Exactly one device resource expected");
    return std::move(resources.front());
}


}  // namespace model
