/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "test/util/ResourceTemplates.hxx"
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

namespace ResourceTemplates
{

std::initializer_list<Versions::GEM_ERP> Versions::GEM_ERP_all{
    Versions::GEM_ERP_1_2,
    Versions::GEM_ERP_1_3,
    Versions::GEM_ERP_1_4,
};
std::initializer_list<Versions::KBV_ERP> Versions::KBV_ERP_all{
    Versions::KBV_ERP_1_1_0,
};

std::initializer_list<Versions::KBV_EVDGA> Versions::KBV_EVDGA_all{
    Versions::KBV_EVDGA_1_1,
};

std::initializer_list<Versions::GEM_ERPCHRG> Versions::GEM_ERPCHRG_all{
    Versions::GEM_ERPCHRG_1_0,
};

std::initializer_list<Versions::DAV_PKV> Versions::DAV_PKV_all{
    Versions::DAV_PKV_1_2,
};

Versions::GEM_ERP::GEM_ERP(fhirtools::FhirVersion ver)
    : FhirVersion{std::move(ver)}
{
}

fhirtools::DefinitionKey Versions::latest(std::string_view profileUrl, const model::Timestamp& reference)
{
    const auto& fhirInstance = Fhir::instance();
    const auto& backend = fhirInstance.backend();
    auto view = fhirInstance.structureRepository(reference);
    auto supported = view.supportedVersions(&backend, {std::string{profileUrl}});
    if (supported.empty())
    {
        return {std::string{profileUrl}, std::nullopt};
    }
    return std::ranges::max(supported, {}, &fhirtools::DefinitionKey::version);
}

Versions::GEM_ERPCHRG Versions::GEM_ERPCHRG_current(const model::Timestamp& reference [[maybe_unused]])
{
    return GEM_ERPCHRG_1_0;
}

Versions::KBV_ERP Versions::KBV_ERP_current(const model::Timestamp& reference [[maybe_unused]])
{
    return KBV_ERP_1_1_0;
}

Versions::KBV_EVDGA Versions::KBV_EVDGA_current(const model::Timestamp& reference [[maybe_unused]])
{
    return KBV_EVDGA_1_1;
}

Versions::GEM_ERP Versions::GEM_ERP_current(const model::Timestamp& reference)
{
    auto taskKey = latest(model::resource::structure_definition::task, reference);
    return GEM_ERP{taskKey.version.value_or(GEM_ERP_1_4)};
}

Versions::DAV_PKV::DAV_PKV(FhirVersion ver)
    : FhirVersion{std::move(ver)}
{
}

Versions::DAV_PKV ResourceTemplates::Versions::DAV_PKV_current(const model::Timestamp& reference [[maybe_unused]])
{
    auto dispenseItemKey = latest(model::resource::structure_definition::dispenseItem, reference);
    return dispenseItemKey.version ? DAV_PKV{std::move(*dispenseItemKey.version)} : DAV_PKV_1_3;
}

std::string kbvBundleXml(const KbvBundleOptions& bundleOptions)
{
    Expect3(holds_alternative<Versions::KBV_ERP>(bundleOptions.medicationOptions.version),
            "Medication in KBV-Bundle must have KBV-Version", std::logic_error);
    auto& resourceManager = ResourceManager::instance();
    std::string kvid10Ns{model::resource::naming_system::gkvKvid10};
    std::string templateFileName =
        "test/EndpointHandlerTest/kbv_bundle_template_" + to_string(bundleOptions.kbvVersion) + ".xml";
    auto bundle = resourceManager.getStringResource(templateFileName);
    const auto& prescriptionId = bundleOptions.prescriptionId;

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
            kvid10Ns = std::string{model::resource::naming_system::pkvKvid10};
            break;
    }
    boost::replace_all(bundle, "###PATIENT_IDENTIFIER_SYSTEM###", bundleOptions.patientIdentifierSystem);
    boost::replace_all(bundle, "###PATIENT_IDENTIFIER_CODE###", bundleOptions.forceInsuranceType.value_or(insuranceType));
    boost::replace_all(bundle, "###KVID10###", bundleOptions.forceKvid10Ns.value_or(kvid10Ns));
    boost::replace_all(bundle, "###AUTHORED_ON###", bundleOptions.authoredOn.toGermanDate());
    boost::replace_all(bundle, "###INSURANT_KVNR###", bundleOptions.kvnr);
    boost::replace_all(bundle, "###PRESCRIPTION_ID###", prescriptionId.toString());
    const auto coverageInsuranceType = bundleOptions.coverageInsuranceType.value_or(insuranceType);
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
    boost::replace_all(bundle, "###META_EXTENSION###", bundleOptions.metaExtension);
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
    boost::replace_all(bundle, "####MEDICATION_RESOURCE####", medicationXml(bundleOptions.medicationOptions));
    return bundle;
}

std::string kbvBundleMvoXml(const KbvBundleMvoOptions& bundleOptions)
{
    auto& resourceManager = ResourceManager::instance();
    const std::string kbvVersionStr{to_string(
        bundleOptions.kbvVersion.value_or(ResourceTemplates::Versions::KBV_ERP_current(bundleOptions.authoredOn)))};
    std::string templateFileName =
        "test/EndpointHandlerTest/kbv_bundle_mehrfachverordnung_template_" + kbvVersionStr + ".xml";

    auto bundle = resourceManager.getStringResource(templateFileName);
    boost::replace_all(bundle, "###TIMESTAMP###", bundleOptions.authoredOn.toXsDateTime());
    boost::replace_all(bundle, "###AUTHORED_ON###", bundleOptions.authoredOn.toGermanDate());
    boost::replace_all(bundle, "###PRESCRIPTION_ID###", bundleOptions.prescriptionId.toString());
    boost::replace_all(bundle, "###LEGAL_BASIS_CODE###", bundleOptions.legalBasisCode);
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
    return bundle;
}

std::string kbvBundlePkvXml(const KbvBundlePkvOptions& bundleOptions)
{
    auto& resourceManager = ResourceManager::instance();
    const std::string kbvVersionStr{to_string(bundleOptions.kbvVersion)};
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
    auto& resourceManager = ResourceManager::instance();
    const std::string gematikVersion{to_string(medicationDispenseOptions.gematikVersion)};
    std::string templateFileName = "test/EndpointHandlerTest/medication_dispense_template_" + gematikVersion + ".xml";
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
                    medicationOptions.version = Versions::GEM_ERP_1_4;
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
            return (std::ostringstream{} << medicationOptions.templatePrefix << version << ".xml").str();
        }
        std::string operator()(const Versions::GEM_ERP& version)
        {
            return (std::ostringstream{} << medicationOptions.templatePrefix << "gem_" << version << ".xml").str();
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
    boost::replace_all(medication, "###MEDICATION_CATEGORY###", medicationOptions.medicationCategory);
    boost::replace_all(medication, "###PZN###", medicationOptions.pzn.id());
    return medication;
}

std::string medicationDispenseDigaXml(const MedicationDispenseDiGAOptions& options)
{
    auto& resourceManager = ResourceManager::instance();
    std::string const templateFileName =
        "test/EndpointHandlerTest/GEM_ERP_PR_MedicationDispense_DiGA_" + to_string(options.version) + ".xml";
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
    const std::string gematikVersion{to_string(taskOptions.gematikVersion)};
    std::string templateFileName;
    switch (taskOptions.taskType)
    {
        case TaskType::Draft:
            templateFileName = "test/EndpointHandlerTest/task_draft_template_" + gematikVersion + ".json";
            break;
        case TaskType::Ready:
            templateFileName = "test/EndpointHandlerTest/task_ready_template_" + gematikVersion + ".json";
            break;
        case TaskType::InProgress:
            templateFileName = "test/EndpointHandlerTest/task_inprogress_template_" + gematikVersion + ".json";
            break;
        case TaskType::Completed:
            templateFileName = "test/EndpointHandlerTest/task_completed_template_" + gematikVersion + ".json";
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

    const auto prescriptionNs = taskOptions.prescriptionId.isPkv() ? model::resource::naming_system::pkvKvid10
                                                                   : model::resource::naming_system::gkvKvid10;
    boost::replace_all(task, "###KVNR_NAMING_SYSTEM###", std::string(prescriptionNs));

    return task;
}

std::string chargeItemXml(const ChargeItemOptions& chargeItemOptions, std::string path /* = "" */)
{
    auto& resourceManager = ResourceManager::instance();
    std::string templateFileName;
    switch (chargeItemOptions.operation)
    {
        case ChargeItemOptions::OperationType::Post:
            templateFileName = "test/EndpointHandlerTest/charge_item_POST_template.xml";
            break;
        case ChargeItemOptions::OperationType::Put:
            if (path.empty())
            {
                templateFileName = "test/EndpointHandlerTest/charge_item_PUT_template.xml";
            }
            else
            {
                templateFileName = "test/EndpointHandlerTest/" + path;
            }
            break;
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
std::string davDispenseItemXml(const DavDispenseItemOptions& dispenseItemOptions)
{
    auto& resourceManager = ResourceManager::instance();
    Expect3(dispenseItemOptions.davPkvVersion != fhirtools::FhirVersion::notVersioned,
            "dispense item must have a version", std::logic_error);
    std::string dispenseItem = resourceManager.getStringResource("test/EndpointHandlerTest/DAV_DispenseItem_template_" +
                                                                 to_string(dispenseItemOptions.davPkvVersion) + ".xml");

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
        std::string{model::profile(options.profileType).value()}.append(1, '|').append(to_string(options.version));
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
    auto& resourceManager = ResourceManager::instance();
    std::string templateFileName =
        "test/EndpointHandlerTest/EVDGA_Bundle_template_" + to_string(options.version) + ".xml";
    auto evdgaBundle = resourceManager.getStringResource(templateFileName);

    boost::replace_all(evdgaBundle, "##PRESCRIPTION_ID##", options.prescriptionId);
    boost::replace_all(evdgaBundle, "##TIMESTAMP##", options.timestamp);
    boost::replace_all(evdgaBundle, "##KVNR##", options.kvnr);
    boost::replace_all(evdgaBundle, "##COVERAGE_SYSTEM##", options.coverageSystem);
    boost::replace_all(evdgaBundle, "##COVERAGE_CODE##", options.coverageCode);
    boost::replace_all(evdgaBundle, "##AUTHORED_ON##", options.authoredOn);

    return evdgaBundle;
}
}// namespace ResourceTemplates

