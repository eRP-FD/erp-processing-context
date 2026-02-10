// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "exporter/TRezeptTransformer.hxx"
#include "erp/model/MedicationsAndDispenses.hxx"
#include "erp/model/Task.hxx"
#include "exporter/model/OrganizationDirectory.hxx"
#include "shared/fhir/FhirCanonicalizer.hxx"
#include "shared/model/KbvBundle.hxx"
#include "shared/model/KbvMedicationBase.hxx"
#include "shared/model/MedicationDispenseOperationParameters.hxx"
#include "test/exporter/Epa4AllTransformerTest.hxx"
#include "test/util/ResourceManager.hxx"
#include "test/util/ResourceTemplates.hxx"

#include <fstream>

class TRezeptTransformerTest : public Epa4AllTransformerTest
{
public:
    void checkCarbonCopy(model::Bundle& kbvBundle, const model::Bundle& medicationDispenseBundle,
                         const model::Bundle& vzdSearchset, const model::Timestamp& signingTime,
                         const model::ErpTPrescriptionCarbonCopy& carbonCopy);
    void checkTRezeptMedication(const rapidjson::Value& source, const rapidjson::Value& tRezeptMedication);
    void checkTRezeptMedicationDispense(const rapidjson::Value& source,
                                        const rapidjson::Value& tRezeptMedicationDispense);
    void checkTRezeptOrganization(const model::Bundle& vzdSearchSet, const rapidjson::Value& tRezeptOrganization);

    testutils::ShiftFhirResourceViewsGuard shift{"erp.t-prescription-1.1.0-ballot3",
                                                 date::floor<date::days>(model::Timestamp::now().toChronoTimePoint())};
};

void TRezeptTransformerTest::checkCarbonCopy(model::Bundle& kbvBundle, const model::Bundle& medicationDispenseBundle,
                                             const model::Bundle& vzdSearchset, const model::Timestamp& signingTime,
                                             const model::ErpTPrescriptionCarbonCopy& carbonCopy)
{
    std::cout << carbonCopy.serializeToJsonString() << std::endl;
    validate(
        fhirtools::DefinitionKey{
            "https://gematik.de/fhir/erp-t-prescription/StructureDefinition/erp-tprescription-carbon-copy|1.1"},
        &carbonCopy.jsonDocument());

    EXPECT_EQ(signingTime, carbonCopy.prescriptionSignatureDate());

    auto kbvMedication = kbvBundle.getUniqueResourceByType<model::KbvMedicationGeneric>();
    const auto& rxPrescriptionMedication = carbonCopy.rxPrescriptionMedication();
    checkTRezeptMedication(kbvMedication.jsonDocument(), rxPrescriptionMedication);

    auto dispenseMedications = medicationDispenseBundle.getResourcesByType<model::GemErpPrMedication>();
    auto dispenses = medicationDispenseBundle.getResourcesByType<model::MedicationDispense>();
    ASSERT_EQ(dispenseMedications.size(), dispenses.size());
    for (size_t i = 0; const auto& dispenseMedication : dispenseMedications)
    {
        const auto& dispense = dispenses[i];
        const auto& [rxMedicationDispense, rxDispensationMedication] = carbonCopy.rxDispensationInformation(i);
        ++i;
        checkTRezeptMedication(dispenseMedication.jsonDocument(), *rxDispensationMedication);

        checkTRezeptMedicationDispense(dispense.jsonDocument(), *rxMedicationDispense);
    }

    const auto& tRezeptOrganization = carbonCopy.organization();
    checkTRezeptOrganization(vzdSearchset, tRezeptOrganization);
}

void TRezeptTransformerTest::checkTRezeptMedication(const rapidjson::Value& source,
                                                    const rapidjson::Value& tRezeptMedication)
{
    const std::vector<std::string> propertiesRetained = {
        "Medication.code.coding.system",
        "Medication.code.coding.code",
        "Medication.code.text",
        "Medication.form.coding.system",
        "Medication.form.coding.code",
        "Medication.form.coding.text",
        "Medication.amount.denominator.value",
        "Medication.amount.denominator.comperator",
        "Medication.amount.denominator.unit",
        "Medication.amount.denominator.system",
        "Medication.amount.denominator.code",
        "Medication.amount.numerator.value",
        "Medication.amount.numerator.comperator",
        "Medication.amount.numerator.unit",
        "Medication.amount.numerator.system",
        "Medication.amount.numerator.code",
        "Medication.ingredient.extension.0.url",
        "Medication.ingredient.extension.0.valueString",
        "Medication.ingredient.itemCodeableConcept.coding.0.system",
        "Medication.ingredient.itemCodeableConcept.coding.0.code",
        "Medication.ingredient.itemCodeableConcept.coding.0.display",
        "Medication.ingredient.itemReference.reference",
        "Medication.ingredient.itemReference.type",
        "Medication.ingredient.itemReference.identifier",
        "Medication.ingredient.itemReference.display",
        "Medication.ingredient.isActive",
    };
    const std::vector<std::string> propertiesNotInTarget{
        "Medication.meta",       "Medication.implicitRules", "Medication.language",     "Medication.text",
        "Medication.identifier", "Medication.status",        "Medication.manufacturer", "Medication.batch",
    };
    std::vector<ExtensionMapping> mappedExtensions{
        {"http://fhir.de/StructureDefinition/normgroesse", "http://fhir.de/StructureDefinition/normgroesse",
         std::vector<MappedValue>{}, std::vector<std::string>{"Extension.valueCode"}, std::vector<std::string>{}},
        {"https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Medication_Packaging",
         "https://gematik.de/fhir/epa-medication/StructureDefinition/medication-formulation-packaging-extension",
         std::vector<MappedValue>{}, std::vector<std::string>{"Extension.valueString"}, std::vector<std::string>{}}};

    checkRetainedProperties(source, tRezeptMedication, propertiesRetained);
    checkPropertiesNotInTarget(tRezeptMedication, propertiesNotInTarget);
    checkExtensions(source, tRezeptMedication, mappedExtensions);
}

void TRezeptTransformerTest::checkTRezeptMedicationDispense(const rapidjson::Value& source,
                                                            const rapidjson::Value& tRezeptMedicationDispense)
{
    const std::vector<std::string> propertiesRetained{
        "MedicationDispense.status",
        "MedicationDispense.medicationReference.reference",
        "MedicationDispense.performer.fuction",
        "MedicationDispense.performer.actor.reference",
        "MedicationDispense.performer.actor.type",
        "MedicationDispense.performer.actor.identifier",
        "MedicationDispense.performer.actor.display",
        "MedicationDispense.quantity.value",
        "MedicationDispense.quantity.unit",
        "MedicationDispense.quantity.system",
        "MedicationDispense.quantity.code",
        "MedicationDispense.dosageInstruction.timing.repeat.boundsDuration.value",
        "MedicationDispense.dosageInstruction.timing.repeat.boundsDuration.unit",
        "MedicationDispense.dosageInstruction.timing.repeat.boundsDuration.system",
        "MedicationDispense.dosageInstruction.timing.repeat.boundsDuration.code",
        "MedicationDispense.dosageInstruction.text",
        "MedicationDispense.dosageInstruction.timing.repeat.frequency",
        "MedicationDispense.dosageInstruction.timing.repeat.period",
        "MedicationDispense.dosageInstruction.timing.repeat.periodUnit",
        "MedicationDispense.when",
    };
    const std::vector<std::string> propertiesNotInTarget{
        "MedicationDispense.identifier",
        "MedicationDispense.partOf",
        "MedicationDispense.statusReason",
        "MedicationDispense.category",
        "MedicationDispense.subject",
        "MedicationDispense.context",
        "MedicationDispense.supportingInformation",
        "MedicationDispense.location",
        "MedicationDispense.authorizingPrescription",
        "MedicationDispense.type",
        "MedicationDispense.daysSupply",
        "MedicationDispense.whenPrepared",
        "MedicationDispense.destination",
        "MedicationDispense.receiver",
        "MedicationDispense.note",
        "MedicationDispense.substitution",
        "MedicationDispense.detectedIssue",
        "MedicationDispense.eventHistory",
    };

    checkRetainedProperties(source, tRezeptMedicationDispense, propertiesRetained);
    checkPropertiesNotInTarget(tRezeptMedicationDispense, propertiesNotInTarget);

    validate(
        fhirtools::DefinitionKey{
            "https://gematik.de/fhir/erp-t-prescription/StructureDefinition/erp-tprescription-medication-dispense|1.1"},
        &tRezeptMedicationDispense);
}

void TRezeptTransformerTest::checkTRezeptOrganization(const model::Bundle& vzdSearchSet,
                                                      const rapidjson::Value& tRezeptOrganization)
{
    const std::vector<std::string> propertiesNotInTarget{
        "Organization.active", "Organization.type",    "Organization.alias",
        "Organization.partOf", "Organization.contact", "Organization.endpoint",
    };
    checkPropertiesNotInTarget(tRezeptOrganization, propertiesNotInTarget);

    const auto& organizationDirectory = vzdSearchSet.getUniqueResourceByType<model::OrganizationDirectory>();
    const std::vector<std::string> fromOrganizationDirectory{"Organization.name"};
    checkRetainedProperties(organizationDirectory.jsonDocument(), tRezeptOrganization, fromOrganizationDirectory);
    rapidjson::Pointer identifiersPointer("/identifier");
    const auto* telematikIdItem = model::NumberAsStringParserDocument::findMemberInArray(
        identifiersPointer.Get(tRezeptOrganization), rapidjson::Pointer("/system"),
        "https://gematik.de/fhir/sid/telematik-id");
    ASSERT_TRUE(telematikIdItem);
    EXPECT_EQ(serialize(organizationDirectory.getTelematikIdIdentifier()), serialize(*telematikIdItem));

    const auto& healthcareService =
        vzdSearchSet.getUniqueResourceByType<model::UnspecifiedResource>("HealthcareService");
    const std::vector<std::string> fromHealthcareService{
        "Organization.telecom.system", "Organization.telecom.value",        "Organization.telecom.use",
        "Organization.telecom.rank",   "Organization.telecom.period.start", "Organization.telecom.period.end",
    };
    checkRetainedProperties(healthcareService.jsonDocument(), tRezeptOrganization, fromHealthcareService);

    const auto& optionalLocationDirectory =
        vzdSearchSet.getResourcesByType<model::UnspecifiedResource>("LocationDirectory");
    if (! optionalLocationDirectory.empty())
    {
        const std::vector<std::string> fromLocationDirectory{
            "Organization.address.use",          "Organization.address.type",       "Organization.address.text",
            "Organization.address.line",         "Organization.address.city",       "Organization.address.district",
            "Organization.address.state",        "Organization.address.postalCode", "Organization.address.country",
            "Organization.address.period.start", "Organization.address.period.end",
        };
        checkRetainedProperties(optionalLocationDirectory[0].jsonDocument(), tRezeptOrganization,
                                fromLocationDirectory);
    }
}

TEST_F(TRezeptTransformerTest, transform)
{
    const ResourceTemplates::KbvBundleOptions options{
        .prescriptionId = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::tRezept, 123456789)};
    auto kbvBundle = model::Bundle::fromXmlNoValidation(ResourceTemplates::kbvBundleXml(options));
    auto medicationDispenseBundle = model::Bundle::fromJsonNoValidation(
        ResourceTemplates::internal_type::medicationDispenseBundle().serializeToJsonString());
    auto vzdSearchset = model::Bundle::fromJsonNoValidation(ResourceTemplates::vzdSearchSetJson());

    std::optional<model::ErpTPrescriptionCarbonCopy> result;
    auto signingTime = model::Timestamp::now();
    ASSERT_NO_THROW(result = TRezeptTransformer::transform(kbvBundle, medicationDispenseBundle, signingTime,
                                                           options.prescriptionId, vzdSearchset));
    ASSERT_TRUE(result);
    checkCarbonCopy(kbvBundle, medicationDispenseBundle, vzdSearchset, signingTime, *result);
}

TEST_F(TRezeptTransformerTest, missingOrganization_logError)
{
    const ResourceTemplates::KbvBundleOptions options{
        .prescriptionId = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::tRezept, 123456789)};
    auto kbvBundle = model::Bundle::fromXmlNoValidation(ResourceTemplates::kbvBundleXml(options));
    auto medicationDispenseBundle = model::Bundle::fromJsonNoValidation(
        ResourceTemplates::internal_type::medicationDispenseBundle().serializeToJsonString());

    auto vzdSearchset = model::Bundle::fromJsonNoValidation(ResourceTemplates::vzdSearchSetJson("test/EndpointHandlerTest/ERP_TPrescription_VZD_SearchSet_missing_organization_template_1.0.json", {}));

    std::optional<model::ErpTPrescriptionCarbonCopy> result;
    auto signingTime = model::Timestamp::now();

    std::string logStr = "";
    bool failed = false;
    try
    {
        TRezeptTransformer::transform(kbvBundle, medicationDispenseBundle, signingTime,
                                      options.prescriptionId, vzdSearchset);
    }
    catch (const std::exception& exc)
    {
        const auto loc = ExceptionWrapperBase::getLocation(exc);
        logStr = "unknown location";
        if (loc && loc->location.fileName)
        {
            logStr = std::string{loc->location.fileName} + ":" + std::to_string(loc->location.line);
        }
        failed = true;
    }
    EXPECT_TRUE(logStr.find("/src/exporter/TRezeptTransformer.cxx:") != std::string::npos);
    EXPECT_TRUE(failed);
}

TEST_F(TRezeptTransformerTest, missingLocationAndHealthcareService_noError)
{
    const ResourceTemplates::KbvBundleOptions options{
        .prescriptionId = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::tRezept, 123456789)};
    auto kbvBundle = model::Bundle::fromXmlNoValidation(ResourceTemplates::kbvBundleXml(options));
    auto medicationDispenseBundle = model::Bundle::fromJsonNoValidation(
        ResourceTemplates::internal_type::medicationDispenseBundle().serializeToJsonString());
    auto vzdSearchset = model::Bundle::fromJsonNoValidation(ResourceTemplates::vzdSearchSetJson("test/EndpointHandlerTest/ERP_TPrescription_VZD_SearchSet_missing_healthcareservice_and_location_template_1.0.json", {}));

    std::optional<model::ErpTPrescriptionCarbonCopy> result;
    auto signingTime = model::Timestamp::now();
    ASSERT_NO_THROW(result = TRezeptTransformer::transform(kbvBundle, medicationDispenseBundle, signingTime,
                                                           options.prescriptionId, vzdSearchset));
    ASSERT_TRUE(result);
}

struct TestCaseParam {
    std::string sampleName;
    std::string kbvBundle;
    std::string medicationDispenseParameter;
    std::string task;
    std::string vzdSearchSet;
    std::string expectedCarbonCopy;
};
std::string makeFileName(const std::string& sampleName, const std::string& part)
{
    const std::string prefix = "test/fhir/transformer/TRezept/";
    return prefix + sampleName + "/example-case-" + part;
}
TestCaseParam makeParam(const std::string& sampleName, const std::string& testCaseNumber)
{
    return {
        .sampleName = sampleName,
        .kbvBundle = makeFileName(sampleName, testCaseNumber + "-KBV_Bundle.xml"),
        .medicationDispenseParameter = makeFileName(sampleName, testCaseNumber + "-MedicationDispense_Parameters.xml"),
        .task = makeFileName(sampleName, testCaseNumber + "-Task.xml"),
        .vzdSearchSet = makeFileName(sampleName, testCaseNumber + "-VZDSearchSet.xml"),
        .expectedCarbonCopy = makeFileName(sampleName, testCaseNumber + "-digitaler-durchschlag.json"),
    };
}
class TRezeptSampleTestP : public TRezeptTransformerTest, public ::testing::WithParamInterface<TestCaseParam>
{
public:
    static std::string name(const ::testing::TestParamInfo<ParamType>& info)
    {
        return String::replaceAll(info.param.sampleName, " ", "_");
    }
};

TEST_P(TRezeptSampleTestP, Test)
{
    auto kbvBundle =
        model::Bundle::fromXmlNoValidation(ResourceManager::instance().getStringResource(GetParam().kbvBundle));
    model::MedicationsAndDispenses medicationAndDispenses;
    medicationAndDispenses.addFromParameters(model::MedicationDispenseOperationParameters::fromXmlNoValidation(
        ResourceManager::instance().getStringResource(GetParam().medicationDispenseParameter)));
    std::map<int64_t, size_t> prescriptionIdIdx;
    for (auto& medicationDispense : medicationAndDispenses.medicationDispenses)
    {
        const auto prescriptionId = medicationDispense.prescriptionId();
        medicationDispense.setId(
            model::MedicationDispenseId{prescriptionId, prescriptionIdIdx[prescriptionId.toDatabaseId()]++});
    }
    model::MedicationDispenseBundle medicationDispenseBundle_{"", medicationAndDispenses.medicationDispenses,
                                                              medicationAndDispenses.medications};
    model::Bundle medicationDispenseBundle{std::move(medicationDispenseBundle_).jsonDocument()};
    auto task = model::Task::fromXmlNoValidation(ResourceManager::instance().getStringResource(GetParam().task));
    auto vzdSearchset =
        model::Bundle::fromXmlNoValidation(ResourceManager::instance().getStringResource(GetParam().vzdSearchSet));

    std::optional<model::ErpTPrescriptionCarbonCopy> result;
    const auto signingTime = model::Timestamp::now();
    ASSERT_NO_THROW(result = TRezeptTransformer::transform(kbvBundle, medicationDispenseBundle, signingTime,
                                                           task.prescriptionId(), vzdSearchset));
    ASSERT_TRUE(result);
    checkCarbonCopy(kbvBundle, medicationDispenseBundle, vzdSearchset, signingTime, *result);

    // {
    //     std::filesystem::path outfilenameXml = String::replaceAll(
    //         ResourceManager::getAbsoluteFilename(GetParam().expectedCarbonCopy), ".json", "_ibm.xml");
    //     std::cout << "writing " << outfilenameXml << std::endl;
    //     auto outpath = outfilenameXml;
    //     std::filesystem::create_directories(outpath.remove_filename());
    //     std::ofstream ofs(outfilenameXml, std::ios::binary);
    //     ofs << result->serializeToXmlString();
    //     std::filesystem::path outfilenameJson = outfilenameXml.replace_extension("json");
    //     std::ofstream ofsJson(outfilenameJson, std::ios::binary);
    //     ofsJson << result->serializeToJsonString();
    // }

    // auto expectedCarbonCopy = model::ErpTPrescriptionCarbonCopy::fromJsonNoValidation(
    //     ResourceManager::instance().getStringResource(GetParam().expectedCarbonCopy));
    // EXPECT_EQ(result->serializeToXmlString(), expectedCarbonCopy.serializeToXmlString());
}

INSTANTIATE_TEST_SUITE_P(x, TRezeptSampleTestP,
                         testing::Values(makeParam("Beispiel 1 PZN Verordnung", "01"),
                                         makeParam("Beispiel 2 Wirkstoff Verordnung", "02"),
                                         makeParam("Beispiel 3 Freitext Verordnung", "03"),
                                         makeParam("Beispiel 4 PZN Verordnung mit absoluter Refernzierung", "04")),
                         &TRezeptSampleTestP::name);
