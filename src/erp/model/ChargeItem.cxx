/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/model/ChargeItem.hxx"
#include "erp/model/ResourceNames.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/RapidjsonDocument.hxx"

#include <rapidjson/pointer.h>


namespace model
{

using namespace resource;

namespace
{

const rapidjson::Pointer idPointer(ElementName::path(elements::id));
const rapidjson::Pointer prescriptionIdValuePointer(ElementName::path(elements::identifier, 0, elements::value));
const rapidjson::Pointer subjectKvnrValuePointer(ElementName::path(elements::subject, elements::identifier, elements::value));
const rapidjson::Pointer entererTelematikIdValuePointer(ElementName::path(elements::enterer, elements::identifier, elements::value));
const rapidjson::Pointer enteredDatePointer(ElementName::path(elements::enteredDate));

const rapidjson::Pointer supportingInformationPointer(ElementName::path(elements::supportingInformation));
const rapidjson::Pointer typePointer(ElementName::path(elements::type));
const rapidjson::Pointer referencePointer(ElementName::path(elements::reference));
const rapidjson::Pointer displayPointer(ElementName::path(elements::display));

const rapidjson::Pointer extensionPointer(ElementName::path(elements::extension));
const rapidjson::Pointer urlPointer(ElementName::path(elements::url));

}  // anonymous namespace

//static
const std::unordered_map<ChargeItem::SupportingInfoType, std::pair<std::string_view, std::string_view>>
    ChargeItem::SupportingInfoTypeNames = {
        { ChargeItem::SupportingInfoType::prescriptionItem, { structure_definition::prescriptionItem, "E-Rezept" } },
        { ChargeItem::SupportingInfoType::dispenseItem, { structure_definition::dispenseItem, "Abgabedatensatz" } },
        { ChargeItem::SupportingInfoType::receipt, { structure_definition::receipt,  "Quittung" } } };


ChargeItem::ChargeItem (NumberAsStringParserDocument&& jsonTree)
    : Resource<ChargeItem>(std::move(jsonTree))
{
}

PrescriptionId ChargeItem::id() const
{
    std::string_view id = getStringValue(prescriptionIdValuePointer);
    return PrescriptionId::fromString(id);
}

std::string_view ChargeItem::subjectKvnr() const
{
    return getStringValue(subjectKvnrValuePointer);
}

std::string_view ChargeItem::entererTelematikId() const
{
    return getStringValue(entererTelematikIdValuePointer);
}

Timestamp ChargeItem::enteredDate() const
{
    return Timestamp::fromFhirDateTime(std::string(getStringValue(enteredDatePointer)));
}

std::optional<std::string_view> ChargeItem::supportingInfoReference(SupportingInfoType supportingInfoType) const
{
    const auto* elem = findMemberInArray(
        supportingInformationPointer, typePointer, SupportingInfoTypeNames.at(supportingInfoType).first);
    if(elem)
        return getOptionalStringValue(*elem, referencePointer);
    return {};
}

bool ChargeItem::isMarked() const
{
    const auto* extensionElem = findMemberInArray(extensionPointer, urlPointer, structure_definition::markingFlag);
    bool result = false;
    if(extensionElem)
    {
        for(auto pos = 0; !result; ++pos)
        {
            const rapidjson::Pointer markingPointer(ElementName::path(elements::extension, pos, elements::valueBoolean));
            const auto* markingValue = getValue(*extensionElem, markingPointer);
            if(!markingValue)
                break;
            ModelExpect(markingValue->IsBool(), ResourceBase::pointerToString(markingPointer) + ": element must be of type boolean");
            result = markingValue->GetBool();
        }
    }
    return result;
}

void ChargeItem::setId(const PrescriptionId& prescriptionId)
{
    setValue(idPointer, prescriptionId.toString());

    // Please note that the prescriptionId of the task is also used as the id of the resource.
    setIdentifier(prescriptionId);
}

void ChargeItem::setIdentifier(const PrescriptionId& prescriptionId)
{
    setValue(prescriptionIdValuePointer, prescriptionId.toString());
}

void ChargeItem::setSubjectKvnr(const std::string_view& kvnr)
{
    setValue(subjectKvnrValuePointer, kvnr);
}

void ChargeItem::setEntererTelematikId(const std::string_view& telematicId)
{
    setValue(entererTelematikIdValuePointer, telematicId);
}

void ChargeItem::setEnteredDate(const model::Timestamp& entered)
{
    setValue(enteredDatePointer, entered.toXsDateTime());
}

void ChargeItem::setSupportingInfoReference(SupportingInfoType supportingInfoType, const std::string_view& reference)
{
    const auto supportingInfoTypeData = SupportingInfoTypeNames.at(supportingInfoType);
    auto elem = findMemberInArray(supportingInformationPointer, typePointer, supportingInfoTypeData.first);
    if(elem)
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

void ChargeItem::deleteSupportingInfoElement(SupportingInfoType supportingInfoType)
{
    const auto supportInfoRefAndPos = findMemberInArray(
        supportingInformationPointer, typePointer, SupportingInfoTypeNames.at(supportingInfoType).first, referencePointer);
    if (supportInfoRefAndPos) {
        removeFromArray(supportingInformationPointer, std::get<1>(supportInfoRefAndPos.value()));
        if(valueSize(supportingInformationPointer) == 0) // array empty => remove it
            removeElement(supportingInformationPointer);
    }
}


} // namespace model
