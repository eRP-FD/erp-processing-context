/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#ifndef TEST_UTIL_RESOURCETEMPLATES_HXX
#define TEST_UTIL_RESOURCETEMPLATES_HXX

#include "erp/model/Bundle.hxx"
#include "erp/model/ResourceVersion.hxx"
#include "erp/model/Timestamp.hxx"

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
    model::ResourceVersion::KbvItaErp kbvVersion = model::ResourceVersion::current<model::ResourceVersion::KbvItaErp>();
    std::string_view medicationCategory = "00";
    std::string_view coverageInsuranceSystem = "http://fhir.de/CodeSystem/versicherungsart-de-basis";
    std::optional<std::string_view> coverageInsuranceType = std::nullopt;
    std::string_view coveragePayorExtension = "";
    std::string_view metaExtension = "";
    std::optional<std::string_view> forceInsuranceType = std::nullopt;
    std::optional<std::string_view> forceAuthoredOn = std::nullopt;
};

std::string kbvBundleXml(const KbvBundleOptions& bundleOptions = {});

struct KbvBundleMvoOptions
{
    model::PrescriptionId prescriptionId = model::PrescriptionId::fromString("160.000.000.004.713.80");
    model::Timestamp timestamp = model::Timestamp::fromFhirDateTime("2020-06-23T08:30:00+00:00");
    std::string_view legalBasisCode = "00";
    int numerator = 4;
    int denominator = 4;
    model::ResourceVersion::KbvItaErp kbvVersion = model::ResourceVersion::current<model::ResourceVersion::KbvItaErp>();
    //std::string_view multiplePrescriptionExtension = "";
    std::optional<model::Timestamp> redeemPeriodStart = model::Timestamp::fromXsDate("2021-01-02");
    std::optional<model::Timestamp> redeemPeriodEnd = model::Timestamp::fromXsDate("2021-01-02");

};
std::string kbvBundleMvoXml(const KbvBundleMvoOptions& bundleOptions = KbvBundleMvoOptions{});

struct MedicationDispenseBundleOptions
{
    model::PrescriptionId prescriptionId = model::PrescriptionId::fromString("160.000.033.491.280.78");
    std::string_view kvnr = "X234567890";
    std::string_view telematikId = "3-SMC-B-Testkarte-883110000120312";
    model::Timestamp whenHandedOver = model::Timestamp::fromFhirDateTime("2021-09-21");
    std::optional<model::Timestamp> whenPrepared = std::nullopt;
    model::ResourceVersion::DeGematikErezeptWorkflowR4 gematikVersion =
        model::ResourceVersion::current<model::ResourceVersion::DeGematikErezeptWorkflowR4>();
};
std::string medicationDispenseBundleXml(const MedicationDispenseBundleOptions& bundleOptions = {});

using MedicationDispenseOptions = MedicationDispenseBundleOptions;
std::string medicationDispenseXml(const MedicationDispenseOptions& medicationDispenseOptions = {});

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
