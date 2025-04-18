/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/Task.hxx"
#include "fhirtools/repository/DefinitionKey.hxx"
#include "fhirtools/util/Gsl.hxx"
#include "shared/ErpConstants.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/model/Composition.hxx"
#include "shared/model/Kvnr.hxx"
#include "shared/model/RapidjsonDocument.hxx"
#include "shared/model/ResourceNames.hxx"
#include "shared/model/Timestamp.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/WorkDay.hxx"

#include <date/tz.h>
#include <rapidjson/pointer.h>
#include <mutex>// for call_once

using  model::Timestamp;
using namespace std::literals::string_view_literals;

namespace model
{
using namespace std::string_literals;
const std::string task_template = R"--(
{
  "resourceType": "Task",
  "id": "",
  "meta": {
    "profile": [
      ""
    ]
  },
  "identifier": [
    {
      "use": "official",
      "system": "https://gematik.de/fhir/erp/NamingSystem/GEM_ERP_NS_PrescriptionId",
      "value": ""
    }
  ],
  "intent": "order",
  "status": "draft",
  "extension": [{
    "url": "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_EX_PrescriptionType",
    "valueCoding": {
        "system": "https://gematik.de/fhir/erp/CodeSystem/GEM_ERP_CS_FlowType",
        "code": "",
        "display": ""
    }
  }],
  "authoredOn": "",
  "lastModified": "",
  "performerType": [
    {
      "coding": [
        {
          "system": "https://gematik.de/fhir/erp/CodeSystem/GEM_ERP_CS_OrganizationType",
          "code": "",
          "display": ""
        }
      ],
      "text": ""
    }
  ]
}
)--";

constexpr const auto* prescriptionReference_template = R"--(
{
  "type": {
    "coding": [
      {
        "system": "https://gematik.de/fhir/erp/CodeSystem/GEM_ERP_CS_DocumentType",
        "code": ""
      }
    ]
  },
  "valueReference": {
    "reference": ""
  }
}
)--";

constexpr const auto* secret_template = R"--(
{
  "use": "official",
  "system": "",
  "value": ""
}
)--";

constexpr const auto* extension_date_template = R"--(
{
  "url": "",
  "valueDate": ""
}
)--";

constexpr const auto* extension_instant_template = R"--(
{
  "url": "",
  "valueInstant": ""
}
)--";

const std::unordered_map<Task::Status, std::string_view> Task::StatusNames = {
    {Task::Status::draft, "draft"},
    {Task::Status::ready, "ready"},
    {Task::Status::inprogress, "in-progress"},
    {Task::Status::completed, "completed"},
    {Task::Status::cancelled, "cancelled"}};
const std::unordered_map<std::string_view, Task::Status> Task::StatusNamesReverse = {
    {"draft", Task::Status::draft},
    {"ready", Task::Status::ready},
    {"in-progress", Task::Status::inprogress},
    {"completed", Task::Status::completed},
    {"cancelled", Task::Status::cancelled}};

int8_t Task::toNumericalStatus(Task::Status status)
{
    return magic_enum::enum_integer(status);
}
Task::Status Task::fromNumericalStatus(int8_t status)
{
    return magic_enum::enum_cast<Task::Status>(status).value();
}

namespace
{

std::once_flag onceFlag;
struct TaskTemplateMark;
RapidjsonNumberAsStringParserDocument<TaskTemplateMark> TaskTemplate;
struct PrescriptionReferenceTemplateMark;
RapidjsonNumberAsStringParserDocument<PrescriptionReferenceTemplateMark> PrescriptionReferenceTemplate;
struct SecretAccessCodeTemplate;
RapidjsonNumberAsStringParserDocument<SecretAccessCodeTemplate> SecretAccessCodeTemplate;
struct ExtensionDateTemplateMark;
RapidjsonNumberAsStringParserDocument<ExtensionDateTemplateMark> ExtensionDateTemplate;
struct ExtensionInstantTemplateMark;
RapidjsonNumberAsStringParserDocument<ExtensionInstantTemplateMark> ExtensionInstantTemplate;

void initTemplates()
{
    rapidjson::StringStream s1(task_template.data());
    TaskTemplate->ParseStream<rapidjson::kParseNumbersAsStringsFlag, rapidjson::CustomUtf8>(s1);

    rapidjson::StringStream s2(prescriptionReference_template);
    PrescriptionReferenceTemplate->ParseStream<rapidjson::kParseNumbersAsStringsFlag, rapidjson::CustomUtf8>(s2);

    rapidjson::StringStream s3(secret_template);
    SecretAccessCodeTemplate->ParseStream<rapidjson::kParseNumbersAsStringsFlag, rapidjson::CustomUtf8>(s3);

    rapidjson::StringStream s4(extension_date_template);
    ExtensionDateTemplate->ParseStream<rapidjson::kParseNumbersAsStringsFlag, rapidjson::CustomUtf8>(s4);

    rapidjson::StringStream s5(extension_instant_template);
    ExtensionInstantTemplate->ParseStream<rapidjson::kParseNumbersAsStringsFlag, rapidjson::CustomUtf8>(s5);
}

// definition of JSON pointers:
const rapidjson::Pointer forPointer("/for");
const rapidjson::Pointer kvnrPointer("/for/identifier/value");
const rapidjson::Pointer kvnrSysPointer("/for/identifier/system");
const rapidjson::Pointer authoredOnPointer("/authoredOn");
const rapidjson::Pointer lastModifiedPointer("/lastModified");
const rapidjson::Pointer statusPointer("/status");
const rapidjson::Pointer taskIdPointer("/id");
const rapidjson::Pointer prescriptionIdPointer("/identifier/0/value");
const rapidjson::Pointer prescriptionIdSystemPointer("/identifier/0/system");
const rapidjson::Pointer flowtypeUrlPointer("/extension/0/url");
const rapidjson::Pointer flowtypeSystemPointer("/extension/0/valueCoding/system");
const rapidjson::Pointer flowtypeCodePointer("/extension/0/valueCoding/code");
const rapidjson::Pointer flowtypeDisplayPointer("/extension/0/valueCoding/display");
const rapidjson::Pointer performerTypePointer("/performerType/0/coding/0/code");
const rapidjson::Pointer performerTypeDisplayPointer("/performerType/0/coding/0/display");
const rapidjson::Pointer performerTypeSystemPointer("/performerType/0/coding/0/system");
const rapidjson::Pointer performerTypeTextPointer("/performerType/0/text");
const rapidjson::Pointer inputArrayPointer("/input");
const rapidjson::Pointer outputArrayPointer("/output");
const rapidjson::Pointer identifierArrayPointer("/identifier");
const rapidjson::Pointer extensionArrayPointer("/extension");
const rapidjson::Pointer ownerIdentifierSystemPointer("/owner/identifier/system");
const rapidjson::Pointer ownerIdentifierValuePointer("/owner/identifier/value");

// relative pointers
// The coding array is fixed length 1
const rapidjson::Pointer codePointerRelToInOutArray("/type/coding/0/code");
const rapidjson::Pointer systemPointerRelToInOutArray("/type/coding/0/system");
const rapidjson::Pointer valueStringPointerRelToInOutArray("/valueReference/reference");
const rapidjson::Pointer systemRelPointer("/system");
const rapidjson::Pointer valueRelPointer ("/value");

constexpr std::string_view codeInputHealthCareProviderPrescription = "1";
constexpr std::string_view codeInputPatientConfirmation = "2";
constexpr std::string_view codeOutputReceipt = "3";

} // namespace


Task::Task(const model::PrescriptionType prescriptionType, const std::optional<std::string_view>& accessCode,
           Timestamp lastStatusChange)
    : Resource<Task>(profileType,
                     []() {
                         std::call_once(onceFlag, initTemplates);

                         A_19114.start("create Task in status draft");
                         return TaskTemplate;
                         A_19114.finish();
                     }()
                         .instance())
    , mLastStatusChange(lastStatusChange)
{
    if (accessCode.has_value())
    {
        setAccessCode(*accessCode);
    }
    A_19112.start("set Task.extension:flowType to the value of prescriptionType");
    setValue(flowtypeCodePointer,
             std::to_string(
                 static_cast<std::underlying_type_t<model::PrescriptionType>>(prescriptionType)));
    setValue(flowtypeDisplayPointer, PrescriptionTypeDisplay.at(prescriptionType));
    A_19112.finish();

    A_19214.start("set Task.performerType corresponding to the value of prescriptionType");
    A_19445_10.start("set Task.performerType corresponding to the value of prescriptionType");
    setValue(performerTypePointer, PrescriptionTypePerformerType.at(prescriptionType));
    setValue(performerTypeDisplayPointer, PrescriptionTypePerformerDisplay.at(prescriptionType));
    setValue(performerTypeTextPointer, PrescriptionTypePerformerDisplay.at(prescriptionType));
    A_19445_10.finish();
    A_19214.finish();

    setValue(authoredOnPointer, Timestamp::now().toXsDateTime());

    updateLastUpdate();
}

Task::Task(const PrescriptionId& id, PrescriptionType prescriptionType, Timestamp lastModified, Timestamp authoredOn,
           Task::Status status, Timestamp lastStatusChange)
    : Task(prescriptionType, {}, lastStatusChange)
{
    setPrescriptionId(id);
    updateLastUpdate(lastModified);
    setValue(authoredOnPointer, authoredOn.toXsDateTime());
    setStatus(status, lastStatusChange);
}

void Task::setPrescriptionId(const PrescriptionId& prescriptionId)
{
    const auto prescriptionIdString = prescriptionId.toString();
    setValue(prescriptionIdPointer, prescriptionIdString);
    setValue(taskIdPointer, prescriptionIdString);
}

Task::Status Task::status() const
{
    const auto statusStr = getStringValue(statusPointer);
    return StatusNamesReverse.at(statusStr);
}

std::optional<Kvnr> Task::kvnr() const
{
    auto kvid10 = getOptionalStringValue(kvnrSysPointer);
    auto kvnr = getOptionalStringValue(kvnrPointer);
    if (! kvid10.has_value() || ! kvnr.has_value())
        return {};
    return Kvnr{*kvnr, *kvid10};
}

PrescriptionId Task::prescriptionId() const
{
    return PrescriptionId::fromString(getStringValue(prescriptionIdPointer));
}

Timestamp Task::authoredOn() const
{
    return Timestamp::fromFhirDateTime(std::string(getStringValue(authoredOnPointer)));
}

PrescriptionType Task::type() const
{
    return static_cast<PrescriptionType>(
        std::stoi(std::string(getStringValue(flowtypeCodePointer))));
}

Timestamp Task::lastModifiedDate() const
{
    return Timestamp::fromFhirDateTime(std::string(getStringValue(lastModifiedPointer)));
}

std::string_view Task::accessCode() const
{
    auto accessCode = findStringInArray(identifierArrayPointer, systemRelPointer, resource::naming_system::accessCode,
                                        valueRelPointer);
    ModelExpect(accessCode.has_value(), "AccessCode not set in task");
    return *accessCode;
}

std::optional<std::string_view> Task::secret() const
{
    return findStringInArray(identifierArrayPointer, systemRelPointer, resource::naming_system::secret,
                             valueRelPointer);
}

std::optional<std::string_view> Task::healthCarePrescriptionUuid() const
{
    return uuidFromArray(inputArrayPointer, codeInputHealthCareProviderPrescription);
}

std::optional<std::string_view> Task::patientConfirmationUuid() const
{
    return uuidFromArray(inputArrayPointer, codeInputPatientConfirmation);
}

std::optional<std::string_view> Task::receiptUuid() const
{
    return uuidFromArray(outputArrayPointer, codeOutputReceipt);
}

std::optional<std::string_view> Task::owner() const
{
    return getOptionalStringValue(ownerIdentifierValuePointer);
}

const Timestamp& Task::lastStatusChangeDate() const
{
    return mLastStatusChange;
}

std::optional<std::string_view> Task::uuidFromArray(const rapidjson::Pointer& array,
                                                    std::string_view code) const
{
    return findStringInArray(array, codePointerRelToInOutArray, code, valueStringPointerRelToInOutArray);
}

Timestamp Task::expiryDate() const
{
    return dateFromExtensionArray(resource::structure_definition::expiryDate);
}

Timestamp Task::acceptDate() const
{
    return dateFromExtensionArray(resource::structure_definition::acceptDate);
}

std::optional<Timestamp> Task::lastMedicationDispense() const
{
    const std::string_view url(resource::structure_definition::lastMedicationDispense);
    const rapidjson::Pointer urlPointer("/url");
    const rapidjson::Pointer valueInstantPointer("/valueInstant");
    const auto arrayEntry = findStringInArray(
        extensionArrayPointer, urlPointer, url, valueInstantPointer);
    // ModelExpect(arrayEntry.has_value(), std::string(url) + " is missing from extension array");
    if (arrayEntry.has_value()) {
        return Timestamp::fromFhirDateTime(std::string(arrayEntry.value()));
    }
    return {};
}

Timestamp Task::dateFromExtensionArray(std::string_view url) const
{
    const rapidjson::Pointer urlPointer("/url");
    const rapidjson::Pointer valueDatePointer("/valueDate");
    const auto arrayEntry = findStringInArray(
        extensionArrayPointer, urlPointer, url, valueDatePointer);
    ModelExpect(arrayEntry.has_value(), std::string(url) + " is missing from extension array");
    return Timestamp::fromGermanDate(std::string(arrayEntry.value()));
}

void Task::updateLastUpdate(const Timestamp& timestamp)
{
    setValue(lastModifiedPointer, timestamp.toXsDateTime());
}

void Task::setStatus(const Task::Status newStatus)
{
    setStatus(newStatus, Timestamp::now());
}

void Task::setStatus(Status newStatus, model::Timestamp lastStatusChange)
{
    setValue(statusPointer, StatusNames.at(newStatus));
    mLastStatusChange = lastStatusChange;
}

void Task::setKvnr(const Kvnr& kvnr)
{
    ModelExpect(!hasValue(kvnrPointer), "KVNR cannot be set multiple times.");
    ModelExpect(kvnr.getType() != model::Kvnr::Type::unspecified, "Unspecified kvnr type not allowed");
    setValue(kvnrPointer, kvnr.id());
    const auto& profileName = getProfileName();
    if (profileName && fhirtools::DefinitionKey{*profileName}.version >= model::version::GEM_ERP_1_4)
    {
        // GEM_ERP_PR_Task V1.4 allows only GKV
        // https://simplifier.net/packages/de.gematik.erezept-workflow.r4/1.4.3/files/2550145
        setValue(kvnrSysPointer, model::resource::naming_system::gkvKvid10);
        return;
    }
    setValue(kvnrSysPointer, kvnr.namingSystem());
}

void Task::setHealthCarePrescriptionUuid()
{
    addUuidToArray(inputArrayPointer, codeInputHealthCareProviderPrescription,
                   prescriptionId().deriveUuid(uuidFeaturePrescription));
}

void Task::setPatientConfirmationUuid()
{
    addUuidToArray(inputArrayPointer, codeInputPatientConfirmation,
                   prescriptionId().deriveUuid(uuidFeatureConfirmation));
}

void Task::setReceiptUuid()
{
    addUuidToArray(outputArrayPointer, codeOutputReceipt,
                   prescriptionId().deriveUuid(uuidFeatureReceipt));
}

void Task::addUuidToArray(const rapidjson::Pointer& array, std::string_view code,
                       std::string_view uuid)
{
    // check that the array entry does not exist.
    ModelExpect(!uuidFromArray(array, code).has_value(), "UUID references can only be set once");

    auto newEntry = copyValue(*PrescriptionReferenceTemplate);
    setKeyValue(newEntry, codePointerRelToInOutArray, code);
    setKeyValue(newEntry, valueStringPointerRelToInOutArray, uuid);
    addToArray(array, std::move(newEntry));
}

void Task::setExpiryDate(const Timestamp& expiryDate)
{
    dateToExtensionArray(resource::structure_definition::expiryDate, expiryDate);
}

void Task::setAcceptDate(const Timestamp& acceptDate)
{
    dateToExtensionArray(resource::structure_definition::acceptDate, acceptDate);
}

void Task::setAcceptDate(const Timestamp& baseTime, const std::optional<KbvStatusKennzeichen>& legalBasisCode,
                         int entlassRezeptValidityWorkingDays)
{
    if (legalBasisCode)
    {
        switch (*legalBasisCode)
        {
            case KbvStatusKennzeichen::ohneErsatzverordnungskennzeichen:
            case KbvStatusKennzeichen::asvKennzeichen:
            case KbvStatusKennzeichen::tssKennzeichen:
            case KbvStatusKennzeichen::nurErsatzverordnungsKennzeichen:
            case KbvStatusKennzeichen::asvKennzeichenMitErsatzverordnungskennzeichen:
            case KbvStatusKennzeichen::tssKennzeichenMitErsatzverordungskennzeichen:
                break;
            case KbvStatusKennzeichen::entlassmanagementKennzeichen:
            case KbvStatusKennzeichen::entlassmanagementKennzeichenMitErsatzverordungskennzeichen:
                A_19517_02.start("deviant accept date for Entlassrezepte");
                // -1 because the current day is part of the duration.
                setAcceptDate((WorkDay(baseTime) + gsl::narrow<unsigned int>(entlassRezeptValidityWorkingDays - 1))
                                  .toTimestamp());
                A_19517_02.finish();
                return;
        }
    }
    A_19445_10.start(
        "Task.AcceptDate = <Date of QES Creationv + (28 days for 160 and 169, 3 months for 162, 200 and 209)>");
    setAcceptDateDependentPrescriptionType(baseTime);
    A_19445_10.finish();
}

void Task::setAcceptDateDependentPrescriptionType(const Timestamp& baseTime)
{
    using namespace std::chrono_literals;
    switch (type())
    {
        case model::PrescriptionType::apothekenpflichtigeArzneimittelPkv:
        case model::PrescriptionType::direkteZuweisungPkv:
        case PrescriptionType::digitaleGesundheitsanwendungen:
        {
            auto acceptDay = date::year_month_day{date::floor<date::days>(
                                 date::make_zoned(model::Timestamp::GermanTimezone, baseTime.toChronoTimePoint())
                                     .get_local_time())} +
                             date::months{3};
            if (! acceptDay.ok())
            {
                acceptDay = acceptDay.year() / acceptDay.month() / date::last;
            }
            setAcceptDate(model::Timestamp{date::sys_days{acceptDay}});
            break;
        }

        case model::PrescriptionType::apothekenpflichigeArzneimittel:
        case model::PrescriptionType::direkteZuweisung:
            setAcceptDate(baseTime + (24h * 28));
            break;
    }
}

void Task::updateLastMedicationDispense(const std::optional<model::Timestamp>& lastMedicationDispense /* = model::Timestamp::now() */)
{
    instantToExtensionArray(resource::structure_definition::lastMedicationDispense, lastMedicationDispense);
}

void Task::dateToExtensionArray(std::string_view url, const Timestamp& date)
{
    const rapidjson::Pointer urlPointer("/url");
    const rapidjson::Pointer valueDatePointer("/valueDate");
    const auto found = findStringInArray(extensionArrayPointer, urlPointer, url, valueDatePointer);
    ModelExpect(!found.has_value(), std::string(url) + " can only be set once");

    auto newValue = copyValue(*ExtensionDateTemplate);
    setKeyValue(newValue, urlPointer, url);
    setKeyValue(newValue, valueDatePointer, date.toGermanDate());
    addToArray(extensionArrayPointer, std::move(newValue));
}

void Task::instantToExtensionArray(std::string_view url, const std::optional<Timestamp>& date)
{
    const rapidjson::Pointer urlPointer("/url");
    const rapidjson::Pointer valueInstantPointer("/valueInstant");

    const auto instantAndPos = findMemberInArray(extensionArrayPointer, urlPointer, url, valueInstantPointer, true);
    if(instantAndPos)
    {
        removeFromArray(extensionArrayPointer, std::get<1>(instantAndPos.value()));
    }

    if(date)
    {
        auto newValue = copyValue(*ExtensionInstantTemplate);
        setKeyValue(newValue, urlPointer, url);
        setKeyValue(newValue, valueInstantPointer, date.value().toXsDateTimeWithoutFractionalSeconds());
        addToArray(extensionArrayPointer, std::move(newValue));
    }
}

void Task::setSecret(std::string_view secret)
{
    ModelExpect(
        ! findStringInArray(identifierArrayPointer, systemRelPointer, resource::naming_system::secret, valueRelPointer)
              .has_value(),
        "Secret cannot be set multiple times.");

    auto newValue = copyValue(*SecretAccessCodeTemplate);
    setKeyValue(newValue, valueRelPointer, secret);
    setKeyValue(newValue, systemRelPointer, resource::naming_system::secret);
    addToArray(identifierArrayPointer, std::move(newValue));
}

void Task::setAccessCode(std::string_view accessCode)
{
    ModelExpect(! findStringInArray(identifierArrayPointer, systemRelPointer, resource::naming_system::accessCode,
                                    valueRelPointer)
                      .has_value(),
                "AccessCode cannot be set multiple times.");

    auto newValue = copyValue(*SecretAccessCodeTemplate);
    setKeyValue(newValue, valueRelPointer, accessCode);
    setKeyValue(newValue, systemRelPointer, resource::naming_system::accessCode);
    addToArray(identifierArrayPointer, std::move(newValue));
}

void Task::setOwner(std::string_view owner)
{
    setValue(ownerIdentifierSystemPointer, resource::naming_system::telematicID);
    setValue(ownerIdentifierValuePointer, owner);
}

void Task::deleteAccessCode()
{
    const auto accessCodeAndPos = findMemberInArray(identifierArrayPointer, systemRelPointer,
                                                    resource::naming_system::accessCode, valueRelPointer, true);
    if(accessCodeAndPos)
    {
        removeFromArray(identifierArrayPointer, std::get<1>(accessCodeAndPos.value()));
    }
}

void Task::deleteSecret()
{
    const auto secretAndPos = findMemberInArray(identifierArrayPointer, systemRelPointer,
                                                resource::naming_system::secret, valueRelPointer, true);
    if(secretAndPos)
    {
        removeFromArray(identifierArrayPointer, std::get<1>(secretAndPos.value()));
    }
}

void Task::deleteOwner()
{
    removeElement(ownerIdentifierSystemPointer);
    removeElement(ownerIdentifierValuePointer);
}

void Task::deleteLastMedicationDispense()
{
    const rapidjson::Pointer urlPointer("/url");
    const rapidjson::Pointer valueInstantPointer("/valueInstant");

    const auto lastMDAndPos = findMemberInArray(extensionArrayPointer, urlPointer,
                            resource::structure_definition::lastMedicationDispense, valueInstantPointer, true);
    if(lastMDAndPos)
    {
        removeFromArray(extensionArrayPointer, std::get<1>(lastMDAndPos.value()));
    }
}

Task::Task(NumberAsStringParserDocument&& jsonTree)
    : Resource<Task>(std::move(jsonTree))
    , mLastStatusChange(Timestamp{0.})
{
    std::call_once(onceFlag, initTemplates);
}


std::optional<model::Timestamp> Task::getValidationReferenceTimestamp() const
{
    return Timestamp::now();
}

void Task::removeLastMedicationDispenseConditional()
{
    using namespace fhirtools::version_literal;
    const auto fhirVersion = fhirtools::DefinitionKey(getProfileName().value()).version;
    if(fhirVersion < "1.3"_ver)
    {
        updateLastMedicationDispense({});
    }
}

} // namespace model
