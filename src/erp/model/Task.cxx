/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/Task.hxx"
#include "erp/ErpConstants.hxx"
#include "erp/ErpRequirements.hxx"
#include "erp/model/Composition.hxx"
#include "erp/model/ResourceNames.hxx"
#include "erp/model/Timestamp.hxx"
#include "erp/util/Expect.hxx"
#include "erp/model/Kvnr.hxx"
#include "erp/util/RapidjsonDocument.hxx"
#include "erp/util/WorkDay.hxx"
#include "fhirtools/util/Gsl.hxx"

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

constexpr std::string_view prescriptionReference_template = R"--(
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

constexpr std::string_view secret_template = R"--(
{
  "use": "official",
  "system": "",
  "value": ""
}
)--";

constexpr std::string_view extension_date_template = R"--(
{
  "url": "",
  "valueDate": ""
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

void initTemplates()
{
    rapidjson::StringStream s1(task_template.data());
    TaskTemplate->ParseStream<rapidjson::kParseNumbersAsStringsFlag, rapidjson::CustomUtf8>(s1);

    rapidjson::StringStream s2(prescriptionReference_template.data());
    PrescriptionReferenceTemplate->ParseStream<rapidjson::kParseNumbersAsStringsFlag, rapidjson::CustomUtf8>(s2);

    rapidjson::StringStream s3(secret_template.data());
    SecretAccessCodeTemplate->ParseStream<rapidjson::kParseNumbersAsStringsFlag, rapidjson::CustomUtf8>(s3);

    rapidjson::StringStream s4(extension_date_template.data());
    ExtensionDateTemplate->ParseStream<rapidjson::kParseNumbersAsStringsFlag, rapidjson::CustomUtf8>(s4);
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
           model::ResourceVersion::DeGematikErezeptWorkflowR4 profileVersion)
    : Resource<Task>(ResourceVersion::deprecatedProfile(profileVersion)
                         ? resource::structure_definition::deprecated::task
                         : resource::structure_definition::task,
                     []() {
                         std::call_once(onceFlag, initTemplates);

                         A_19114.start("create Task in status draft");
                         return TaskTemplate;
                         A_19114.finish();
                     }()
                         .instance())
{
    if (accessCode.has_value())
    {
        setAccessCode(*accessCode);
    }
    if (ResourceVersion::deprecatedProfile(profileVersion))
    {
        setValue(flowtypeUrlPointer, resource::structure_definition::deprecated::prescriptionType);
        setValue(flowtypeSystemPointer, resource::code_system::deprecated::flowType);
        setValue(prescriptionIdSystemPointer, resource::naming_system::deprecated::prescriptionID);
        setValue(performerTypeSystemPointer, "urn:ietf:rfc:3986");
    }
    A_19112.start("set Task.extension:flowType to the value of prescriptionType");
    setValue(flowtypeCodePointer,
             std::to_string(
                 static_cast<std::underlying_type_t<model::PrescriptionType>>(prescriptionType)));
    setValue(flowtypeDisplayPointer, PrescriptionTypeDisplay.at(prescriptionType));
    A_19112.finish();

    A_19214.start("set Task.performerType corresponding to the value of prescriptionType");
    A_19445_08.start("set Task.performerType corresponding to the value of prescriptionType");
    setValue(performerTypePointer, PrescriptionTypePerformerType.at(prescriptionType));
    setValue(performerTypeDisplayPointer, PrescriptionTypePerformerDisplay.at(prescriptionType));
    setValue(performerTypeTextPointer, PrescriptionTypePerformerDisplay.at(prescriptionType));
    A_19445_08.finish();
    A_19214.finish();

    setValue(authoredOnPointer, Timestamp::now().toXsDateTime());

    updateLastUpdate();
}

Task::Task(const PrescriptionId& id, PrescriptionType prescriptionType, Timestamp lastModified, Timestamp authoredOn,
           Task::Status status, model::ResourceVersion::DeGematikErezeptWorkflowR4 profileVersion)
    : Task(prescriptionType, {}, profileVersion)
{
    setPrescriptionId(id);
    updateLastUpdate(lastModified);
    setValue(authoredOnPointer, authoredOn.toXsDateTime());
    setStatus(status);
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
    auto nsAccessCode = ResourceVersion::deprecatedProfile(value(getSchemaVersion()))
                            ? resource::naming_system::deprecated::accessCode
                            : resource::naming_system::accessCode;
    auto accessCode = findStringInArray(identifierArrayPointer, systemRelPointer, nsAccessCode, valueRelPointer);
    ModelExpect(accessCode.has_value(), "AccessCode not set in task");
    return *accessCode;
}

std::optional<std::string_view> Task::secret() const
{
    auto nsSecret = ResourceVersion::deprecatedProfile(value(getSchemaVersion())) ? resource::naming_system::deprecated::secret
                                                                           : resource::naming_system::secret;
    return findStringInArray(identifierArrayPointer, systemRelPointer, nsSecret, valueRelPointer);
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

std::optional<std::string_view> Task::uuidFromArray(const rapidjson::Pointer& array,
                                                    std::string_view code) const
{
    return findStringInArray(array, codePointerRelToInOutArray, code, valueStringPointerRelToInOutArray);
}

Timestamp Task::expiryDate() const
{
    auto url = ResourceVersion::deprecatedProfile(value(getSchemaVersion()))
                   ? resource::structure_definition::deprecated::expiryDate
                   : resource::structure_definition::expiryDate;
    return dateFromExtensionArray(url);
}

Timestamp Task::acceptDate() const
{
    auto url = ResourceVersion::deprecatedProfile(value(getSchemaVersion()))
                   ? resource::structure_definition::deprecated::acceptDate
                   : resource::structure_definition::acceptDate;
    return dateFromExtensionArray(url);
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
    setValue(statusPointer, StatusNames.at(newStatus).data());
}

void Task::setKvnr(const Kvnr& kvnr)
{
    ModelExpect(!hasValue(kvnrPointer), "KVNR cannot be set multiple times.");
    ModelExpect(kvnr.getType() != model::Kvnr::Type::unspecified, "Unspecified kvnr type not allowed");
    const bool deprecatedProfile = ResourceVersion::deprecatedProfile(value(getSchemaVersion()));
    setValue(kvnrPointer, kvnr.id());
    setValue(kvnrSysPointer, kvnr.namingSystem(deprecatedProfile));
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
    if (ResourceVersion::deprecatedProfile(value(getSchemaVersion())))
    {
        setKeyValue(newEntry, systemPointerRelToInOutArray, resource::code_system::deprecated::documentType);
    }
    addToArray(array, std::move(newEntry));
}

void Task::setExpiryDate(const Timestamp& expiryDate)
{
    auto url = ResourceVersion::deprecatedProfile(value(getSchemaVersion()))
                   ? resource::structure_definition::deprecated::expiryDate
                   : resource::structure_definition::expiryDate;
    dateToExtensionArray(url, expiryDate);
}

void Task::setAcceptDate(const Timestamp& acceptDate)
{
    auto url = ResourceVersion::deprecatedProfile(value(getSchemaVersion()))
                   ? resource::structure_definition::deprecated::acceptDate
                   : resource::structure_definition::acceptDate;
    dateToExtensionArray(url, acceptDate);
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
    A_19445_08.start(
        "Task.AcceptDate = <Date of QES Creationv + (28 days for 160 and 169, 3 months for 200)>");
    setAcceptDateDependentPrescriptionType(baseTime);
    A_19445_08.finish();
}

void Task::setAcceptDateDependentPrescriptionType(const Timestamp& baseTime)
{
    using namespace std::chrono_literals;
    switch (type())
    {
        case model::PrescriptionType::apothekenpflichtigeArzneimittelPkv:
        case model::PrescriptionType::direkteZuweisungPkv:
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

void Task::setSecret(std::string_view secret)
{
    auto nsSecret = ResourceVersion::deprecatedProfile(value(getSchemaVersion())) ? resource::naming_system::deprecated::secret
                                                                           : resource::naming_system::secret;
    ModelExpect(! findStringInArray(identifierArrayPointer, systemRelPointer, nsSecret, valueRelPointer).has_value(),
                "Secret cannot be set multiple times.");

    auto newValue = copyValue(*SecretAccessCodeTemplate);
    setKeyValue(newValue, valueRelPointer, secret);
    setKeyValue(newValue, systemRelPointer, nsSecret);
    addToArray(identifierArrayPointer, std::move(newValue));
}

void Task::setAccessCode(std::string_view accessCode)
{
    auto nsAccessCode = ResourceVersion::deprecatedProfile(value(getSchemaVersion()))
                            ? resource::naming_system::deprecated::accessCode
                            : resource::naming_system::accessCode;
    ModelExpect(! findStringInArray(identifierArrayPointer, systemRelPointer,
                                    nsAccessCode, valueRelPointer)
                      .has_value(),
                "AccessCode cannot be set multiple times.");

    auto newValue = copyValue(*SecretAccessCodeTemplate);
    setKeyValue(newValue, valueRelPointer, accessCode);
    setKeyValue(newValue, systemRelPointer, nsAccessCode);
    addToArray(identifierArrayPointer, std::move(newValue));
}

void Task::deleteAccessCode()
{
    auto nsAccessCode = ResourceVersion::deprecatedProfile(value(getSchemaVersion()))
                            ? resource::naming_system::deprecated::accessCode
                            : resource::naming_system::accessCode;
    const auto accessCodeAndPos =
        findMemberInArray(identifierArrayPointer, systemRelPointer, nsAccessCode, valueRelPointer, true);
    if(accessCodeAndPos)
    {
        removeFromArray(identifierArrayPointer, std::get<1>(accessCodeAndPos.value()));
    }
}

void Task::deleteSecret()
{
    auto nsSecret = ResourceVersion::deprecatedProfile(value(getSchemaVersion()))
                            ? resource::naming_system::deprecated::secret
                            : resource::naming_system::secret;
    const auto secretAndPos =
        findMemberInArray(identifierArrayPointer, systemRelPointer, nsSecret, valueRelPointer, true);
    if(secretAndPos)
    {
        removeFromArray(identifierArrayPointer, std::get<1>(secretAndPos.value()));
    }
}

Task::Task(NumberAsStringParserDocument&& jsonTree)
    : Resource<Task>(std::move(jsonTree))
{
    std::call_once(onceFlag, initTemplates);
}

} // namespace model
