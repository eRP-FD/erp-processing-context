/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/ChargeItem.hxx"
#include "shared/model/ResourceNames.hxx"
#include "shared/util/Expect.hxx"
#include "shared/model/RapidjsonDocument.hxx"

#include <rapidjson/pointer.h>
#include <mutex>

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

const rapidjson::Pointer containedBinaryArrayPointer(ElementName::path(elements::contained));
const rapidjson::Pointer resourceTypePointer(ElementName::path(elements::resourceType));

const rapidjson::Pointer identifierPointer(ElementName::path(elements::identifier));
const rapidjson::Pointer identifierEntrySystemPointer(ElementName::path(elements::system));
const rapidjson::Pointer identifierEntrySystemValuePointer(ElementName::path(elements::value));

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
const std::unordered_map<ChargeItem::SupportingInfoType, std::string_view>
    ChargeItem::SupportingInfoDisplayNames = {
        {ChargeItem::SupportingInfoType::prescriptionItemBundle, structure_definition::prescriptionItem},
        {ChargeItem::SupportingInfoType::dispenseItemBundle, structure_definition::dispenseItem},
        {ChargeItem::SupportingInfoType::dispenseItemBinary, "Binary"},
        {ChargeItem::SupportingInfoType::receiptBundle, structure_definition::receipt}};

ChargeItem::ChargeItem()
    : Resource{profileType,
               []() {
                   ::std::call_once(onceFlag, initTemplates);
                   return chargeItemTemplate;
               }()
                   .instance()}
{
}

ChargeItem::ChargeItem(NumberAsStringParserDocument&& jsonTree)
    : Resource(std::move(jsonTree))
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

std::optional<Kvnr> ChargeItem::subjectKvnr() const
{
    auto kvnrValue = getOptionalStringValue(subjectKvnrValuePointer);
    if (! kvnrValue.has_value())
        return {};
    return Kvnr{*kvnrValue, Kvnr::Type::pkv};
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
    auto supportingInfoName = SupportingInfoDisplayNames.at(supportingInfoType);
    const auto* elem = findMemberInArray(supportingInformationPointer, displayPointer, supportingInfoName);
    if (elem)
        return getOptionalStringValue(*elem, referencePointer);
    return {};
}

bool ChargeItem::isMarked() const
{
    const auto marking = markingFlags();
    return marking && ChargeItemMarkingFlags::isMarked(marking->getAllMarkings());
}

std::optional<model::ChargeItemMarkingFlags> ChargeItem::markingFlags() const
{
    return getExtension<ChargeItemMarkingFlags>();
}

std::optional<std::string_view> ChargeItem::accessCode() const
{
    return findStringInArray(identifierPointer, identifierEntrySystemPointer, resource::naming_system::accessCode,
                             identifierEntrySystemValuePointer);
}

// GEMREQ-start containedBinary
std::optional<model::Binary> ChargeItem::containedBinary() const
{
    // extract the binary from "contained"
    const auto* containedBinaryResource = findMemberInArray(containedBinaryArrayPointer, resourceTypePointer, "Binary");
    if (! containedBinaryResource)
    {
        return {};
    }
    return model::Binary::fromJson(*containedBinaryResource);
}
// GEMREQ-end containedBinary

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

void ChargeItem::setSubjectKvnr(std::string_view kvnr)
{
    setValue(subjectKvnrSystemPointer, ::model::resource::naming_system::pkvKvid10);
    setValue(subjectKvnrValuePointer, kvnr);
}

void ChargeItem::setSubjectKvnr(const Kvnr& kvnr)
{
    setValue(subjectKvnrSystemPointer, ::model::resource::naming_system::pkvKvid10);
    setValue(subjectKvnrValuePointer, kvnr.id());
}

void ChargeItem::setEntererTelematikId(std::string_view telematicId)
{
    setValue(entererTelematikIdSystemPointer, ::model::resource::naming_system::telematicID);
    setValue(entererTelematikIdValuePointer, telematicId);
}

void ChargeItem::setEnteredDate(const model::Timestamp& entered)
{
    setValue(enteredDatePointer, entered.toXsDateTime());
}

// GEMREQ-start setSupportingInfoReference
void ChargeItem::setSupportingInfoReference(SupportingInfoType supportingInfoType)
{
    const auto supportingInfoTypeData = SupportingInfoDisplayNames.at(supportingInfoType);
    const auto reference = supportingInfoTypeToReference(supportingInfoType);
    auto* elem = findMemberInArray(supportingInformationPointer, displayPointer, supportingInfoTypeData);
    if (elem)
    {
        setKeyValue(*elem, referencePointer, reference);
    }
    else
    {
        auto entry = createEmptyObject();
        setKeyValue(entry, displayPointer, supportingInfoTypeData);
        setKeyValue(entry, referencePointer, reference);
        addToArray(supportingInformationPointer, std::move(entry));
    }
}
// GEMREQ-end setSupportingInfoReference


void ChargeItem::setMarkingFlags(const ChargeItemMarkingFlags& markingFlags)
{
    const auto containedMarkingFlagAndPos =
        findMemberInArray(extensionArrayPointer, urlPointer, ChargeItemMarkingFlags::url, urlPointer);
    if (containedMarkingFlagAndPos)
    {
        removeFromArray(extensionArrayPointer, std::get<1>(containedMarkingFlagAndPos.value()));
    }
    auto entry = this->copyValue(markingFlags.jsonDocument());
    const auto idx = addToArray(extensionArrayPointer, std::move(entry));

    namespace rj = rapidjson;
    const rj::Pointer markingArray{ElementName::path(elements::extension, idx, elements::extension)};
    for(const auto& markingUrl : ChargeItemMarkingFlags::allMarkingFlags)
    {
        if (!findMemberInArray(markingArray, rj::Pointer{ElementName::path(resource::elements::url)}, markingUrl))
        {
            ChargeItemMarkingFlag markingFlag(markingUrl, false);
            entry = this->copyValue(markingFlag.jsonDocument());
            addToArray(markingArray, std::move(entry));
        }
    }
}

void ChargeItem::setAccessCode(std::string_view accessCode)
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
        findMemberInArray(supportingInformationPointer, displayPointer,
                          SupportingInfoDisplayNames.at(supportingInfoType), referencePointer);
    if (supportInfoRefAndPos)
    {
        removeFromArray(supportingInformationPointer, std::get<1>(supportInfoRefAndPos.value()));
        if (valueSize(supportingInformationPointer) == 0)// array empty => remove it
        {
            removeElement(supportingInformationPointer);
        }
    }
}

// GEMREQ-start deleteContainedBinary
void ChargeItem::deleteContainedBinary(bool withSupportingReference)
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
    if (withSupportingReference)
        deleteSupportingInfoReference(SupportingInfoType::dispenseItemBinary);
}
// GEMREQ-end deleteContainedBinary

void ChargeItem::deleteMarkingFlag()
{
    const auto containedMarkingFlagAndPos =
        findMemberInArray(extensionArrayPointer, urlPointer, ChargeItemMarkingFlags::url, urlPointer);
    if (containedMarkingFlagAndPos)
    {
        removeFromArray(extensionArrayPointer, std::get<1>(containedMarkingFlagAndPos.value()));
        if (valueSize(extensionArrayPointer) == 0)// array empty => remove it
        {
            removeElement(extensionArrayPointer);
        }
    }
}
// GEMREQ-start supportingInfoTypeToReference
std::string ChargeItem::supportingInfoTypeToReference(ChargeItem::SupportingInfoType supportingInfoType) const
{
    const auto pId = prescriptionId();
    ModelExpect(pId.has_value(), "ChargeItem has no PrescriptionId assigned.");

    switch(supportingInfoType)
    {
        case SupportingInfoType::prescriptionItemBundle:
            return Uuid{pId->deriveUuid(uuidFeaturePrescription)}.toUrn();
        case SupportingInfoType::dispenseItemBundle:
            return Uuid{pId->deriveUuid(uuidFeatureDispenseItem)}.toUrn();
        case SupportingInfoType::dispenseItemBinary: {
            const auto contained = containedBinary();
            ModelExpect(contained.has_value(), "ChargeItem has no contained binary.");
            ModelExpect(contained->id().has_value(), "ChargeItem contained binary has no ID.");
            return "#" + std::string{contained->id().value()};
        }
        case SupportingInfoType::receiptBundle:
            return Uuid{pId->deriveUuid(uuidFeatureReceipt)}.toUrn();
    }
    Fail("invalid SupportingInfoType: " + std::to_string(::uintmax_t(supportingInfoType)));
}
// GEMREQ-end supportingInfoTypeToReference


std::optional<model::Timestamp> ChargeItem::getValidationReferenceTimestamp() const
{
    return Timestamp::now();
}

void ChargeItem::prepare()
{
    if (valueSize(containedBinaryArrayPointer) <= 1)
    {
        const rapidjson::Pointer containedBinaryArrayMetaPointer(ElementName::path(elements::contained, 0, elements::meta));
        removeElement(containedBinaryArrayMetaPointer);
    }
    else
    {
        TVLOG(2) << "Unexpected contained array size in charge item.";
    }
}

}// namespace model
