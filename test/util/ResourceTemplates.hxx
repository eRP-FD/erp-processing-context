/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef TEST_UTIL_RESOURCETEMPLATES_HXX
#define TEST_UTIL_RESOURCETEMPLATES_HXX

#include "erp/model/Bundle.hxx"
#include "erp/model/Iknr.hxx"
#include "erp/model/Kvnr.hxx"
#include "erp/model/Lanr.hxx"
#include "erp/model/Pzn.hxx"
#include "erp/model/ResourceVersion.hxx"
#include "erp/model/Timestamp.hxx"

#include <string>
#include <string_view>
#include <optional>
#include <variant>
#include <chrono>

using namespace std::chrono_literals;

namespace ResourceTemplates
{

struct KbvBundleOptions
{
    model::PrescriptionId prescriptionId = model::PrescriptionId::fromString("160.000.000.004.713.80");
    model::Timestamp authoredOn = model::Timestamp::now();
    std::string_view kvnr = "X234567891";
    std::optional<model::ResourceVersion::KbvItaErp> kbvVersion = std::nullopt;
    std::string_view medicationCategory = "00";
    std::string_view coverageInsuranceSystem = "http://fhir.de/CodeSystem/versicherungsart-de-basis";
    std::optional<std::string_view> coverageInsuranceType = std::nullopt;
    std::string_view coveragePayorExtension = "";
    std::string_view metaExtension = "";
    model::Iknr iknr = model::Iknr{"109500969"};
    model::Lanr lanr = model::Lanr{"444444400", model::Lanr::Type::lanr};
    model::Pzn pzn = model::Pzn{"04773414"};
    std::optional<std::string_view> forceInsuranceType = std::nullopt;
    std::optional<std::string_view> forceAuthoredOn = std::nullopt;
    std::optional<std::string_view> forceKvid10Ns = std::nullopt;
    std::string_view patientIdentifierSystem = "http://fhir.de/CodeSystem/identifier-type-de-basis";
    std::string darreichungsfrom = "TAB";
};

std::string kbvBundleXml(const KbvBundleOptions& bundleOptions = {});

struct KbvBundleMvoOptions
{
    model::PrescriptionId prescriptionId = model::PrescriptionId::fromString("160.000.000.004.713.80");
    model::Timestamp authoredOn = model::Timestamp::now();
    std::string_view legalBasisCode = "00";
    int numerator = 4;
    int denominator = 4;
    std::optional<model::ResourceVersion::KbvItaErp> kbvVersion = std::nullopt;
    std::optional<std::string> redeemPeriodStart = "2021-01-02";
    std::optional<std::string> redeemPeriodEnd = "2021-01-02";
    std::string mvoId = "urn:uuid:24e2e10d-e962-4d1c-be4f-8760e690a5f0";
};
std::string kbvBundleMvoXml(const KbvBundleMvoOptions& bundleOptions = KbvBundleMvoOptions{});

struct KbvBundlePkvOptions
{
    model::PrescriptionId prescriptionId = model::PrescriptionId::fromString("200.000.000.004.711.03");
    model::Kvnr kvnr;
    model::ResourceVersion::KbvItaErp kbvVersion = model::ResourceVersion::KbvItaErp::v1_1_0;
    model::Timestamp authoredOn = model::Timestamp::now();
};
std::string kbvBundlePkvXml(const KbvBundlePkvOptions& bundleOptions);

struct MedicationOptions {
    std::optional<model::ResourceVersion::KbvItaErp> version{};
    std::string templatePrefix = "test/EndpointHandlerTest/medication_template_";
    std::string id = "001413e4-a5e9-48da-9b07-c17bab476407";
};

struct MedicationDispenseOptions {
    std::variant<model::PrescriptionId, std::string> prescriptionId = model::PrescriptionId::fromString("160.000.033.491.280.78");
    std::string_view kvnr = "X234567891";
    std::string_view telematikId = "3-SMC-B-Testkarte-883110000120312";
    std::variant<model::Timestamp, std::string> whenHandedOver = model::Timestamp::now();
    std::optional<model::Timestamp> whenPrepared = std::nullopt;
    model::ResourceVersion::DeGematikErezeptWorkflowR4 gematikVersion =
        model::ResourceVersion::current<model::ResourceVersion::DeGematikErezeptWorkflowR4>();
    MedicationOptions medication{};
};

struct MedicationDispenseBundleOptions {
    model::ResourceVersion::FhirProfileBundleVersion bundleFhirBundleVersion = model::ResourceVersion::currentBundle();
    std::list<MedicationDispenseOptions> medicationDispenses = {{}, {}};// two defaulted medication dispenses
    model::PrescriptionId prescriptionId = model::PrescriptionId::fromString("160.123.456.789.123.58");
};

std::string medicationDispenseBundleXml(const MedicationDispenseBundleOptions& bundleOptions = {});

std::string medicationDispenseXml(const MedicationDispenseOptions& medicationDispenseOptions = {});

std::string medicationXml(const MedicationOptions& medicationOptions = {});

enum class TaskType
{
    Draft,
    Ready,
    InProgress,
    Completed
};
struct TaskOptions
{
    model::ResourceVersion::DeGematikErezeptWorkflowR4 gematikVersion =
        model::ResourceVersion::current<model::ResourceVersion::DeGematikErezeptWorkflowR4>();
    TaskType taskType;
    model::PrescriptionId prescriptionId = model::PrescriptionId::fromString("160.000.000.004.713.80");
    model::Timestamp timestamp = model::Timestamp::fromFhirDateTime("2021-06-08T13:44:53.012475+02:00");
    model::Timestamp expirydate = model::Timestamp::now() + 48h;
    std::string_view kvnr = "X234567891";
    std::string_view owningPharmacy = "3-SMC-B-Testkarte-883110000120312";
};


std::string taskJson(const TaskOptions& taskOptions = {});

struct ChargeItemOptions
{
    model::ResourceVersion::DeGematikErezeptPatientenrechnungR4 version =
        model::ResourceVersion::current<model::ResourceVersion::DeGematikErezeptPatientenrechnungR4>();
    model::Kvnr kvnr;
    model::PrescriptionId prescriptionId;
    std::string dispenseBundleBase64;
    enum class OperationType
    {
        Post,
        Put,
    } operation;
};

std::string chargeItemXml(const ChargeItemOptions& chargeItemOptions);

} // namespace ResourceTemplates

#endif // TEST_UTIL_RESOURCETEMPLATES_HXX
