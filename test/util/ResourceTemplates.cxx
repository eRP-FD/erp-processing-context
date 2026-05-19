/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "test/util/ResourceTemplates.hxx"
#include "erp/model/WorkflowParameters.hxx"
#include "fhirtools/repository/VersionMapper.hxx"
#include "fhirtools/repository/views/FhirResourceViewList.hxx"
#include "shared/fhir/Fhir.hxx"
#include "shared/model/GemErpPrMedication.hxx"
#include "shared/model/MedicationDispense.hxx"
#include "shared/model/MedicationDispenseBundle.hxx"
#include "shared/model/MedicationDispenseId.hxx"
#include "shared/model/Resource.hxx"
#include "shared/model/ResourceNames.hxx"
#include "shared/model/Timestamp.hxx"
#include "shared/util/Uuid.hxx"
#include "test/util/ResourceManager.hxx"
#include "test/util/TestUtils.hxx"

#include <boost/algorithm/string.hpp>





static constexpr char bsnrIdentifier[] = R"(
          <type>
            <coding>
              <system value="http://terminology.hl7.org/CodeSystem/v2-0203" />
              <code value="BSNR" />
            </coding>
          </type>
          <system value="https://fhir.kbv.de/NamingSystem/KBV_NS_Base_BSNR" />
          <value value="###LANR###" />)";

static constexpr char kzvaIdentifier[] = R"(
          <type>
            <coding>
              <system value="http://fhir.de/CodeSystem/identifier-type-de-basis"/>
              <code value="KZVA"/>
            </coding>
          </type>
          <system value="http://fhir.de/sid/kzbv/kzvabrechnungsnummer"/>
          <value value="###LANR###"/>)";

static constexpr char practitionerRole[] = R"(
        <entry>
        <fullUrl value="http://pvs.praxis.local/fhir/PractitionerRole/34329d70-ccb5-43ae-b551-e5368022b226"/>
        <resource>
            <PractitionerRole xmlns="http://hl7.org/fhir">
                <id value="34329d70-ccb5-43ae-b551-e5368022b226"/>
                <meta>
                    ##VERSION_ID##
                    <profile value="https://fhir.kbv.de/StructureDefinition/KBV_PR_FOR_PractitionerRole|##KBV_PR_FOR_VERSION##"/>
                </meta>
                <practitioner>
                    <reference value="Practitioner/667ffd79-42a3-4002-b7ca-6b9098f20ccb"/>
                </practitioner>
                <organization>
                    <identifier>
                        <system value="http://fhir.de/NamingSystem/asv/teamnummer"/>
                        <value value="003456788"/>
                    </identifier>
                </organization>
            </PractitionerRole>
        </resource>
    </entry>
)";

static constexpr char practitionerRoleSection[] = R"(
                <section>
                    <code>
                        <coding>
                            <system value="https://fhir.kbv.de/CodeSystem/KBV_CS_ERP_Section_Type"/>
                            <code value="FOR_PractitionerRole"/>
                        </coding>
                    </code>
                    <entry>
                        <!-- Referenz auf ASV-Kennzeichen(PractitionerRole)  -->
                        <reference value="PractitionerRole/34329d70-ccb5-43ae-b551-e5368022b226"/>
                    </entry>
                </section>
)";

namespace {
std::string renderVersion(const std::string_view url, const fhirtools::FhirVersion& version)
{
    const auto& config = Configuration::instance();
    const fhirtools::VersionMapper mapper{config.fhirVersionMapping()};
    return to_string(mapper.renderVersion(url, version));

}
}


namespace ResourceTemplates
{

Versions::KBV_ERP::KBV_ERP(FhirVersion ver)
    : FhirVersion{std::move(ver)}
{
}

Versions::GEM_ERP::GEM_ERP(FhirVersion ver)
    : FhirVersion{std::move(ver)}
{
}

Versions::GEM_ERPCHRG::GEM_ERPCHRG(FhirVersion ver)
    : FhirVersion{std::move(ver)}
{
}

Versions::KBV_EVDGA::KBV_EVDGA(FhirVersion ver)
    : FhirVersion{std::move(ver)}
{
}

std::initializer_list<Versions::GEM_ERP> Versions::GEM_ERP_all{
    Versions::GEM_ERP_1_5_2,
    Versions::GEM_ERP_1_6_2,
};

std::initializer_list<Versions::KBV_ERP> Versions::KBV_ERP_all{
    Versions::KBV_ERP_1_3_3,
    Versions::KBV_ERP_1_4_2,
};

std::initializer_list<Versions::KBV_EVDGA> Versions::KBV_EVDGA_all{
    Versions::KBV_EVDGA_1_2_2,
};

std::initializer_list<Versions::GEM_ERPCHRG> Versions::GEM_ERPCHRG_all{
    Versions::GEM_ERPCHRG_1_1_0,
};

std::initializer_list<Versions::DAV_PKV> Versions::DAV_PKV_all{
    Versions::DAV_PKV_1_2,
    Versions::DAV_PKV_1_3,
    Versions::DAV_PKV_1_4_0,
};

std::initializer_list<Versions::GEM_ERPEU> Versions::GEM_ERPEU_all{
    Versions::GEM_ERPEU_1_1_2,
};


fhirtools::DefinitionKey Versions::latest(std::string_view profileUrl, const model::Timestamp& reference)
{
    const auto& fhirInstance = Fhir::instance();
    auto view = fhirInstance.structureRepository(reference);
    auto supported = view.supportedVersions({std::string{profileUrl}});
    if (supported.empty())
    {
        return {std::string{profileUrl}, std::nullopt};
    }
    return std::ranges::max(supported, {}, &fhirtools::DefinitionKey::version);
}

Versions::GEM_ERPCHRG Versions::GEM_ERPCHRG_current(const model::Timestamp& reference)
{
    auto chrgKey = latest(model::resource::structure_definition::chargeItem, reference);
    return chrgKey.version ? GEM_ERPCHRG{std::move(*chrgKey.version)} : GEM_ERPCHRG_1_1_0;
}

Versions::KBV_ERP Versions::KBV_ERP_current(const model::Timestamp& reference)
{
    auto prescriptionKey = latest(model::resource::structure_definition::prescriptionItem, reference);
    return KBV_ERP{prescriptionKey.version.value_or(KBV_ERP_1_3_3)};
}

Versions::KBV_EVDGA Versions::KBV_EVDGA_current(const model::Timestamp& reference)
{
    auto evdgaBundleKey = latest(model::resource::structure_definition::kbv_pr_evdga_bundle, reference);
    return KBV_EVDGA{evdgaBundleKey.version.value_or(KBV_EVDGA_1_2_2)};
}

Versions::GEM_ERP Versions::GEM_ERP_current(const model::Timestamp& reference)
{
    auto taskKey = latest(model::resource::structure_definition::task, reference);
    return GEM_ERP{taskKey.version.value_or(GEM_ERP_1_5_2)};
}

Versions::DAV_PKV::DAV_PKV(FhirVersion ver)
    : FhirVersion{std::move(ver)}
{
}

Versions::DAV_PKV ResourceTemplates::Versions::DAV_PKV_current(const model::Timestamp& reference)
{
    auto dispenseItemKey = latest(model::resource::structure_definition::dispenseItem, reference);
    return dispenseItemKey.version ? DAV_PKV{std::move(*dispenseItemKey.version)} : DAV_PKV_1_3;
}

Versions::GEM_ERPEU Versions::GEM_ERPEU_current(const model::Timestamp& reference [[maybe_unused]])
{
    return GEM_ERPEU_1_1_2;
}

Versions::KBV_FOR Versions::KBV_FOR_current(const model::Timestamp& reference)
{
    auto practitionerKey = latest(model::resource::structure_definition::kbv_for_practitioner, reference);
    return KBV_FOR{practitionerKey.version.value_or(KBV_FOR_1_3_1)};
}

std::string Versions::KBV_ERP::renderVersion() const
{
    return ::renderVersion(model::resource::structure_definition::prescriptionItem, *this);
}

std::string Versions::GEM_ERP::renderVersion() const
{
    return ::renderVersion(model::resource::structure_definition::task, *this);
}

std::string Versions::GEM_ERPCHRG::renderVersion() const
{
    return ::renderVersion(model::resource::structure_definition::communicationChargChangeReq, *this);
}

std::string Versions::DAV_PKV::renderVersion() const
{
    return ::renderVersion(model::resource::structure_definition::dispenseItem, *this);
}

std::string Versions::GEM_ERPEU::renderVersion() const
{
    return ::renderVersion(model::resource::structure_definition::gem_erpeu_pr_par_get_prescription_input, *this);
}

std::string Versions::GEM_ERP_TREZEPT::renderVersion() const
{
    return ::renderVersion(model::resource::structure_definition::erp_tprescription_carbon_copy, *this);
}

Versions::KBV_FOR::KBV_FOR(FhirVersion ver)
    : FhirVersion{std::move(ver)}
{
}

std::string Versions::KBV_FOR::renderVersion() const
{
    return ::renderVersion(model::resource::structure_definition::kbv_for_practitioner, *this);
}

Versions::EPA_MEDICATION::EPA_MEDICATION(FhirVersion ver)
    : FhirVersion{std::move(ver)}
{
}

std::string Versions::EPA_MEDICATION::renderVersion() const
{
    return ::renderVersion(model::resource::structure_definition::epa_op_provide_dispensation_erp_input_parameters,
                           *this);
}

std::string Versions::KBV_EVDGA::renderVersion() const
{
    return ::renderVersion(model::resource::structure_definition::kbv_pr_evdga_bundle, *this);
}

std::string kbvBundleXml(const KbvBundleOptions& bundleOptions)
{
    Expect3(holds_alternative<Versions::KBV_ERP>(bundleOptions.medicationOptions.version),
            "Medication in KBV-Bundle must have KBV-Version", std::logic_error);
    auto& resourceManager = ResourceManager::instance();
    std::string kvid10Ns{model::resource::naming_system::gkvKvid10};
    std::string templateFileName =
        "test/EndpointHandlerTest/kbv_bundle_template_" + bundleOptions.kbvVersion.renderVersion() + ".xml";
    auto bundle = resourceManager.getStringResource(templateFileName);
    if (bundleOptions.generateNewId)
    {
        boost::replace_all(bundle, "8938aff5-720a-414a-b574-114bd8d1e11c", Uuid{}.toString());
    }
    const auto& prescriptionId = bundleOptions.prescriptionId;
    std::string expectedSupplyDuration;

    auto medicationOptions = bundleOptions.medicationOptions;

    std::string insuranceType;
    switch (prescriptionId.type())
    {
        case model::PrescriptionType::apothekenpflichigeArzneimittel:
        case model::PrescriptionType::direkteZuweisung:
        case model::PrescriptionType::digitaleGesundheitsanwendungen:
            insuranceType = "GKV";
            boost::replace_all(bundle, "###PKV_ASSIGNER###", "");
            break;
        case model::PrescriptionType::apothekenpflichtigeArzneimittelPkv:
        case model::PrescriptionType::direkteZuweisungPkv:
            insuranceType = "PKV";
            boost::replace_all(bundle, "###PKV_ASSIGNER###",
                               R"(<assigner><display value="Assigning organization"/></assigner>)");
            break;
        case model::PrescriptionType::tRezept:
            if (bundleOptions.tRezeptIsPkv)
            {
                insuranceType = "PKV";
                boost::replace_all(bundle, "###PKV_ASSIGNER###",
                                   R"(<assigner><display value="Assigning organization"/></assigner>)");
            }
            else
            {
                insuranceType = "GKV";
                boost::replace_all(bundle, "###PKV_ASSIGNER###", "");
            }
            {
                const auto tRezeptExtension = R"(        <extension url="https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Teratogenic">
              <extension url="Off-Label">
                <valueBoolean value="true" />
              </extension>
              <extension url="GebaerfaehigeFrau">
                <valueBoolean value="false" />
              </extension>
              <extension url="EinhaltungSicherheitsmassnahmen">
                <valueBoolean value="true" />
              </extension>
              <extension url="AushaendigungInformationsmaterialien">
                <valueBoolean value="true" />
              </extension>
              <extension url="ErklaerungSachkenntnis">
                <valueBoolean value="true" />
              </extension>
            </extension>)";
                boost::replace_all(bundle, "###MEDICATION_REQUEST_EXTENSION###", tRezeptExtension);
                expectedSupplyDuration =
                    R"-(<expectedSupplyDuration><value value="5"/><unit value="Woche(n)"/></expectedSupplyDuration>)-";
            }
            if (! medicationOptions.medicationCategory.has_value())
            {
                medicationOptions.medicationCategory = "02";
            }
            break;
    }
    boost::replace_all(bundle, "###EXPECTED_SUPPLY_DURATION###", expectedSupplyDuration);
    const std::string coverageInsuranceType{bundleOptions.coverageInsuranceType.value_or(insuranceType)};
    if (bundleOptions.kbvVersion >= Versions::KBV_ERP_1_3_3)
    {
        insuranceType = "KVZ10";
        kvid10Ns = model::resource::naming_system::gkvKvid10;
    }
    boost::replace_all(bundle, "###PATIENT_IDENTIFIER_TYPE_SYSTEM###", bundleOptions.patientIdentifierTypeSystem);
    boost::replace_all(bundle, "###PATIENT_IDENTIFIER_TYPE_CODE###", bundleOptions.forceInsuranceType.value_or(insuranceType));
    boost::replace_all(bundle, "###KVID10###", bundleOptions.forceKvid10Ns.value_or(kvid10Ns));
    boost::replace_all(bundle, "###AUTHORED_ON###", bundleOptions.authoredOn.toGermanDate());
    boost::replace_all(bundle, "###INSURANT_KVNR###", bundleOptions.kvnr);
    boost::replace_all(bundle, "###PRESCRIPTION_ID###", prescriptionId.toString());
    boost::replace_all(bundle, "###COVERAGE_INSURANCE_TYPE###", coverageInsuranceType);
    boost::replace_all(bundle, "###COVERAGE_INSURANCE_SYSTEM###", bundleOptions.coverageInsuranceSystem);
    if (bundleOptions.iknr.id().empty())
    {
        boost::replace_all(bundle, "###PAYOR_IDENTIFIER###", "");
    }
    else
    {
        const std::string argeIknrNs{model::resource::naming_system::argeIknr};
        boost::replace_all(bundle, "###PAYOR_IDENTIFIER###",
            "<identifier>\n###COVERAGE_PAYOR_EXTENSION###"
            "            <system value=\"" + argeIknrNs +  "\"/>\n"
            "            <value value=\"###IKNR###\" />\n"
            "          </identifier>" );
        boost::replace_all(bundle, "###COVERAGE_PAYOR_EXTENSION###", bundleOptions.coveragePayorExtension);
        boost::replace_all(bundle, "###IKNR###", bundleOptions.iknr.id());
    }
    boost::replace_all(bundle, "###STATUS_CO_PAYMENT###", bundleOptions.statusCoPayment);
    boost::replace_all(bundle, "###MEDICATION_REQUEST_EXTENSION###", bundleOptions.medicationRequestExtension);
    boost::replace_all(bundle, "###META_EXTENSION###", bundleOptions.metaExtension);
    switch (bundleOptions.lanr.getType())
    {
        case model::Lanr::Type::lanr:
        case model::Lanr::Type::unspecified:
            boost::replace_all(bundle, "###IDENTIFIER_ORGANIZATION_NUMBER###", bsnrIdentifier);
            break;
        case model::Lanr::Type::zanr:
            boost::replace_all(bundle, "###IDENTIFIER_ORGANIZATION_NUMBER###", kzvaIdentifier);
            break;
    }
    boost::replace_all(bundle, "###LANR###", bundleOptions.lanr.id());
    std::string anrType;
    std::string anrCodeSystem;
    std::string qualificationType;
    switch(bundleOptions.lanr.getType())
    {
        case model::Lanr::Type::lanr:
            anrType = "LANR";
            anrCodeSystem = "http://terminology.hl7.org/CodeSystem/v2-0203";
            qualificationType = "00";
            break;
        case model::Lanr::Type::zanr:
            anrType = "ZANR";
            anrCodeSystem = "http://fhir.de/CodeSystem/identifier-type-de-basis";
            qualificationType = "01";
            break;
        case model::Lanr::Type::unspecified:
            break;
    }
    boost::replace_all(bundle, "###LANR_TYPE###", anrType);
    boost::replace_all(bundle, "###LANR_SYSTEM###", bundleOptions.lanr.namingSystem());
    boost::replace_all(bundle, "###LANR_CODE_SYSTEM###", anrCodeSystem);
    boost::replace_all(bundle, "###QUALIFICATION_TYPE###", qualificationType);
    boost::replace_all(bundle, "####MEDICATION_RESOURCE####", medicationXml(medicationOptions));
    return bundle;
}

std::string ResourceTemplates::KbvBundleMvoOptions::getRedeemPeriodEnd(model::Timestamp authoredOn,
                                                                       std::optional<std::string> redeemPeriodStart)
{
    auto timestamp = authoredOn;
    if (redeemPeriodStart)
    {
        try
        {
           timestamp = model::Timestamp::fromFhirDateTime(*redeemPeriodStart, model::Timestamp::GermanTimezone);
        }
        catch (const model::ModelException&)
        {
            // if conversion fails we stick to authoredOn
        }
    }
    return (timestamp + std::chrono::days{30}).toGermanDate();
}

std::string kbvBundleMvoXml(const KbvBundleMvoOptions& bundleOptions)
{
    using namespace std::string_view_literals;
    auto& resourceManager = ResourceManager::instance();
    const std::string kbvVersionStr{
        bundleOptions.kbvVersion.value_or(ResourceTemplates::Versions::KBV_ERP_current(bundleOptions.authoredOn)).renderVersion()};
    std::string templateFileName =
        "test/EndpointHandlerTest/kbv_bundle_mehrfachverordnung_template_" + kbvVersionStr + ".xml";

    auto bundle = resourceManager.getStringResource(templateFileName);
    boost::replace_all(bundle, "###TIMESTAMP###", bundleOptions.authoredOn.toXsDateTime());
    boost::replace_all(bundle, "###AUTHORED_ON###", bundleOptions.authoredOn.toGermanDate());
    boost::replace_all(bundle, "###PRESCRIPTION_ID###", bundleOptions.prescriptionId.toString());
    boost::replace_all(bundle, "##KVNR##", bundleOptions.kvnr);
    boost::replace_all(bundle, "###LEGAL_BASIS_CODE###", bundleOptions.legalBasisCode);
    const bool withPractitionerRole =  std::set{"01"sv, "11"sv}.contains(bundleOptions.legalBasisCode);
    boost::replace_all(bundle, "###PRACTITIONER_ROLE_SECTION###", withPractitionerRole?practitionerRoleSection:std::string_view{});
    boost::replace_all(bundle, "###PRACTITIONER_ROLE###", withPractitionerRole?practitionerRole:std::string_view{});
    std::string versionId;
    if (bundleOptions.kbvVersion >= Versions::KBV_ERP_1_4_2)
    {
        versionId = R"(<versionId value="1"/>)";
    }
    boost::replace_all(bundle, "##VERSION_ID##", versionId);
    boost::replace_all(bundle,"##KBV_PR_FOR_VERSION##", Versions::KBV_FOR_current().renderVersion());
    boost::replace_all(bundle, "###NUMERATOR###", std::to_string(bundleOptions.numerator));
    boost::replace_all(bundle, "###DENOMINATOR###", std::to_string(bundleOptions.denominator));
    std::string redeemPeriodStart;
    if (bundleOptions.redeemPeriodStart.has_value())
    {
        redeemPeriodStart = "<start value=\"" + bundleOptions.redeemPeriodStart.value() + "\" />";
    }
    boost::replace_all(bundle, "###REDEEM_START###", redeemPeriodStart);

    std::string redeemPeriodEnd;
    if (bundleOptions.redeemPeriodEnd.has_value())
    {
        redeemPeriodEnd = "<end value=\"" + bundleOptions.redeemPeriodEnd.value() + "\" />";
    }
    boost::replace_all(bundle, "###REDEEM_END###", redeemPeriodEnd);
    boost::replace_all(bundle, "###MVOID###", bundleOptions.mvoId);
    boost::replace_all(bundle, "##CO_PAYMENT_VALUE##", bundleOptions.coPaymentValue);
    return bundle;
}

std::string kbvBundlePkvXml(const KbvBundlePkvOptions& bundleOptions)
{
    auto& resourceManager = ResourceManager::instance();
    const std::string kbvVersionStr{bundleOptions.kbvVersion.renderVersion()};
    std::string templateFileName = "test/EndpointHandlerTest/kbv_pkv_bundle_template_" + kbvVersionStr + ".xml";
    auto bundle = resourceManager.getStringResource(templateFileName);
    boost::replace_all(bundle, "###PRESCRIPTION_ID###", bundleOptions.prescriptionId.toString());
    boost::replace_all(bundle, "###KVNR###", bundleOptions.kvnr.id());
    boost::replace_all(bundle, "###AUTHORED_ON###", bundleOptions.authoredOn.toGermanDate());
    return bundle;
}

std::variant<model::PrescriptionId, std::string> MedicationDispenseOptions::guessPrescriptionId(
    std::variant<model::MedicationDispenseId, std::string>& medicationDispenseId)
{
    struct Guess {
        using ResultType = std::variant<model::PrescriptionId, std::string>;
        ResultType operator()(const model::MedicationDispenseId& mediDisId) const
        {
            return mediDisId.getPrescriptionId();
        }
        ResultType operator()(const std::string& mediDisId)
        {
            return mediDisId.substr(0, mediDisId.rfind('-'));
        }
    };
    return std::visit(Guess{}, medicationDispenseId);
}

std::string medicationDispenseBundleXml(const MedicationDispenseBundleOptions& bundleOptions)
{
    const fhirtools::DefinitionKey profileKey{
        std::string{profile(model::ProfileType::MedicationDispenseBundle).value()}, bundleOptions.gematikVersion};
    model::MedicationDispenseBundle bundle{model::BundleType::collection, profileKey};
    bundle.setIdentifier(bundleOptions.prescriptionId);
    for (size_t idx = 0; const auto& medicationDispenseOpt : bundleOptions.medicationDispenses)
    {
        std::optional<model::MedicationDispenseId> id;
        if (std::holds_alternative<model::PrescriptionId>(medicationDispenseOpt.prescriptionId))
        {
            id.emplace(std::get<model::PrescriptionId>(medicationDispenseOpt.prescriptionId), idx);
        }
        else
        {
            id.emplace(model::PrescriptionId::fromStringNoValidation(
                           std::get<std::string>(medicationDispenseOpt.prescriptionId)),
                       idx);
        }
        ++idx;
        auto medicationDispense =
            model::MedicationDispense::fromXmlNoValidation(medicationDispenseXml(medicationDispenseOpt));
        medicationDispense.setId(*id);
        std::string fullUrl = "http://pvs.praxis-topp-gluecklich.local/fhir/MedicationDispense/";
        fullUrl.append(id->toString());
        bundle.addResource(fullUrl, std::nullopt, std::nullopt, std::move(medicationDispense).jsonDocument());
    }
    return bundle.serializeToXmlString();
}

model::MedicationDispenseBundle
internal_type::medicationDispenseBundle(const MedicationDispenseBundleOptions& bundleOptions)
{
    const fhirtools::DefinitionKey profileKey{
        std::string{profile(model::ProfileType::MedicationDispenseBundle).value()}, bundleOptions.gematikVersion};
    model::MedicationDispenseBundle bundle{model::BundleType::collection, profileKey};
    bundle.setIdentifier(bundleOptions.prescriptionId);
    for (size_t idx = 0; const auto& medicationDispenseOpt : bundleOptions.medicationDispenses)
    {
        std::optional<model::MedicationDispenseId> id;
        if (std::holds_alternative<model::PrescriptionId>(medicationDispenseOpt.prescriptionId))
        {
            id.emplace(std::get<model::PrescriptionId>(medicationDispenseOpt.prescriptionId), idx);
        }
        else
        {
            id.emplace(model::PrescriptionId::fromStringNoValidation(
                           std::get<std::string>(medicationDispenseOpt.prescriptionId)),
                       idx);
        }
        ++idx;
        MedicationDispenseOptions medicationDispenseOptions = medicationDispenseOpt;
        if (bundleOptions.gematikVersion >= Versions::GEM_ERP_1_4)
        {
            auto medicationOptions = std::get<MedicationOptions>(medicationDispenseOpt.medication);
            medicationOptions.id = medicationOptions.id.value_or(Uuid{});
            medicationOptions.version = bundleOptions.gematikVersion;
            medicationDispenseOptions.medication = "urn:uuid:" + *medicationOptions.id;
            auto medication = model::GemErpPrMedication::fromXmlNoValidation(medicationXml(medicationOptions));
            bundle.addResource("http://pvs.praxis-topp-gluecklich.local/fhir/Medication/" + *medicationOptions.id,
                               std::nullopt, std::nullopt, std::move(medication).jsonDocument());
        }
        auto medicationDispense =
            model::MedicationDispense::fromXmlNoValidation(medicationDispenseXml(medicationDispenseOptions));
        medicationDispense.setId(*id);
        std::string fullUrl = "http://pvs.praxis-topp-gluecklich.local/fhir/MedicationDispense/";
        fullUrl.append(id->toString());
        bundle.addResource(fullUrl, std::nullopt, std::nullopt, std::move(medicationDispense).jsonDocument());
    }
    return bundle;
}


std::string medicationDispenseXml(const MedicationDispenseOptions& medicationDispenseOptions)
{
    auto renderVersionString = medicationDispenseOptions.gematikVersion.renderVersion();
    auto& resourceManager = ResourceManager::instance();
    std::string templateFileName = "test/EndpointHandlerTest/medication_dispense_template_" + renderVersionString + ".xml";
    auto bundle = resourceManager.getStringResource(templateFileName);
    auto medicationOptions = medicationDispenseOptions.medication;
    struct ToString {
        std::string operator()(const model::PrescriptionId& prescriptionId) const
        {
            return prescriptionId.toString();
        }
        std::string operator()(const model::Timestamp& timestamp) const
        {
            return timestamp.toGermanDate();
        }
        std::string operator()(const std::string& str)
        {
            return str;
        }
        std::string operator()(const std::monostate&)
        {
            Fail2("cannot convert monostate to string.", std::logic_error);
        }
        std::string operator()(const model::MedicationDispenseId& medicationDispenseId)
        {
            return medicationDispenseId.toString();
        }
    };
    ToString toString{};
    boost::replace_all(bundle, "###MEDICATION_DISPENSE_ID###", std::visit(toString, medicationDispenseOptions.id));
    boost::replace_all(bundle, "###PRESCRIPTION_ID###", std::visit(toString, medicationDispenseOptions.prescriptionId));
    boost::replace_all(bundle, "###INSURANT_KVNR###", medicationDispenseOptions.kvnr);
    boost::replace_all(bundle, "###TELEMATIK_ID###", medicationDispenseOptions.telematikId);
    boost::replace_all(bundle, "###WHENHANDEDOVER###",
                       std::visit(toString, medicationDispenseOptions.whenHandedOver));
    struct InsertMedication {
        void operator()(const ResourceTemplates::MedicationDispenseOptions::MedicationReference& ref)
        {
            boost::replace_all(bundle, "###CONTAINED_MEDICATION###", std::string{});
            boost::replace_all(bundle, "###MEDICATION_REFERENCE###", ref);
        }
        void operator()(ResourceTemplates::MedicationOptions medicationOptions)
        {
            if (!medicationOptions.id)
            {
                medicationOptions.id = Uuid{};
            }
            if (std::holds_alternative<std::monostate>(medicationOptions.version))
            {
                if (medicationDispenseOptions.gematikVersion >= Versions::GEM_ERP_1_4)
                {
                    medicationOptions.version = medicationDispenseOptions.gematikVersion;
                }
                else
                {
                    medicationOptions.version = Versions::KBV_ERP_1_1_0;
                }
            }
            boost::replace_all(bundle, "###CONTAINED_MEDICATION###",
                               "\n    <contained>\n" + medicationXml(medicationOptions) + "\n    </contained>\n");
            boost::replace_all(bundle, "###MEDICATION_REFERENCE###", "#" + medicationOptions.id.value());
        }
        std::string& bundle;
        const MedicationDispenseOptions& medicationDispenseOptions;
    };
    visit(InsertMedication{bundle, medicationDispenseOptions}, medicationOptions);
    std::string whenPrepared;
    if (!std::holds_alternative<std::monostate>(medicationDispenseOptions.whenPrepared))
    {
        whenPrepared = "<whenPrepared value=\"" + visit(toString, medicationDispenseOptions.whenPrepared) + "\" />";
    }
    boost::replace_all(bundle, "###WHENPREPARED###", whenPrepared);
    return bundle;
}

std::string medicationXml(const MedicationOptions& medicationOptions)
{
    struct GetTemplate {
        std::string operator()(const Versions::KBV_ERP& version)
        {
            return (std::ostringstream{} << medicationOptions.templatePrefix << version.renderVersion() << ".xml").str();
        }
        std::string operator()(const Versions::GEM_ERP& version)
        {
            return (std::ostringstream{} << medicationOptions.templatePrefix << "gem_" << version.renderVersion() << ".xml").str();
        }
        std::string operator()(const std::monostate&)
        {
            if (auto gemVer = Versions::GEM_ERP_current(model::Timestamp::now()); gemVer >= Versions::GEM_ERP_1_4)
            {
                return (*this)(gemVer);
            }
            return (*this)(Versions::KBV_ERP_current(model::Timestamp::now()));
        }

        const MedicationOptions& medicationOptions;
    };

    using namespace std::string_literals;
    auto& resourceManager = ResourceManager::instance();
    auto medication =
        resourceManager.getStringResource(visit(GetTemplate{medicationOptions}, medicationOptions.version));
    boost::replace_all(medication, "###MEDICATION_ID###",
                       medicationOptions.id.value_or("001413e4-a5e9-48da-9b07-c17bab476407"));
    boost::replace_all(medication, "###DARREICHUNGSFORM###", medicationOptions.darreichungsform);
    boost::replace_all(medication, "###MEDICATION_CATEGORY###", medicationOptions.medicationCategory.value_or("00"));
    boost::replace_all(medication, "###PZN###", medicationOptions.pzn.id());
    return medication;
}

std::string medicationDispenseDigaXml(const MedicationDispenseDiGAOptions& options)
{
    auto& resourceManager = ResourceManager::instance();
    std::string const templateFileName =
        "test/EndpointHandlerTest/GEM_ERP_PR_MedicationDispense_DiGA_template_" + options.version.renderVersion() + ".xml";
    auto medicationDispense = resourceManager.getStringResource(templateFileName);
    boost::replace_all(medicationDispense, "##PRESCRIPTION_ID##", options.prescriptionId);
    boost::replace_all(medicationDispense, "##KVNR##", options.kvnr);
    boost::replace_all(medicationDispense, "##TELEMATIK_ID##", options.telematikId);
    boost::replace_all(medicationDispense, "##WHENHANDEDOVER##", options.whenHandedOver);
    const auto redeemCodeExtension =
        options.withRedeemCode
            ? R"(<extension url="https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_EX_RedeemCode"><valueString value="DE12345678901234" /></extension>)"
            : "";
    const auto dataAbsentExtension =
        options.withRedeemCode
            ? ""
            : R"(<extension url="http://hl7.org/fhir/StructureDefinition/data-absent-reason"><valueCode value="asked-declined"/></extension>)";
    boost::replace_all(medicationDispense, "##REDEEM_CODE_EXTENSION##", redeemCodeExtension);
    boost::replace_all(medicationDispense, "##DATA_ABSENT_EXTENSION##", dataAbsentExtension);
    return medicationDispense;
}

std::string taskJson(const TaskOptions& taskOptions)
{
    auto& resourceManager = ResourceManager::instance();
    auto renderVersion = taskOptions.gematikVersion.renderVersion();
    std::string templateFileName;
    switch (taskOptions.taskType)
    {
        case TaskType::Draft:
            templateFileName = "test/EndpointHandlerTest/task_draft_template_" + renderVersion + ".json";
            break;
        case TaskType::Ready:
            templateFileName = "test/EndpointHandlerTest/task_ready_template_" + renderVersion + ".json";
            break;
        case TaskType::InProgress:
            templateFileName = "test/EndpointHandlerTest/task_inprogress_template_" + renderVersion + ".json";
            break;
        case TaskType::Completed:
            templateFileName = "test/EndpointHandlerTest/task_completed_template_" + renderVersion + ".json";
            break;
    }

    auto task = resourceManager.getStringResource(templateFileName);
    boost::replace_all(task, "###INSURANT_KVNR###", taskOptions.kvnr);
    boost::replace_all(task, "###PRESCRIPTION_ID###", taskOptions.prescriptionId.toString());
    boost::replace_all(task, "###PRESCRIPTION_TYPE###", std::to_string(static_cast<int>(taskOptions.prescriptionId.type())));
    boost::replace_all(task, "###PRESCRIPTION_TYPE_DISPLAY###", model::PrescriptionTypeDisplay.at(taskOptions.prescriptionId.type()));
    boost::replace_all(task, "###TIMESTAMP###", taskOptions.timestamp.toXsDateTime());
    boost::replace_all(task, "###OWNING_PHARMACY###", taskOptions.owningPharmacy);
    boost::replace_all(task, "###PRESCRIPTION_UUID###", taskOptions.prescriptionId.deriveUuid(model::uuidFeaturePrescription));
    boost::replace_all(task, "###EXPIRY_DATE###", taskOptions.expirydate.toGermanDate());

    auto kvnrNamingSystem = taskOptions.kvnrNamingSystem;
    if (!kvnrNamingSystem)
    {
        kvnrNamingSystem = model::resource::naming_system::gkvKvid10;
    }
    boost::replace_all(task, "###KVNR_NAMING_SYSTEM###", std::string(*kvnrNamingSystem));

    return task;
}

std::string chargeItemXml(const ChargeItemOptions& chargeItemOptions, std::string path /* = "" */)
{
    auto& resourceManager = ResourceManager::instance();
    std::string renderVersion = chargeItemOptions.version.renderVersion();
    std::string templateFileName = "test/EndpointHandlerTest/" + path;
    if (path.empty())
    {
        switch (chargeItemOptions.operation)
        {
            case ChargeItemOptions::OperationType::Post:
                templateFileName = "test/EndpointHandlerTest/charge_item_POST_" + renderVersion + "_template.xml";
                break;
            case ChargeItemOptions::OperationType::Put:
                templateFileName = "test/EndpointHandlerTest/charge_item_PUT_" + renderVersion + "_template.xml";
                break;
        }
    }
    const auto& prescriptionId = chargeItemOptions.prescriptionId;
    auto chargeItem = resourceManager.getStringResource(templateFileName);
    boost::replace_all(chargeItem, "##KVNR##", chargeItemOptions.kvnr.id());
    boost::replace_all(chargeItem, "##PRESCRIPTION_ID##", prescriptionId.toString());
    boost::replace_all(chargeItem, "##DISPENSE_BUNDLE##", chargeItemOptions.dispenseBundleBase64);
    boost::replace_all(chargeItem, "##RECEIPT_REF##",
                       Uuid{prescriptionId.deriveUuid(model::uuidFeatureReceipt)}.toUrn());
    boost::replace_all(chargeItem, "##PRESCRIPTION_REF##",
                       Uuid{prescriptionId.deriveUuid(model::uuidFeaturePrescription)}.toUrn());

    return chargeItem;
}

namespace
{
constexpr std::string_view markingFlagElementTemplate = R"^(
    {
        "name": "operation",
        "part": [
          {
            "name": "type",
            "valueCode": "add"
          },
          {
            "name": "path",
            "valueString": "ChargeItem.extension('https://gematik.de/fhir/erpchrg/StructureDefinition/GEM_ERPCHRG_EX_MarkingFlag').extension('##EXTENSION_NAME##')"
          },
          {
            "name": "name",
            "valueString": "valueBoolean"
          },
          {
            "name": "value",
            "valueBoolean": ##VALUE_BOOLEAN##
          }
        ]
    }
    )^";

std::string constructMarkingFlagElement(const std::string& extensionName, bool value)
{
    auto result = String::replaceAll(std::string{markingFlagElementTemplate}, "##EXTENSION_NAME##", extensionName);
    result = String::replaceAll(result, "##VALUE_BOOLEAN##", value ? "true" : "false");

    return result;
}
} // anonymous namespace

std::string legacyPatchChargeItemBodyJson(const PatchChargeItemOptions& patchChargeItemOptions)
{
    std::string result{R"^(
        {
            "resourceType": "Parameters",
            "parameter": [
        )^"};

    if (patchChargeItemOptions.insuranceProviderMarking.has_value())
    {
        result += constructMarkingFlagElement("insuranceProvider", *patchChargeItemOptions.insuranceProviderMarking);
        result += ",";
    }

    if (patchChargeItemOptions.subsidyMarking.has_value())
    {
        result += constructMarkingFlagElement("subsidy", *patchChargeItemOptions.subsidyMarking);
        result += ",";
    }

    if (patchChargeItemOptions.taxOfficeMarking.has_value())
    {
        result += constructMarkingFlagElement("taxOffice", *patchChargeItemOptions.taxOfficeMarking);
        result += ",";
    }

    if (result.back() == ',')
    {
        result.pop_back();
    }

    result += "]}";

    return result;
}

std::string patchChargeItemJson(const PatchChargeItemOptions& patchChargeItemOptions)
{
    if (patchChargeItemOptions.version < Versions::GEM_ERPCHRG_1_1_0)
    {
        return legacyPatchChargeItemBodyJson(patchChargeItemOptions);
    }

    const std::string partTemplate = R"({
"name": "##NAME##",
"valueBoolean": ##VALUE_BOOLEAN##
})";
    std::string sep;

    auto& resourceManager = ResourceManager::instance();
    std::string patchParameters =
        resourceManager.getStringResource("test/EndpointHandlerTest/patch_charge_item_parameters_template_" +
                                          patchChargeItemOptions.version.renderVersion() + ".json");
    if (patchChargeItemOptions.insuranceProviderMarking.has_value())
    {
        auto part = boost::replace_all_copy(partTemplate, "##NAME##", "insuranceProvider");
        boost::replace_all(part, "##VALUE_BOOLEAN##",
                           patchChargeItemOptions.insuranceProviderMarking.value() ? "true" : "false");
        boost::replace_all(patchParameters, "##INSURANCE_PROVIDER##", sep + part);
        sep = ",";
    }
    else
    {
        boost::replace_all(patchParameters, "##INSURANCE_PROVIDER##", "");
    }
    if (patchChargeItemOptions.subsidyMarking.has_value())
    {
        auto part = boost::replace_all_copy(partTemplate, "##NAME##", "subsidy");
        boost::replace_all(part, "##VALUE_BOOLEAN##", patchChargeItemOptions.subsidyMarking.value() ? "true" : "false");
        boost::replace_all(patchParameters, "##SUBSIDY##", sep + part);
        sep = ",";
    }
    else
    {
        boost::replace_all(patchParameters, "##SUBSIDY##", "");
    }
    if (patchChargeItemOptions.taxOfficeMarking.has_value())
    {
        auto part = boost::replace_all_copy(partTemplate, "##NAME##", "taxOffice");
        boost::replace_all(part, "##VALUE_BOOLEAN##",
                           patchChargeItemOptions.taxOfficeMarking.value() ? "true" : "false");
        boost::replace_all(patchParameters, "##TAX_OFFICE##", sep + part);
        sep = ",";
    }
    else
    {
        boost::replace_all(patchParameters, "##TAX_OFFICE##", "");
    }
    return patchParameters;
}

std::string davDispenseItemXml(const DavDispenseItemOptions& dispenseItemOptions)
{
    auto& resourceManager = ResourceManager::instance();
    Expect3(dispenseItemOptions.davPkvVersion != fhirtools::FhirVersion::notVersioned,
            "dispense item must have a version", std::logic_error);
    std::string dispenseItem = resourceManager.getStringResource("test/EndpointHandlerTest/DAV_DispenseItem_template_" +
                                                                 dispenseItemOptions.davPkvVersion.renderVersion() + ".xml");

    boost::replace_all(dispenseItem, "##PRESCRIPTION_ID##", dispenseItemOptions.prescriptionId.toString());
    boost::replace_all(dispenseItem, "###WHEN_HANDED_OVER###", dispenseItemOptions.whenHandenOver.toGermanDate());

    return dispenseItem;
}

std::string medicationDispenseOperationParametersXml(const MedicationDispenseOperationParametersOptions& options)
{
    auto part = [](std::string name, std::string resource) {
        std::string result;
        result += "  <part>\n";
        result += "    <name value=\"" + name += "\" />\n";
        result += "    <resource>\n" + resource + "\n    </resource>\n";
        result += "  </part>\n";
        return result;
    };
    std::string profile =
        std::string{model::profile(options.profileType).value()}.append(1, '|').append(options.version.renderVersion());
    std::string result = "<Parameters xmlns=\"http://hl7.org/fhir\">\n  <id value=\"" + options.id + "\"/>\n" +
                         "  <meta>\n"
                         "    <profile value=\"" + profile + "\" />\n"
                         "  </meta>\n";
    for (auto dispense: options.medicationDispenses)
    {
        Expect(std::holds_alternative<MedicationOptions>(dispense.medication), "medication must not be MedicationReference");
        auto medicationOptions = get<MedicationOptions>(dispense.medication);
        if (!medicationOptions.id)
        {
            medicationOptions.id = Uuid{};
        }
        dispense.medication =  "Medication/" + medicationOptions.id.value();
        result += "<parameter>\n";
        result += "  <name value=\"rxDispensation\" />\n";
        result += part("medicationDispense", medicationDispenseXml(dispense));
        result += part("medication", medicationXml(medicationOptions));
        result += "</parameter>\n";
    }
    for (const auto& medicationDispenseDiGa : options.medicationDispensesDiGA)
    {
        result += "<parameter>\n";
        result += "  <name value=\"rxDispensation\" />\n";
        result += part("medicationDispense", medicationDispenseDigaXml(medicationDispenseDiGa));
        result += "</parameter>\n";
    }
    result += "</Parameters>";
    return result;
}

std::string dispenseOrCloseTaskBodyXml(const MedicationDispenseOperationParametersOptions& options)
{
    if (options.version >= Versions::GEM_ERP_1_4)
    {
        return medicationDispenseOperationParametersXml(options);
    }
    if (options.medicationDispenses.size() == 1)
    {
        return medicationDispenseXml(options.medicationDispenses.front());
    }
    return medicationDispenseBundleXml({
        .gematikVersion = options.version,
        .medicationDispenses = options.medicationDispenses,
    });
}

std::string evdgaBundleXml(const EvdgaBundleOptions& options)
{
    using namespace std::string_literals;
    static constexpr char deviceRequestExtension[] = R"(
<extension url="https://fhir.kbv.de/StructureDefinition/KBV_EX_FOR_Accident">
    <extension url="Unfallkennzeichen">
        <valueCoding>
            <system value="https://fhir.kbv.de/CodeSystem/KBV_CS_FOR_Ursache_Type"/>
            <code value="4"/>
        </valueCoding>
    </extension>
</extension>
)";


    auto& resourceManager = ResourceManager::instance();
    std::string templateFileName =
        "test/EndpointHandlerTest/EVDGA_Bundle_template_" + options.version.renderVersion() + ".xml";
    auto evdgaBundle = resourceManager.getStringResource(templateFileName);

    boost::replace_all(evdgaBundle, "##PRESCRIPTION_ID##", options.prescriptionId);
    boost::replace_all(evdgaBundle, "##TIMESTAMP##", options.timestamp);
    boost::replace_all(evdgaBundle, "##KVNR##", options.kvnr);
    boost::replace_all(evdgaBundle, "##COVERAGE_SYSTEM##", options.coverageSystem);
    boost::replace_all(evdgaBundle, "##COVERAGE_CODE##", options.coverageCode);
    boost::replace_all(evdgaBundle, "##AUTHORED_ON##", options.authoredOn);
    boost::replace_all(evdgaBundle, "##DEVICE_REQUEST_EXTENSION##",
                       std::set{"BG"s, "UK"s}.contains(options.coverageCode) ? deviceRequestExtension : "");

    return evdgaBundle;
}

std::string consentJson(const ConsentOptions& options)
{
    struct VersionStr {
        std::string operator()(const Versions::GEM_ERPCHRG& ver)
        {
            Expect3(options.consentCategory == model::ConsentType::CHARGCONS, "version type doesn't match consentType",
                    std::logic_error);
            return ver.renderVersion();
        }
        std::string operator()(const Versions::GEM_ERPEU& ver)
        {
            Expect3(options.consentCategory == model::ConsentType::EUDISPCONS, "version type doesn't match consentType",
                    std::logic_error);
            return ver.renderVersion();
        }
        std::string operator()(const std::monostate&)
        {
            switch (options.consentCategory)
            {
                case model::ConsentType::CHARGCONS:
                    return Versions::GEM_ERPCHRG_current().renderVersion();
                case model::ConsentType::EUDISPCONS:
                    return Versions::GEM_ERPEU_current().renderVersion();
            }
            Fail2("Invalid Consent Type: " + std::to_string(static_cast<uintmax_t>(options.consentCategory)), std::logic_error);
        }
        const ConsentOptions& options;
    };
    auto& resourceManager = ResourceManager::instance();
    auto consent = resourceManager.getStringResource("test/EndpointHandlerTest/consent_template.json");
    boost::replace_all(consent, "##KVNR##", options.kvnr);
    boost::replace_all(consent, "##DATETIME##", options.timestamp);
    boost::replace_all(consent, "##CATEGORY##", magic_enum::enum_name(options.consentCategory));
    auto version = std::visit(VersionStr{options}, options.version);
    switch (options.consentCategory)
    {
        case model::ConsentType::CHARGCONS: {
            boost::replace_all(consent, "##CATEGORY_SYSTEM##", "https://gematik.de/fhir/erpchrg/CodeSystem/GEM_ERPCHRG_CS_ConsentType");
            boost::replace_all(consent, "##PROFILE##",
                               "https://gematik.de/fhir/erpchrg/StructureDefinition/GEM_ERPCHRG_PR_Consent|" + version);
            boost::replace_all(consent, "##KVNR-NS##", "http://fhir.de/sid/gkv/kvid-10");
            break;
        }
        case model::ConsentType::EUDISPCONS: {
            boost::replace_all(consent, "##CATEGORY_SYSTEM##", "https://gematik.de/fhir/erp-eu/CodeSystem/GEM_ERPEU_CS_ConsentType");
            boost::replace_all(consent, "##PROFILE##",
                               "https://gematik.de/fhir/erp-eu/StructureDefinition/GEM_ERPEU_PR_Consent|" + version);
            boost::replace_all(consent, "##KVNR-NS##", "http://fhir.de/sid/gkv/kvid-10");
            break;
        }
    }
    return consent;
}

std::string euAccessPermissionRequestJson(const EuAccessPermissionOptions& options)
{
    auto& resourceManager = ResourceManager::instance();
    auto resource = resourceManager.getStringResource(
        "test/EndpointHandlerTest/GEM_ERPEU_PR_PAR_Access_Authorization_Request_template.json");
    boost::replace_all(resource, "###COUNTRY_CODE###", options.twoLetterCountryCode);
    boost::replace_all(resource, "###ACCESS_CODE###", options.accessCode);
    return resource;
}

std::string euPostGetPrescriptionsXml(const EuPostGetPrescriptionsOptions& options)
{
    auto& resourceManager = ResourceManager::instance();
    auto resource =
        resourceManager.getStringResource("test/EndpointHandlerTest/GEM_ERPEU_PR_PAR_Post_Get_Prescriptions_template_" +
                                          options.version.renderVersion() + ".xml");
    boost::replace_all(resource, "##REQUEST_TYPE##",
                       model::GemErpEuPrParGetPrescriptionInput::requestTypeToString(options.requestType));
    boost::replace_all(resource, "##KVNR##", options.kvnr);
    boost::replace_all(resource, "##ACCESS_CODE##", options.accessCode);
    boost::replace_all(resource, "##COUNTRY_CODE##", options.countryCode);
    const std::string prescriptionIdTemplate = R"(
<part>
    <name value="prescription-id" />
    <valueIdentifier>
        <system value="https://gematik.de/fhir/erp/NamingSystem/GEM_ERP_NS_PrescriptionId" />
        <value value="##PRESCRIPTION_ID##" />
    </valueIdentifier>
</part>
)";
    std::string prescriptionIds;
    for (const auto& prescriptionId : options.prescriptionIds)
    {
        std::string prescriptionIdXml = prescriptionIdTemplate;
        boost::replace_all(prescriptionIdXml, "##PRESCRIPTION_ID##", prescriptionId);
        prescriptionIds += prescriptionIdXml;
    }
    boost::replace_all(resource, "##PRESCRIPTION_IDS##", prescriptionIds);
    return resource;
}

std::string euPatchTaskJson(const EuPatchTaskOptions& options)
{
    auto& resourceManager = ResourceManager::instance();
    auto resource = resourceManager.getStringResource("test/EndpointHandlerTest/gem_erpeu_pr_par_patch_task_input_" +
                                                      options.version.renderVersion() + "_template.json");
    boost::replace_all(resource, "##FLAG##", options.EuRedeemableFlag ? "true" : "false");
    return resource;
}

std::string euCloseTaskXml(const EuCloseTaskOptions& options)
{
    auto& resourceManager = ResourceManager::instance();
    auto resource = resourceManager.getStringResource("test/EndpointHandlerTest/gem_erpeu_pr_par_closeoperation_input_" +
                                                      options.version.renderVersion() + "_template.xml");
    boost::replace_all(resource, "##KVNR##", options.kvnr);
    boost::replace_all(resource, "##PRESCRIPTIONID##", options.prescriptionId);
    boost::replace_all(resource, "##WHENHANDEDOVER##", options.whenHandedOver);
    boost::replace_all(resource, "##ACCESSCODE##", options.accessCode);
    boost::replace_all(resource, "##COUNTRYCODE##", options.countryCode);
    boost::replace_all(resource, "##MEDICATIONDISPENSEID##", options.medicationDispenseId);

    return resource;
}

std::string vzdFhirBundleJson(const VZDFhirBundleOptions& options /* = {} */ [[maybe_unused]])
{
    auto& resourceManager = ResourceManager::instance();
    auto resource = resourceManager.getStringResource("test/exporter/VZD-FHIR-Directory.bundle.xml");
    boost::replace_all(resource, "##NAME##", options.name);
    boost::replace_all(resource, "##ADDRESS_TEXT##", options.addressText);
    return resource;
}

std::string vzdSearchSetJson(const std::string_view path, const VzdSearchSetOptions& options)
{
    auto& resourceManager = ResourceManager::instance();
    auto resource =
        resourceManager.getStringResource(path);
    boost::replace_all(resource, "##TELEMATIKID##", options.telematikId);
    return resource;
}

std::string kbvPractitionerXml(const KbvPractitionerOptions& options)
{
    auto& resourceManager = ResourceManager::instance();
    auto resource = resourceManager.getStringResource("test/EndpointHandlerTest/KBV_PR_FOR_Practitioner_template_" +
                                                      options.kbvPrForVersion.renderVersion() + ".xml");
    boost::replace_all(resource, "##ID##", options.id);
    boost::replace_all(resource, "##TELEMATIKID##", options.telematikId);
    boost::replace_all(resource, "##GIVEN_NAME##", options.givenName);
    return resource;
}

}// namespace ResourceTemplates
