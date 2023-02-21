/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#include "test/util/ResourceTemplates.hxx"
#include "erp/model/ResourceNames.hxx"
#include "erp/model/Timestamp.hxx"
#include "test/util/ResourceManager.hxx"

#include <boost/algorithm/string.hpp>

namespace ResourceTemplates
{

std::string kbvBundleXml(const KbvBundleOptions& bundleOptions)
{
    auto& resourceManager = ResourceManager::instance();
    const std::string kbvVersionStr{v_str(bundleOptions.kbvVersion)};
    bool deprecatedKbv = bundleOptions.kbvVersion == model::ResourceVersion::KbvItaErp::v1_0_2;
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
    boost::replace_all(bundle, "###INSURANCE_TYPE###", bundleOptions.forceInsuranceType.value_or(insuranceType));
    boost::replace_all(bundle, "###KVID10###", kvid10Ns);
    boost::replace_all(bundle, "###TIMESTAMP###", bundleOptions.timestamp.toXsDateTime());
    boost::replace_all(bundle, "###TIMESTAMP_DATE###", bundleOptions.timestamp.toXsDate());
    boost::replace_all(bundle, "###INSURANT_KVNR###", bundleOptions.kvnr);
    boost::replace_all(bundle, "###PRESCRIPTION_ID###", prescriptionId.toString());
    boost::replace_all(bundle, "###MEDICATION_CATEGORY###", bundleOptions.medicationCategory);
    const auto coverageInsuranceType = bundleOptions.coverageInsuranceType.value_or(insuranceType);
    boost::replace_all(bundle, "###COVERAGE_INSURANCE_TYPE###", coverageInsuranceType);
    boost::replace_all(bundle, "###COVERAGE_INSURANCE_SYSTEM###", bundleOptions.coverageInsuranceSystem);
    boost::replace_all(bundle, "###COVERAGE_PAYOR_EXTENSION###", bundleOptions.coveragePayorExtension);
    boost::replace_all(bundle, "###META_EXTENSION###", bundleOptions.metaExtension);
    return bundle;
}

std::string kbvBundleMvoXml(const KbvBundleMvoOptions& bundleOptions)
{
    auto& resourceManager = ResourceManager::instance();
    const std::string kbvVersionStr{v_str(bundleOptions.kbvVersion)};
    bool deprecatedKbv = bundleOptions.kbvVersion == model::ResourceVersion::KbvItaErp::v1_0_2;
    std::string kvid10Ns = deprecatedKbv ? std::string{model::resource::naming_system::deprecated::gkvKvid10}
                                         : std::string{model::resource::naming_system::gkvKvid10};
    std::string templateFileName = "test/EndpointHandlerTest/kbv_bundle_mehrfachverordnung_template_" + kbvVersionStr + ".xml";

    auto bundle = resourceManager.getStringResource(templateFileName);
    boost::replace_all(bundle, "###TIMESTAMP###", bundleOptions.timestamp.toXsDateTime());
    boost::replace_all(bundle, "###TIMESTAMP_DATE###", bundleOptions.timestamp.toXsDate());
    boost::replace_all(bundle, "###PRESCRIPTION_ID###", bundleOptions.prescriptionId.toString());
    boost::replace_all(bundle, "###LEGAL_BASIS_CODE###", bundleOptions.legalBasisCode);
    boost::replace_all(bundle, "###NUMERATOR###", std::to_string(bundleOptions.numerator));
    boost::replace_all(bundle, "###DENOMINATOR###", std::to_string(bundleOptions.denominator));
    std::string redeemPeriodStart;
    if (bundleOptions.redeemPeriodStart.has_value())
    {
        redeemPeriodStart = "<start value=\"" + bundleOptions.redeemPeriodStart->toXsDate() + "\" />";
    }
    boost::replace_all(bundle, "###REDEEM_START###", redeemPeriodStart);

    std::string redeemPeriodEnd;
    if (bundleOptions.redeemPeriodEnd.has_value())
    {
        redeemPeriodEnd = "<end value=\"" + bundleOptions.redeemPeriodEnd->toXsDate() + "\" />";
    }
    boost::replace_all(bundle, "###REDEEM_END###", redeemPeriodEnd);
    return bundle;
}

std::string medicationDispenseBundleXml(const MedicationDispenseBundleOptions& bundleOptions)
{
    auto& resourceManager = ResourceManager::instance();
    const std::string gematikVersion{v_str(bundleOptions.gematikVersion)};
    std::string templateFileName = "test/EndpointHandlerTest/medication_dispense_bundle_template_" + gematikVersion + ".xml";
    auto bundle = resourceManager.getStringResource(templateFileName);

    boost::replace_all(bundle, "###PRESCRIPTION_ID###", bundleOptions.prescriptionId.toString());
    boost::replace_all(bundle, "###INSURANT_KVNR###", bundleOptions.kvnr);
    boost::replace_all(bundle, "###TELEMATIK_ID###", bundleOptions.telematikId);
    boost::replace_all(bundle, "###WHENHANDEDOVER###", bundleOptions.whenHandedOver.toXsDateTime());
    std::string whenPrepared;
    if (bundleOptions.whenPrepared.has_value())
    {
        whenPrepared = "<whenPrepared value=\"" + bundleOptions.whenPrepared->toXsDateTime() + "\" />";
    }
    boost::replace_all(bundle, "###WHENPREPARED###", whenPrepared);
    return bundle;
}


std::string medicationDispenseXml(const MedicationDispenseOptions& medicationDispenseOptions)
{
    auto& resourceManager = ResourceManager::instance();
    const std::string gematikVersion{v_str(medicationDispenseOptions.gematikVersion)};
    std::string templateFileName = "test/EndpointHandlerTest/medication_dispense_template_" + gematikVersion + ".xml";
    auto bundle = resourceManager.getStringResource(templateFileName);

    boost::replace_all(bundle, "###PRESCRIPTION_ID###", medicationDispenseOptions.prescriptionId.toString());
    boost::replace_all(bundle, "###INSURANT_KVNR###", medicationDispenseOptions.kvnr);
    boost::replace_all(bundle, "###TELEMATIK_ID###", medicationDispenseOptions.telematikId);
    boost::replace_all(bundle, "###WHENHANDEDOVER###", medicationDispenseOptions.whenHandedOver.toXsDateTime());
    std::string whenPrepared;
    if (medicationDispenseOptions.whenPrepared.has_value())
    {
        whenPrepared = "<whenPrepared value=\"" + medicationDispenseOptions.whenPrepared->toXsDateTime() + "\" />";
    }
    boost::replace_all(bundle, "###WHENPREPARED###", whenPrepared);
    return bundle;
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

    if(taskOptions.gematikVersion >= model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_2_0)
    {
        const auto prescriptionNs = taskOptions.prescriptionId.isPkv() ?
            model::resource::naming_system::pkvKvid10 :
            model::resource::naming_system::gkvKvid10;
        boost::replace_all(task, "###KVNR_NAMING_SYSTEM###", std::string(prescriptionNs));
    }

    return task;
}

}// namespace ResourceTemplates
