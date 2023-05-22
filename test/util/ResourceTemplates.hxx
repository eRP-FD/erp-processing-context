/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef TEST_UTIL_RESOURCETEMPLATES_HXX
#define TEST_UTIL_RESOURCETEMPLATES_HXX

#include "erp/model/Bundle.hxx"
#include "erp/model/ResourceVersion.hxx"
#include "erp/model/Timestamp.hxx"
#include "erp/model/Kvnr.hxx"

#include <string>
#include <string_view>
#include <optional>

namespace ResourceTemplates
{

struct KbvBundleOptions
{
    model::PrescriptionId prescriptionId = model::PrescriptionId::fromString("160.000.000.004.713.80");
    model::Timestamp timestamp = model::Timestamp::fromFhirDateTime("2021-06-08T13:44:53.012475+02:00");
    std::string_view kvnr = "X234567890";
    std::optional<model::ResourceVersion::KbvItaErp> kbvVersion = std::nullopt;
    std::string_view medicationCategory = "00";
    std::string_view coverageInsuranceSystem = "http://fhir.de/CodeSystem/versicherungsart-de-basis";
    std::optional<std::string_view> coverageInsuranceType = std::nullopt;
    std::string_view coveragePayorExtension = "";
    std::string_view metaExtension = "";
    std::optional<std::string_view> forceInsuranceType = std::nullopt;
    std::optional<std::string_view> forceAuthoredOn = std::nullopt;
    std::optional<std::string_view> forceKvid10Ns = std::nullopt;
};

std::string kbvBundleXml(const KbvBundleOptions& bundleOptions = {});

struct KbvBundleMvoOptions
{
    model::PrescriptionId prescriptionId = model::PrescriptionId::fromString("160.000.000.004.713.80");
    model::Timestamp timestamp = model::Timestamp::fromFhirDateTime("2020-06-23T08:30:00+00:00");
    std::string_view legalBasisCode = "00";
    int numerator = 4;
    int denominator = 4;
    std::optional<model::ResourceVersion::KbvItaErp> kbvVersion = std::nullopt;
    //std::string_view multiplePrescriptionExtension = "";
    std::optional<std::string> redeemPeriodStart = "2021-01-02";
    std::optional<std::string> redeemPeriodEnd = "2021-01-02";

};
std::string kbvBundleMvoXml(const KbvBundleMvoOptions& bundleOptions = KbvBundleMvoOptions{});

struct KbvBundlePkvOptions
{
    KbvBundlePkvOptions(const model::PrescriptionId& prescriptionId, const model::Kvnr& kvnr,
                        model::ResourceVersion::KbvItaErp kbvVersion = model::ResourceVersion::KbvItaErp::v1_1_0);
    model::PrescriptionId prescriptionId;
    model::Kvnr kvnr;
    model::ResourceVersion::KbvItaErp kbvVersion;
};
std::string kbvBundlePkvXml(const KbvBundlePkvOptions& bundleOptions);

struct MedicationOptions {
    std::optional<model::ResourceVersion::KbvItaErp> version{};
    std::string templatePrefix = "test/EndpointHandlerTest/medication_template_";
    std::string id = "001413e4-a5e9-48da-9b07-c17bab476407";
};

struct MedicationDispenseOptions {
    model::PrescriptionId prescriptionId = model::PrescriptionId::fromString("160.000.033.491.280.78");
    std::string_view kvnr = "X234567890";
    std::string_view telematikId = "3-SMC-B-Testkarte-883110000120312";
    model::Timestamp whenHandedOver = model::Timestamp::fromFhirDateTime("2021-09-21");
    std::optional<model::Timestamp> whenPrepared = std::nullopt;
    model::ResourceVersion::DeGematikErezeptWorkflowR4 gematikVersion =
        model::ResourceVersion::current<model::ResourceVersion::DeGematikErezeptWorkflowR4>();
    MedicationOptions medication{};
};

struct MedicationDispenseBundleOptions {
    model::ResourceVersion::FhirProfileBundleVersion bundleFhirBundleVersion = model::ResourceVersion::currentBundle();
    std::list<MedicationDispenseOptions> medicationDispenses = {{}, {}};// two defaulted medication dispenses
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
    std::string_view kvnr = "X234567890";
};


std::string taskJson(const TaskOptions& taskOptions = {});

} // namespace ResourceTemplates

#endif // TEST_UTIL_RESOURCETEMPLATES_HXX
