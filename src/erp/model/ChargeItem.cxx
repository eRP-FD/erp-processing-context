/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/model/ChargeItem.hxx"
#include "erp/model/ResourceNames.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/RapidjsonDocument.hxx"

#include <rapidjson/pointer.h>

using namespace ::std::literals;

namespace model
{

using namespace resource;

namespace
{

const rapidjson::Pointer idPointer(ElementName::path(elements::id));
const rapidjson::Pointer subjectKvnrSystemPointer(ElementName::path(elements::subject, elements::identifier,
                                                                    elements::system));
const rapidjson::Pointer subjectKvnrValuePointer(ElementName::path(elements::subject, elements::identifier,
                                                                   elements::value));
const rapidjson::Pointer entererTelematikIdSystemPointer(ElementName::path(elements::enterer, elements::identifier,
                                                                           elements::system));
const rapidjson::Pointer entererTelematikIdValuePointer(ElementName::path(elements::enterer, elements::identifier,
                                                                          elements::value));
const rapidjson::Pointer enteredDatePointer(ElementName::path(elements::enteredDate));

const rapidjson::Pointer supportingInformationPointer(ElementName::path(elements::supportingInformation));
const rapidjson::Pointer typePointer(ElementName::path(elements::type));
const rapidjson::Pointer referencePointer(ElementName::path(elements::reference));
const rapidjson::Pointer displayPointer(ElementName::path(elements::display));

const rapidjson::Pointer extensionArrayPointer(ElementName::path(elements::extension));
const rapidjson::Pointer urlPointer(ElementName::path(elements::url));

const rapidjson::Pointer containedBinaryArrayPointer(ElementName::path("contained"));
const rapidjson::Pointer resourceTypePointer(ElementName::path(elements::resourceType));

const rapidjson::Pointer identifierPointer(ElementName::path("identifier"));
const rapidjson::Pointer identifierEntrySystemPointer(ElementName::path("system"));
const rapidjson::Pointer identifierEntrySystemValuePointer(ElementName::path("value"));

constexpr ::std::string_view charge_item_template = R"--(
{
    "resourceType": "ChargeItem",
    "meta": {
        "profile": [""]
    },
    "status": "billable",
    "code": {
        "coding": [
            {
                "system": "http://terminology.hl7.org/CodeSystem/data-absent-reason",
                "code": "not-applicable"
            }
        ]
    }
}
)--";

constexpr std::string_view identifierEntryTemplate = R"--(
{
  "system": "",
  "value": ""
}
)--";

std::once_flag onceFlag;

struct ChargeItemTemplateMark;
::RapidjsonNumberAsStringParserDocument<ChargeItemTemplateMark> chargeItemTemplate;

struct IdentifierEntryTemplate;
RapidjsonNumberAsStringParserDocument<IdentifierEntryTemplate> IdentifierEntryTemplate;

void initTemplates ()
{
    ::rapidjson::StringStream stream(charge_item_template.data());
    chargeItemTemplate->ParseStream<::rapidjson::kParseNumbersAsStringsFlag, ::rapidjson::CustomUtf8>(stream);

    rapidjson::StringStream stringStream{identifierEntryTemplate.data()};
    IdentifierEntryTemplate->ParseStream<rapidjson::kParseNumbersAsStringsFlag, rapidjson::CustomUtf8>(stringStream);
}

}  // anonymous namespace

//static
const std::unordered_map<ChargeItem::SupportingInfoType, std::pair<std::string_view, std::string_view>>
    ChargeItem::SupportingInfoTypeNames = {
        { ChargeItem::SupportingInfoType::prescriptionItem, { structure_definition::prescriptionItem, "E-Rezept" } },
        { ChargeItem::SupportingInfoType::dispenseItem, { structure_definition::dispenseItem, "Abgabedatensatz" } },
        { ChargeItem::SupportingInfoType::receipt, { structure_definition::receipt,  "Quittung" } } };

ChargeItem::ChargeItem()
    : Resource<ChargeItem>{"https://gematik.de/fhir/StructureDefinition/ErxChargeItem",
                           []() {
                               ::std::call_once(onceFlag, initTemplates);
                               return chargeItemTemplate;
                           }()
                               .instance()}
{
}

ChargeItem::ChargeItem (NumberAsStringParserDocument&& jsonTree)
    : Resource<ChargeItem>(std::move(jsonTree))
{
    std::call_once(onceFlag, initTemplates);
}

::std::optional<PrescriptionId> ChargeItem::id() const
{
    const auto id = getOptionalStringValue(idPointer);
    if (id)
    {
        return PrescriptionId::fromString(*id);
    }

    return {};
}

::std::optional<PrescriptionId> ChargeItem::prescriptionId() const
{
    const auto identifier =
        findStringInArray(identifierPointer, identifierEntrySystemPointer, resource::naming_system::prescriptionID,
                          identifierEntrySystemValuePointer);
    if (identifier)
    {

        return PrescriptionId::fromString(*identifier);
    }

    return {};
}

::std::optional<std::string_view> ChargeItem::subjectKvnr() const
{
    return getOptionalStringValue(subjectKvnrValuePointer);
}

::std::optional<std::string_view> ChargeItem::entererTelematikId() const
{
    return getOptionalStringValue(entererTelematikIdValuePointer);
}

::std::optional<Timestamp> ChargeItem::enteredDate() const
{
    const auto timestamp = getOptionalStringValue(enteredDatePointer);
    if (timestamp)
    {
        return Timestamp::fromFhirDateTime(::std::string{*timestamp});
    }

    return {};
}

std::optional<std::string_view> ChargeItem::supportingInfoReference(SupportingInfoType supportingInfoType) const
{
    const auto* elem = findMemberInArray(supportingInformationPointer, typePointer,
                                         SupportingInfoTypeNames.at(supportingInfoType).first);
    if (elem)
        return getOptionalStringValue(*elem, referencePointer);
    return {};
}

bool ChargeItem::isMarked() const
{
    const auto marking = markingFlag();
    return marking && ChargeItemMarkingFlag::isMarked(marking->getAllMarkings());
}

std::optional<model::ChargeItemMarkingFlag> ChargeItem::markingFlag() const
{
    return getExtension<ChargeItemMarkingFlag>();
}

std::optional<std::string_view> ChargeItem::accessCode() const
{
    return findStringInArray(identifierPointer, identifierEntrySystemPointer, resource::naming_system::accessCode,
                             identifierEntrySystemValuePointer);
}

std::optional<model::Binary> ChargeItem::containedBinary() const
{
    std::optional<model::Binary> result;
    const auto* containedBinaryResource = findMemberInArray(containedBinaryArrayPointer, resourceTypePointer, "Binary");
    if(containedBinaryResource)
    {
        result.emplace(model::Binary::fromJson(*containedBinaryResource));
    }
    return result;
}

void ChargeItem::setId(const PrescriptionId& prescriptionId)
{
    setValue(idPointer, prescriptionId.toString());
}

void ChargeItem::setPrescriptionId(const PrescriptionId& prescriptionId)
{
    const auto existingPrescriptionId =
        findMemberInArray(identifierPointer, identifierEntrySystemPointer, resource::naming_system::prescriptionID,
                          identifierEntrySystemValuePointer);

    if (existingPrescriptionId)
    {
        removeFromArray(identifierPointer, std::get<1>(existingPrescriptionId.value()));
    }

    auto copiedValue = copyValue(*IdentifierEntryTemplate);

    setKeyValue(copiedValue, identifierEntrySystemValuePointer, prescriptionId.toString());
    setKeyValue(copiedValue, identifierEntrySystemPointer, resource::naming_system::prescriptionID);

    addToArray(identifierPointer, std::move(copiedValue));
}

void ChargeItem::setSubjectKvnr(const std::string_view& kvnr)
{
    setValue(subjectKvnrSystemPointer, ::model::resource::naming_system::gkvKvid10);
    setValue(subjectKvnrValuePointer, kvnr);
}

void ChargeItem::setEntererTelematikId(const std::string_view& telematicId)
{
    setValue(entererTelematikIdSystemPointer, ::model::resource::naming_system::telematicID);
    setValue(entererTelematikIdValuePointer, telematicId);
}

void ChargeItem::setEnteredDate(const fhirtools::Timestamp& entered)
{
    setValue(enteredDatePointer, entered.toXsDateTime());
}

template<class ResourceType>
void ChargeItem::setSupportingInfoReference(SupportingInfoType supportingInfoType, const ResourceType& resource)
{
    const auto reference = [&resource]() {
        if constexpr (::std::is_same_v<ResourceType, ::model::Bundle>)
        {
            return "Bundle/"s + resource.getId().toString();
        }
        else
        {
            return ::std::string{resource};
        }
    }();

    const auto supportingInfoTypeData = SupportingInfoTypeNames.at(supportingInfoType);
    auto* elem = findMemberInArray(supportingInformationPointer, typePointer, supportingInfoTypeData.first);
    if (elem)
    {
        setKeyValue(*elem, referencePointer, reference);
    }
    else
    {
        auto entry = createEmptyObject();
        setKeyValue(entry, typePointer, supportingInfoTypeData.first);
        setKeyValue(entry, displayPointer, supportingInfoTypeData.second);
        setKeyValue(entry, referencePointer, reference);
        addToArray(supportingInformationPointer, std::move(entry));
    }
}

template void ChargeItem::setSupportingInfoReference<::model::Bundle>(SupportingInfoType supportingInfoType,
                                                                      const ::model::Bundle& resource);
template void ChargeItem::setSupportingInfoReference<::std::string>(SupportingInfoType supportingInfoType,
                                                                    const ::std::string& resource);
template void ChargeItem::setSupportingInfoReference<::std::string_view>(SupportingInfoType supportingInfoType,
                                                                         const ::std::string_view& resource);

void ChargeItem::setMarkingFlag(const ::model::Extension& markingFlag)
{
    const auto containedMarkingFlagAndPos =
        findMemberInArray(extensionArrayPointer, urlPointer, ChargeItemMarkingFlag::url, urlPointer);
    if (containedMarkingFlagAndPos)
    {
        removeFromArray(extensionArrayPointer, std::get<1>(containedMarkingFlagAndPos.value()));
    }
    auto entry = this->copyValue(markingFlag.jsonDocument());
    addToArray(extensionArrayPointer, std::move(entry));
}

void ChargeItem::setAccessCode(const std::string_view& accessCode)
{
    deleteAccessCode();

    auto copiedValue = copyValue(*IdentifierEntryTemplate);

    setKeyValue(copiedValue, identifierEntrySystemValuePointer, accessCode);
    setKeyValue(copiedValue, identifierEntrySystemPointer, resource::naming_system::accessCode);

    addToArray(identifierPointer, std::move(copiedValue));
}

void ChargeItem::deleteAccessCode()
{
    const auto existingAccessCode =
        findMemberInArray(identifierPointer, identifierEntrySystemPointer, resource::naming_system::accessCode,
                          identifierEntrySystemValuePointer);

    if (existingAccessCode)
    {
        removeFromArray(identifierPointer, std::get<1>(existingAccessCode.value()));
    }
}

void ChargeItem::deleteSupportingInfoReference(SupportingInfoType supportingInfoType)
{
    const auto supportInfoRefAndPos =
        findMemberInArray(supportingInformationPointer, typePointer,
                          SupportingInfoTypeNames.at(supportingInfoType).first, referencePointer);
    if (supportInfoRefAndPos)
    {
        removeFromArray(supportingInformationPointer, std::get<1>(supportInfoRefAndPos.value()));
        if (valueSize(supportingInformationPointer) == 0)// array empty => remove it
        {
            removeElement(supportingInformationPointer);
        }
    }
}

void ChargeItem::deleteContainedBinary()
{
    const auto containedBinaryResourceAndPos =
        findMemberInArray(containedBinaryArrayPointer, resourceTypePointer, "Binary", resourceTypePointer);
    if (containedBinaryResourceAndPos)
    {
        removeFromArray(containedBinaryArrayPointer, std::get<1>(containedBinaryResourceAndPos.value()));
        if (valueSize(containedBinaryArrayPointer) == 0)// array empty => remove it
        {
            removeElement(containedBinaryArrayPointer);
        }
    }
}

void ChargeItem::deleteMarkingFlag()
{
    const auto containedMarkingFlagAndPos =
        findMemberInArray(extensionArrayPointer, urlPointer, ChargeItemMarkingFlag::url, urlPointer);
    if (containedMarkingFlagAndPos)
    {
        removeFromArray(extensionArrayPointer, std::get<1>(containedMarkingFlagAndPos.value()));
        if (valueSize(extensionArrayPointer) == 0)// array empty => remove it
        {
            removeElement(extensionArrayPointer);
        }
    }
}

}// namespace model
