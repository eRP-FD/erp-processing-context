/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "test/util/ResourceTemplates.hxx"
#include "erp/model/MedicationDispense.hxx"
#include "erp/model/MedicationDispenseBundle.hxx"
#include "erp/model/MedicationDispenseId.hxx"
#include "erp/model/ResourceNames.hxx"
#include "erp/model/Timestamp.hxx"
#include "test/util/ResourceManager.hxx"
#include "test/util/TestUtils.hxx"
#include "erp/util/Uuid.hxx"

#include <boost/algorithm/string.hpp>

namespace ResourceTemplates
{

std::string kbvBundleXml(const KbvBundleOptions& bundleOptions)
{
    auto& resourceManager = ResourceManager::instance();
    auto kbvVersion =
        bundleOptions.kbvVersion.value_or(model::ResourceVersion::current<model::ResourceVersion::KbvItaErp>());
    const std::string kbvVersionStr{v_str(kbvVersion)};
    bool deprecatedKbv = kbvVersion == model::ResourceVersion::KbvItaErp::v1_0_2;
    std::string kvid10Ns = deprecatedKbv ? std::string{model::resource::naming_system::deprecated::gkvKvid10}
                                         : std::string{model::resource::naming_system::gkvKvid10};
    std::string templateFileName = "test/EndpointHandlerTest/kbv_bundle_template_" + kbvVersionStr + ".xml";
    auto bundle = resourceManager.getStringResource(templateFileName);
    const auto& prescriptionId = bundleOptions.prescriptionId;

    std::string insuranceType;
    switch (prescriptionId.type())
    {
        case model::PrescriptionType::apothekenpflichigeArzneimittel:
        case model::PrescriptionType::direkteZuweisung:
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
    boost::replace_all(bundle, "###MEDICATION_CATEGORY###", bundleOptions.medicationCategory);
    const auto coverageInsuranceType = bundleOptions.coverageInsuranceType.value_or(insuranceType);
    boost::replace_all(bundle, "###COVERAGE_INSURANCE_TYPE###", coverageInsuranceType);
    boost::replace_all(bundle, "###COVERAGE_INSURANCE_SYSTEM###", bundleOptions.coverageInsuranceSystem);
    if (bundleOptions.iknr.id().empty())
    {
        boost::replace_all(bundle, "###PAYOR_IDENTIFIER###", "");
    }
    else
    {
        std::string argeIknrNs = deprecatedKbv ? std::string{model::resource::naming_system::deprecated::argeIknr}
                                            : std::string{model::resource::naming_system::argeIknr};
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
    boost::replace_all(bundle, "###LANR_SYSTEM###", bundleOptions.lanr.namingSystem(deprecatedKbv));
    boost::replace_all(bundle, "###LANR_CODE_SYSTEM###", anrCodeSystem);
    boost::replace_all(bundle, "###QUALIFICATION_TYPE###", qualificationType);
    boost::replace_all(bundle, "###PZN###", bundleOptions.pzn.id());
    return bundle;
}

std::string kbvBundleMvoXml(const KbvBundleMvoOptions& bundleOptions)
{
    auto& resourceManager = ResourceManager::instance();
    auto kbvVersion =
        bundleOptions.kbvVersion.value_or(model::ResourceVersion::current<model::ResourceVersion::KbvItaErp>());
    const std::string kbvVersionStr{v_str(kbvVersion)};
    bool deprecatedKbv = kbvVersion == model::ResourceVersion::KbvItaErp::v1_0_2;
    std::string kvid10Ns = deprecatedKbv ? std::string{model::resource::naming_system::deprecated::gkvKvid10}
                                         : std::string{model::resource::naming_system::gkvKvid10};
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
    return bundle;
}

std::string kbvBundlePkvXml(const KbvBundlePkvOptions& bundleOptions)
{
    auto& resourceManager = ResourceManager::instance();
    const std::string kbvVersionStr{v_str(bundleOptions.kbvVersion)};
    std::string templateFileName = "test/EndpointHandlerTest/kbv_pkv_bundle_template_" + kbvVersionStr + ".xml";
    auto bundle = resourceManager.getStringResource(templateFileName);
    boost::replace_all(bundle, "###PRESCRIPTION_ID###", bundleOptions.prescriptionId.toString());
    boost::replace_all(bundle, "###KVNR###", bundleOptions.kvnr.id());
    boost::replace_all(bundle, "###AUTHORED_ON###", bundleOptions.authoredOn.toGermanDate());
    return bundle;
}

std::string medicationDispenseBundleXml(const MedicationDispenseBundleOptions& bundleOptions)
{
    std::optional<std::string> profileStr;
    model::ResourceBase::Profile profile = model::ResourceBase::NoProfile;
    // there is no profile in the deprecated fhir bundle
    if (! deprecatedBundle(bundleOptions.bundleFhirBundleVersion))
    {
        profileStr.emplace(
            testutils::profile(SchemaType::MedicationDispenseBundle, bundleOptions.bundleFhirBundleVersion));
        profile = *profileStr;
    }
    model::MedicationDispenseBundle bundle{model::BundleType::collection, profile};
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


std::string medicationDispenseXml(const MedicationDispenseOptions& medicationDispenseOptions)
{
    namespace rv = model::ResourceVersion;
    auto& resourceManager = ResourceManager::instance();
    const std::string gematikVersion{v_str(medicationDispenseOptions.gematikVersion)};
    std::string templateFileName = "test/EndpointHandlerTest/medication_dispense_template_" + gematikVersion + ".xml";
    auto bundle = resourceManager.getStringResource(templateFileName);
    auto fhirBundleVersion = rv::fhirProfileBundleFromSchemaVersion(medicationDispenseOptions.gematikVersion);
    auto medicationOptions = medicationDispenseOptions.medication;
    medicationOptions.version =
            medicationOptions.version.value_or(get<rv::KbvItaErp>(rv::profileVersionFromBundle(fhirBundleVersion)));
    struct toString {
        std::string operator()(const model::PrescriptionId& prescriptionId) const
        {
            return prescriptionId.toString();
        }
        std::string operator()(const model::Timestamp& timestamp) const
        {
            return timestamp.toXsDateTime();
        }
        std::string operator()(const std::string& str)
        {
            return str;
        }
    };
    boost::replace_all(bundle, "###PRESCRIPTION_ID###", std::visit(toString{}, medicationDispenseOptions.prescriptionId));
    boost::replace_all(bundle, "###INSURANT_KVNR###", medicationDispenseOptions.kvnr);
    boost::replace_all(bundle, "###TELEMATIK_ID###", medicationDispenseOptions.telematikId);
    boost::replace_all(bundle, "###WHENHANDEDOVER###", std::visit(toString{}, medicationDispenseOptions.whenHandedOver));
    boost::replace_all(bundle, "###MEDICATION###", medicationXml(medicationOptions));
    std::string whenPrepared;
    if (medicationDispenseOptions.whenPrepared.has_value())
    {
        whenPrepared = "<whenPrepared value=\"" + medicationDispenseOptions.whenPrepared->toXsDateTime() + "\" />";
    }
    boost::replace_all(bundle, "###WHENPREPARED###", whenPrepared);
    return bundle;
}

std::string medicationXml(const MedicationOptions& medicationOptions)
{
    using namespace std::string_literals;
    auto& resourceManager = ResourceManager::instance();
    auto version =
        medicationOptions.version.value_or(model::ResourceVersion::current<model::ResourceVersion::KbvItaErp>());
    std::ostringstream templateFileName;
    templateFileName << medicationOptions.templatePrefix << v_str(version) << ".xml";
    auto medication = resourceManager.getStringResource(templateFileName.view());
    boost::replace_all(medication, "###MEDICATION_ID###", medicationOptions.id);
    return medication;
}

std::string taskJson(const TaskOptions& taskOptions)
{
    auto& resourceManager = ResourceManager::instance();
    const std::string gematikVersion{v_str(taskOptions.gematikVersion)};
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

    if(taskOptions.gematikVersion >= model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_2_0)
    {
        const auto prescriptionNs = taskOptions.prescriptionId.isPkv() ?
            model::resource::naming_system::pkvKvid10 :
            model::resource::naming_system::gkvKvid10;
        boost::replace_all(task, "###KVNR_NAMING_SYSTEM###", std::string(prescriptionNs));
    }

    return task;
}

std::string chargeItemXml(const ChargeItemOptions& chargeItemOptions)
{
    auto& resourceManager = ResourceManager::instance();
    std::string templateFileName;
    switch (chargeItemOptions.operation)
    {
        case ChargeItemOptions::OperationType::Post:
            templateFileName = "test/EndpointHandlerTest/charge_item_POST_template.xml";
            break;
        case ChargeItemOptions::OperationType::Put:
            templateFileName = "test/EndpointHandlerTest/charge_item_PUT_template.xml";
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

}// namespace ResourceTemplates
