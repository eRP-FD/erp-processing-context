/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/model/MedicationDispense.hxx"
#include "shared/model/Extension.txx"
#include "shared/model/MedicationDispenseId.hxx"
#include "shared/model/RapidjsonDocument.hxx"
#include "shared/model/ResourceNames.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/util/Expect.hxx"

#include <rapidjson/pointer.h>
#include <mutex>// for call_once

using namespace model;
using namespace model::resource;

namespace model
{
namespace
{
/* Definition of JSON pointers:
--------------------------------

MedicationDispense is derived from (contains all elements) from DomainResource.
DomainResource is derived from (contains all elements) from Resource.

Profiles as defined by E-Rezept Workflow (https://simplifier.net/erezept-workflow/GemerxMedicationDispense)
have priority over profiles defined by FHIR (http://hl7.org/fhir/medicationdispense.html).
The cardinality is therefore taken from the E-Rezept Workflow.
The type (Member or Array) must still be determined from the cardinality of the FHIR specification.

                    | Name           | Type   |Card. | Pointer
--------------------+----------------+--------+------+-----------------------------------
Resource            | id             | Member | 0..1 | id
MedicationDispense  | prescriptionId | Array  | 1..1 | identifier/0/value
MedicationDispense  | kvnr           | Member | 1..1 | subject.identifier.value
MedicationDispense  | telematikId    | Array  | 1..1 | performer/0/actor.identifier.value
MedicationDispense  | whenHandedOver | Member | 1..1 | whenHandedOver
MedicationDispense  | whenPrepared   | Member | 0..1 | whenPrepared


Notes:
------

The cardinality of "id" would be 0..1.
The cardinality of "identifier" is 1..1 and is therefore mandatory.
The PrescriptionId is also used as "id" and therefore also "id" becomes mandatory.
The data type of "identifier/0/value" is PrescriptionId.
*/

const rapidjson::Pointer idPointer(ElementName::path(elements::id));
const rapidjson::Pointer prescriptionIdSystemPointer(ElementName::path(elements::identifier, 0, elements::system));
const rapidjson::Pointer prescriptionIdValuePointer(ElementName::path(elements::identifier, 0, elements::value));
const rapidjson::Pointer kvnrValuePointer(ElementName::path(elements::subject, elements::identifier, elements::value));
const rapidjson::Pointer kvnrSystemPointer(ElementName::path(elements::subject, elements::identifier,
                                                             elements::system));
const rapidjson::Pointer telematicIdSystemPointer(ElementName::path(elements::performer, 0, elements::actor,
                                                                    elements::identifier, elements::value));
const rapidjson::Pointer telematikIdValuePointer(ElementName::path(elements::performer, 0, elements::actor,
                                                                   elements::identifier, elements::value));
const rapidjson::Pointer whenHandedOverPointer(ElementName::path(elements::whenHandedOver));
const rapidjson::Pointer whenPreparedPointer(ElementName::path(elements::whenPrepared));
const rapidjson::Pointer medicationReferencePointer(ElementName::path(elements::medicationReference, elements::reference));

}// anonymous namespace


MedicationDispense::MedicationDispense(NumberAsStringParserDocument&& jsonTree)
    : Resource<MedicationDispense>(std::move(jsonTree))
{
}

ProfileType MedicationDispense::profileType() const
{
    auto profileName = getProfileName();
    ModelExpect(profileName.has_value(), "Profile missing in MedicationDispense resource.");
    auto profileType = findProfileType(*profileName);
    ModelExpect(profileType.has_value(), "Invalid Profile type in MedicationDispense resource.");
    return *profileType;
}

PrescriptionId MedicationDispense::prescriptionId() const
{
    std::string_view id = getStringValue(prescriptionIdValuePointer);
    return PrescriptionId::fromString(id);
}

Kvnr MedicationDispense::kvnr() const
{
    return Kvnr{getStringValue(kvnrValuePointer), getStringValue(kvnrSystemPointer)};
}

TelematikId MedicationDispense::telematikId() const
{
    return TelematikId{getStringValue(telematikIdValuePointer)};
}

model::Timestamp MedicationDispense::whenHandedOver() const
{
    std::string_view value = getStringValue(whenHandedOverPointer);
    return model::Timestamp::fromGermanDate(std::string(value));
}

std::optional<model::Timestamp> MedicationDispense::whenPrepared() const
{
    std::optional<std::string_view> value = getOptionalStringValue(whenPreparedPointer);
    if (value.has_value())
    {
        return model::Timestamp::fromGermanDate(std::string(value.value()));
    }
    return {};
}

MedicationDispenseId MedicationDispense::id() const
{
    return MedicationDispenseId::fromString(getStringValue(idPointer));
}

std::string_view MedicationDispense::medicationReference() const
{
    return getStringValue(medicationReferencePointer);
}

void MedicationDispense::setId(const MedicationDispenseId& id)
{
    setValue(idPointer, id.toString());
}

void MedicationDispense::setPrescriptionId(const PrescriptionId& prescriptionId)
{
    setValue(prescriptionIdValuePointer, prescriptionId.toString());
}

void MedicationDispense::setKvnr(const model::Kvnr& kvnr)
{
    setValue(kvnrValuePointer, kvnr.id());
    setValue(kvnrSystemPointer, kvnr.namingSystem());
}

void MedicationDispense::setTelematicId(const model::TelematikId& telematikId)
{
    setValue(telematikIdValuePointer, telematikId.id());
}

void MedicationDispense::setWhenHandedOver(const model::Timestamp& whenHandedOver)
{
    setValue(whenHandedOverPointer, whenHandedOver.toGermanDate());
}

void MedicationDispense::setWhenPrepared(const model::Timestamp& whenPrepared)
{
    setValue(whenPreparedPointer, whenPrepared.toGermanDate());
}

void MedicationDispense::setMedicationReference(std::string_view newReference)
{
    setValue(medicationReferencePointer, newReference);
}

void MedicationDispense::additionalValidation() const
{
    Resource<MedicationDispense>::additionalValidation();
    A_22073_01.start("check for date format YYYY-MM-DD");
    static_cast<void>(whenHandedOver());
    static_cast<void>(whenPrepared());
}

std::optional<Timestamp> MedicationDispense::getValidationReferenceTimestamp() const
{
    return whenHandedOver();
}

template class Extension<GemErpExRedeemCode>;

}// namespace model
