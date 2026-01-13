/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef TEST_UTIL_RESOURCETEMPLATES_HXX
#define TEST_UTIL_RESOURCETEMPLATES_HXX

#include "erp/model/Consent.hxx"
#include "erp/model/Lanr.hxx"
#include "erp/model/eu/GemErpEuPrParGetPrescriptionInput.hxx"
#include "shared/model/Bundle.hxx"
#include "shared/model/Iknr.hxx"
#include "shared/model/Kvnr.hxx"
#include "shared/model/MedicationDispenseBundle.hxx"
#include "shared/model/MedicationDispenseId.hxx"
#include "shared/model/Pzn.hxx"
#include "shared/model/Timestamp.hxx"

#include <optional>
#include <string>
#include <string_view>
#include <variant>

using namespace std::chrono_literals;

namespace ResourceTemplates
{

struct Versions {
    struct KBV_ERP : fhirtools::FhirVersion {
        using FhirVersion::FhirVersion;
        explicit KBV_ERP(FhirVersion ver);
        std::string renderVersion()  const;
    };
    struct GEM_ERP : fhirtools::FhirVersion {
        using FhirVersion::FhirVersion;
        explicit GEM_ERP(FhirVersion ver);
        std::string renderVersion() const;
    };
    struct GEM_ERPCHRG : fhirtools::FhirVersion {
        using FhirVersion::FhirVersion;
        explicit GEM_ERPCHRG(FhirVersion ver);
        std::string renderVersion() const;
    };
    struct DAV_PKV : fhirtools::FhirVersion {
        using FhirVersion::FhirVersion;
        explicit DAV_PKV(FhirVersion ver);
        std::string renderVersion() const;
    };
    struct KBV_EVDGA : fhirtools::FhirVersion {
        using FhirVersion::FhirVersion;
        explicit KBV_EVDGA(FhirVersion ver);
        std::string renderVersion() const;
    };
    struct GEM_ERPEU : fhirtools::FhirVersion {
        using FhirVersion::FhirVersion;
        std::string renderVersion() const;
    };


    static inline GEM_ERP GEM_ERP_1_2{"1.2"};
    static inline GEM_ERP GEM_ERP_1_3{"1.3"};
    static inline GEM_ERP GEM_ERP_1_4{"1.4"};
    static inline GEM_ERP GEM_ERP_1_5_2{"1.5.2"};
    static inline KBV_ERP KBV_ERP_1_1_0{"1.1.0"};
    static inline KBV_ERP KBV_ERP_1_3_3{"1.3.3"};
    static inline KBV_EVDGA KBV_EVDGA_1_1{"1.1"};
    static inline KBV_EVDGA KBV_EVDGA_1_2_2{"1.2.2"};
    static inline GEM_ERPCHRG GEM_ERPCHRG_1_0{"1.0"};
    static inline GEM_ERPCHRG GEM_ERPCHRG_1_1{"1.1"};
    static inline DAV_PKV DAV_PKV_1_2{"1.2"};
    static inline DAV_PKV DAV_PKV_1_3{"1.3"};
    static inline DAV_PKV DAV_PKV_1_4_0{"1.4.0"};
    static inline GEM_ERPEU GEM_ERPEU_1_0{"1.0"};

    static fhirtools::DefinitionKey latest(std::string_view profileUrl,
                                           const model::Timestamp& reference = model::Timestamp::now());
    static GEM_ERP GEM_ERP_current(const model::Timestamp& reference = model::Timestamp::now());
    static KBV_ERP KBV_ERP_current(const model::Timestamp& reference = model::Timestamp::now());
    static KBV_EVDGA KBV_EVDGA_current(const model::Timestamp& reference = model::Timestamp::now());
    static GEM_ERPCHRG GEM_ERPCHRG_current(const model::Timestamp& reference = model::Timestamp::now());
    static DAV_PKV DAV_PKV_current(const model::Timestamp& reference = model::Timestamp::now());
    static GEM_ERPEU GEM_ERPEU_current(const model::Timestamp& reference = model::Timestamp::now());

    static std::initializer_list<GEM_ERP> GEM_ERP_all;
    static std::initializer_list<KBV_ERP> KBV_ERP_all;
    static std::initializer_list<KBV_EVDGA> KBV_EVDGA_all;
    static std::initializer_list<GEM_ERPCHRG> GEM_ERPCHRG_all;
    static std::initializer_list<DAV_PKV> DAV_PKV_all;
    static std::initializer_list<GEM_ERPEU> GEM_ERPEU_all;
};


struct KbvBundleMvoOptions
{
    model::PrescriptionId prescriptionId = model::PrescriptionId::fromString("160.000.000.004.713.80");
    std::string kvnr = "X234567891";
    model::Timestamp authoredOn = model::Timestamp::now();
    std::string_view legalBasisCode = "00";
    int numerator = 4;
    int denominator = 4;
    std::optional<Versions::KBV_ERP> kbvVersion = std::nullopt;
    std::optional<std::string> redeemPeriodStart = authoredOn.toGermanDate();
    std::optional<std::string> redeemPeriodEnd = getRedeemPeriodEnd(authoredOn, redeemPeriodStart);
    std::string mvoId = "urn:uuid:24e2e10d-e962-4d1c-be4f-8760e690a5f0";

private:
    static std::string getRedeemPeriodEnd(model::Timestamp authoredOn, std::optional<std::string> redeemPeriodStart);
};
std::string kbvBundleMvoXml(const KbvBundleMvoOptions& bundleOptions = KbvBundleMvoOptions{});

struct KbvBundlePkvOptions
{
    model::PrescriptionId prescriptionId = model::PrescriptionId::fromString("200.000.000.004.711.03");
    model::Kvnr kvnr;
    Versions::KBV_ERP kbvVersion = Versions::KBV_ERP_current(model::Timestamp::now());
    model::Timestamp authoredOn = model::Timestamp::now();
};
std::string kbvBundlePkvXml(const KbvBundlePkvOptions& bundleOptions);

struct MedicationOptions {
    static constexpr auto* PZN{"test/EndpointHandlerTest/medication_template_"};
    static constexpr auto* COMPOUNDING{"test/EndpointHandlerTest/medication_compounding_template_"};
    static constexpr auto* INGREDIENT{"test/EndpointHandlerTest/medication_ingredient_template_"};
    static constexpr auto* FREETEXT{"test/EndpointHandlerTest/medication_freetext_template_"};
    std::variant<std::monostate, Versions::KBV_ERP, Versions::GEM_ERP> version = std::monostate{};
    std::string templatePrefix = PZN;
    std::optional<std::string> id = std::nullopt;
    std::string darreichungsform = "TAB";
    std::string_view medicationCategory = "00";
    model::Pzn pzn = model::Pzn{"04773414"};
};

struct MedicationDispenseOptions {
    std::variant<model::MedicationDispenseId, std::string> id{
        std::in_place_type<model::MedicationDispenseId>, model::PrescriptionId::fromString("160.000.033.491.280.78"),
        0};
    std::variant<model::PrescriptionId, std::string> prescriptionId = guessPrescriptionId(id);
    std::string_view kvnr = "X234567891";
    std::string_view telematikId = "3-SMC-B-Testkarte-883110000120312";
    std::variant<model::Timestamp, std::string> whenHandedOver = model::Timestamp::now();
    std::variant<std::monostate, model::Timestamp, std::string> whenPrepared = std::monostate{};
    Versions::GEM_ERP gematikVersion = Versions::GEM_ERP_current();
    using MedicationReference = std::string;
    std::variant<MedicationOptions, MedicationReference> medication{};
private:
    std::variant<model::PrescriptionId, std::string>
    guessPrescriptionId(std::variant<model::MedicationDispenseId, std::string>& medicationDispenseId);
};

struct MedicationDispenseBundleOptions {
    Versions::GEM_ERP gematikVersion = Versions::GEM_ERP_current();
    std::list<MedicationDispenseOptions> medicationDispenses = {{}, {}};// two defaulted medication dispenses
    model::PrescriptionId prescriptionId = model::PrescriptionId::fromString("160.123.456.789.123.58");
};

std::string medicationDispenseBundleXml(const MedicationDispenseBundleOptions& bundleOptions = {});
namespace internal_type
{
model::MedicationDispenseBundle medicationDispenseBundle(const MedicationDispenseBundleOptions& bundleOptions = {});
}

std::string medicationDispenseXml(const MedicationDispenseOptions& medicationDispenseOptions = {});

std::string medicationXml(const MedicationOptions& medicationOptions = {});

struct MedicationDispenseDiGAOptions {
    std::string whenHandedOver = "2025-02-06";
    Versions::GEM_ERP version = Versions::GEM_ERP_current(model::Timestamp::fromGermanDate(whenHandedOver));
    std::string prescriptionId = "162.000.033.491.280.78";
    std::string kvnr = "X234567891";
    std::string telematikId = "3-SMC-B-Testkarte-883110000120312";
    bool withRedeemCode = true;
};
std::string medicationDispenseDigaXml(const MedicationDispenseDiGAOptions& options);

struct KbvBundleOptions
{
    model::PrescriptionId prescriptionId = model::PrescriptionId::fromString("160.000.000.004.713.80");
    model::Timestamp authoredOn = model::Timestamp::now();
    std::string_view kvnr = "X234567891";
    Versions::KBV_ERP kbvVersion = Versions::KBV_ERP_current(authoredOn);
    std::string_view coverageInsuranceSystem = "http://fhir.de/CodeSystem/versicherungsart-de-basis";
    std::optional<std::string_view> coverageInsuranceType = std::nullopt;
    std::string_view coveragePayorExtension = "";
    std::string_view medicationRequestExtension = "";
    std::string_view metaExtension = "";
    std::string_view statusCoPayment = "0";
    model::Iknr iknr = model::Iknr{"109500969"};
    model::Lanr lanr = model::Lanr{"444444400", model::Lanr::Type::lanr};
    std::optional<std::string_view> forceInsuranceType = std::nullopt;
    std::optional<std::string_view> forceAuthoredOn = std::nullopt;
    std::optional<std::string_view> forceKvid10Ns = std::nullopt;
    std::string_view patientIdentifierTypeSystem = "http://fhir.de/CodeSystem/identifier-type-de-basis";
    MedicationOptions medicationOptions{.version = kbvVersion};
};

std::string kbvBundleXml(const KbvBundleOptions& bundleOptions = {});

enum class TaskType
{
    Draft,
    Ready,
    InProgress,
    Completed
};
struct TaskOptions
{
    Versions::GEM_ERP gematikVersion = Versions::GEM_ERP_current();
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
    Versions::GEM_ERPCHRG version = Versions::GEM_ERPCHRG_current();
    model::Kvnr kvnr;
    model::PrescriptionId prescriptionId;
    std::string dispenseBundleBase64;
    enum class OperationType
    {
        Post,
        Put,
    } operation;
};

std::string chargeItemXml(const ChargeItemOptions& chargeItemOptions, std::string path = "");

struct PatchChargeItemOptions
{
    Versions::GEM_ERPCHRG version = Versions::GEM_ERPCHRG_current();
    std::optional<bool> insuranceProviderMarking = true;
    std::optional<bool> subsidyMarking = true;
    std::optional<bool> taxOfficeMarking = true;
};
std::string patchChargeItemJson(const PatchChargeItemOptions& patchChargeItemOptions);

// ensure we get a warning, when we leave somethin uninitialized when using designated init:
//NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init,hicpp-member-init)
struct DavDispenseItemOptions {
    Versions::DAV_PKV davPkvVersion = Versions::DAV_PKV_current();
    model::PrescriptionId prescriptionId;
    model::Timestamp whenHandenOver = model::Timestamp::now();
};

std::string davDispenseItemXml(const DavDispenseItemOptions& dispenseItemOptions);


struct MedicationDispenseOperationParametersOptions {
    model::ProfileType profileType = model::ProfileType::GEM_ERP_PR_PAR_CloseOperation_Input;
    std::string id = Uuid{};
    Versions::GEM_ERP version = Versions::GEM_ERP_current();
    std::list<MedicationDispenseOptions> medicationDispenses{1};
    std::list<MedicationDispenseDiGAOptions> medicationDispensesDiGA{};
};

std::string medicationDispenseOperationParametersXml(const MedicationDispenseOperationParametersOptions& options);

std::string dispenseOrCloseTaskBodyXml(const MedicationDispenseOperationParametersOptions& options);

struct EvdgaBundleOptions {
    Versions::KBV_EVDGA version = Versions::KBV_EVDGA_current();
    std::string prescriptionId = "162.100.000.000.032.60";
    std::string timestamp = "2023-03-26T13:12:00Z";
    std::string kvnr = "X234567890";
    std::string coverageSystem = "http://fhir.de/CodeSystem/versicherungsart-de-basis";
    std::string coverageCode = "GKV";
    std::string authoredOn = "2025-01-22";
};

std::string evdgaBundleXml(const EvdgaBundleOptions& options);

struct ConsentOptions
{
    using VersionType = std::variant<std::monostate, Versions::GEM_ERPCHRG, Versions::GEM_ERPEU>;
    model::ConsentType consentCategory;
    std::string kvnr;
    std::string timestamp = model::Timestamp::now().toXsDateTime();
    VersionType version{};
};
std::string consentJson(const ConsentOptions& options);

struct EuAccessPermissionOptions
{
    std::string twoLetterCountryCode = "FR";
    std::string accessCode = "abcdef";
};
std::string euAccessPermissionRequestJson(const EuAccessPermissionOptions& options);

struct EuPostGetPrescriptionsOptions
{
    Versions::GEM_ERPEU version = Versions::GEM_ERPEU_current();
    model::GemErpEuPrParGetPrescriptionInput::RequestType requestType{model::GemErpEuPrParGetPrescriptionInput::RequestType::demographics};
    std::string_view kvnr = "X234567891";
    std::string accessCode = "abcdef";
    std::string countryCode = "FR";
    std::vector<std::string> prescriptionIds;
};
std::string euPostGetPrescriptionsXml(const EuPostGetPrescriptionsOptions& options);

struct EuPatchTaskOptions
{
    Versions::GEM_ERPEU version = Versions::GEM_ERPEU_current();
    bool EuRedeemableFlag = true;
};
std::string euPatchTaskJson(const EuPatchTaskOptions& options);

struct EuCloseTaskOptions
{
    Versions::GEM_ERPEU version = Versions::GEM_ERPEU_current();
    std::string kvnr = "X234567891";
    std::string prescriptionId = "162.100.000.000.032.60";
    std::string whenHandedOver = "2025-07-09";
    std::string accessCode = "abcdef";
    std::string countryCode = "FR";
    std::string medicationDispenseId = "Example-MedicationDispense-EU";
};
std::string euCloseTaskXml(const EuCloseTaskOptions& options);

} // namespace ResourceTemplates

#endif // TEST_UTIL_RESOURCETEMPLATES_HXX
