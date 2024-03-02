/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/ErxReceipt.hxx"
#include "erp/fhir/Fhir.hxx"
#include "erp/model/Binary.hxx"
#include "erp/model/NumberAsStringParserDocument.hxx"
#include "erp/model/ResourceNames.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/RapidjsonDocument.hxx"

#include <rapidjson/pointer.h>
#include <mutex>// for call_once

namespace model
{

namespace
{
using namespace std::string_literals;

constexpr std::string_view prescriptionid_template = R"--(
{
  "system":"https://gematik.de/fhir/erp/NamingSystem/GEM_ERP_NS_PrescriptionId",
  "value":""
}
)--";


std::once_flag onceFlag;
struct PrescriptionIdTemplateMark;
RapidjsonNumberAsStringParserDocument<PrescriptionIdTemplateMark> prescriptionIdTemplate;


void initTemplates()
{
    rapidjson::StringStream s2(prescriptionid_template.data());
    prescriptionIdTemplate->ParseStream<rapidjson::kParseNumbersAsStringsFlag, rapidjson::CustomUtf8>(s2);
}

constexpr std::string_view resourceNameComposition("Composition");
constexpr std::string_view resourceNameDevice("Device");
constexpr ::std::string_view resourceNamePrescriptionDigest("Binary");

// definition of JSON pointers:
const rapidjson::Pointer identifierPointer("/identifier");
const rapidjson::Pointer prescriptionIdSystemPointer("/identifier/system");
const rapidjson::Pointer prescriptionIdPointer("/identifier/value");

}// namespace


ErxReceipt::ErxReceipt(const Uuid& bundleId, const std::string& selfLink, const model::PrescriptionId& prescriptionId,
                       const model::Composition& composition, const std::string& deviceIdentifier,
                       const model::Device& device, const ::std::string& prescriptionDigestIdentifier,
                       const model::Binary& prescriptionDigest,
                       ResourceVersion::DeGematikErezeptWorkflowR4 profileVersion)
    : BundleBase<ErxReceipt>(BundleType::document,
                             ResourceVersion::deprecatedProfile(profileVersion)
                                 ? resource::structure_definition::deprecated::receipt
                                 : resource::structure_definition::receipt,
                             bundleId)
{
    setLink(Link::Type::Self, selfLink);
    std::call_once(onceFlag, initTemplates);
    {
        auto entry = copyValue(*prescriptionIdTemplate);
        setValue(identifierPointer, entry);
        setValue(prescriptionIdPointer, prescriptionId.toString());
    }
    if (ResourceVersion::deprecatedProfile(profileVersion))
    {
        setValue(prescriptionIdSystemPointer, resource::naming_system::deprecated::prescriptionID);
    }
    addResource("urn:uuid:" + std::string{composition.id()}, {}, {}, composition.jsonDocument());
    addResource(deviceIdentifier, {}, {}, device.jsonDocument());

    ModelExpect(prescriptionDigest.data().has_value(), "Missing prescription message digest.");

    addResource(prescriptionDigestIdentifier, {}, {}, prescriptionDigest.jsonDocument());
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

model::Binary ErxReceipt::prescriptionDigest() const
{
    auto resources = getResourcesByType<::model::Binary>(resourceNamePrescriptionDigest);
    ModelExpect(resources.size() == 1, "Exactly one prescription digest resource expected");
    return std::move(resources.front());
}

}// namespace model
