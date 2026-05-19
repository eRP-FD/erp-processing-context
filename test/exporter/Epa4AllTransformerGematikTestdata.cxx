/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/KbvPractitioner.hxx"
#include "exporter/Epa4AllTransformer.hxx"
#include "exporter/ExporterRequirements.hxx"
#include "shared/model/KbvBundle.hxx"
#include "shared/model/KbvMedicationBase.hxx"
#include "shared/model/KbvMedicationRequest.hxx"
#include "shared/model/KbvOrganization.hxx"
#include "shared/model/MedicationDispense.hxx"
#include "shared/model/MedicationDispenseId.hxx"
#include "shared/model/Patient.hxx"
#include "test/exporter/Epa4AllTransformerTest.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"
#include "test/util/ResourceManager.hxx"
#include "test/util/TestUtils.hxx"

#include <fstream>

/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

namespace
{
enum class Type
{
    medicationCompounding,
    medicationFreeText,
    medicationIngredient,
    medicationPzn,
    medicationDispense,
    autoDetect
};
struct Params {
    std::filesystem::path sampleDir;
    std::string sampleName;
    std::filesystem::path inputFile() const
    {
        return sampleDir / inputFileName;
    }
    std::filesystem::path outputFile() const
    {
        return sampleDir / "output.xml";
    }
    Type type;
    std::string inputFileName = "input.xml";
    std::optional<std::string> shiftView{};
    friend std::ostream& operator<<(std::ostream& os, const Params& params)
    {
        os << params.sampleName;
        return os;
    }
};
}

class Epa4AllTransformerGematikTestdataPrescription : public Epa4AllTransformerTest,
                                                      public testing::WithParamInterface<Params>
{
public:
    Type detectMedicationType(const rapidjson::Value& medicationResource)
    {
        const rapidjson::Pointer profilePtr{"/meta/profile/0"};
        const auto* profile = profilePtr.Get(medicationResource);
        if (! profile)
        {
            return Type::autoDetect;
        }
        if (std::string_view{profile->GetString()}.find("KBV_PR_ERP_Medication_Compounding") != std::string_view::npos)
        {
            return Type::medicationCompounding;
        }
        if (std::string_view{profile->GetString()}.find("KBV_PR_ERP_Medication_Ingredient") != std::string_view::npos)
        {
            return Type::medicationIngredient;
        }
        if (std::string_view{profile->GetString()}.find("KBV_PR_ERP_Medication_FreeText") != std::string_view::npos)
        {
            return Type::medicationFreeText;
        }
        if (std::string_view{profile->GetString()}.find("KBV_PR_ERP_Medication_PZN") != std::string_view::npos)
        {
            return Type::medicationPzn;
        }
        return Type::autoDetect;
    }
};

TEST_P(Epa4AllTransformerGematikTestdataPrescription, Test)
{
    std::optional<testutils::ShiftFhirResourceViewsGuard> guard =
        GetParam().shiftView.has_value()
            ? std::make_optional<testutils::ShiftFhirResourceViewsGuard>(
                  GetParam().shiftView.value_or(""), date::floor<date::days>(std::chrono::system_clock::now()))
            : std::nullopt;
    const auto inputResourceXmlString = ResourceManager::instance().getStringResource(GetParam().inputFile().string());

    const auto kbvInputBundle = model::Bundle::fromXmlNoValidation(inputResourceXmlString);
    auto kbvMedication = kbvInputBundle.getUniqueResourceByType<model::KbvMedicationGeneric>();

    std::optional<model::EPAOpProvidePrescriptionERPInputParameters> params;
    ASSERT_NO_THROW(params = Epa4AllTransformer::transformPrescription(
                        kbvInputBundle, telematikIdFromQes, telematikIdFromAccessToken, organizationNameFromJwt,
                        std::string{profession_oid::oid_oeffentliche_apotheke}));
    ASSERT_TRUE(params);
    std::cout << serialize(params->jsonDocument()) << std::endl;


    //    {
    //        std::filesystem::path outfilenameXml = "/tmp/" + String::replaceAll(
    //            GetParam().outputFile(), ".xml", "_ibm.xml");
    //        std::cout << "writing " << outfilenameXml << std::endl;
    //        auto outpath = outfilenameXml;
    //        std::filesystem::create_directories(outpath.remove_filename());
    //        std::ofstream ofs(outfilenameXml, std::ios::binary);
    //        ofs << params->serializeToXmlString();
    //        std::filesystem::path outfilenameJson = outfilenameXml.replace_extension("json");
    //        std::ofstream ofsJson(outfilenameJson, std::ios::binary);
    //        ofsJson << params->serializeToJsonString();
    //    }

    checkMedicationRequest(kbvInputBundle.getUniqueResourceByType<model::KbvMedicationRequest>().jsonDocument(),
                           *params->getMedicationRequest(),
                           kbvInputBundle.getUniqueResourceByType<model::Patient>().kvnr());

    const auto* medication = params->getMedication();
    ASSERT_TRUE(medication);

    auto type = GetParam().type;
    if (type == Type::autoDetect)
    {
        type = detectMedicationType(kbvMedication.jsonDocument());
    }

    switch (type)
    {
        case Type::medicationCompounding:
            checkMedicationCompounding(kbvMedication.jsonDocument(), *medication);
            break;
        case Type::medicationFreeText:
            checkMedicationFreeText(kbvMedication.jsonDocument(), *medication);
            break;
        case Type::medicationIngredient:
            checkMedicationIngredient(kbvMedication.jsonDocument(), *medication);
            break;
        case Type::medicationPzn:
            checkMedicationPzn(kbvMedication.jsonDocument(), *medication);
            break;
        case Type::medicationDispense:
            break;
        case Type::autoDetect:
            FAIL() << "not detected";
            break;
    }
    checkOrganization(kbvInputBundle.getUniqueResourceByType<model::KbvOrganization>().jsonDocument(),
                      *params->getOrganization());
    checkPractitioner(kbvInputBundle.getResourcesByType<model::KbvPractitioner>()[0].jsonDocument(),
                      *params->getPractitioner());

    validate(fhirtools::DefinitionKey{"https://gematik.de/fhir/epa-medication/StructureDefinition/"
                                      "epa-op-provide-prescription-erp-input-parameters"},
             &params->jsonDocument());
}

namespace
{
std::vector<Params> makeParams(std::string_view baseDir, Type type)
{
    static const auto absRoot =
        ResourceManager::getAbsoluteFilename("test/fhir/transformer/ExampleRepoTestData/valid/");

    std::vector<Params> params;

    auto absBase = absRoot / baseDir;
    std::filesystem::directory_iterator dirIt{absBase};
    for (const auto& dir : dirIt)
    {
        auto rel = std::filesystem::relative(dir, absBase);
        params.emplace_back(Params{.sampleDir = dir, .sampleName = rel.string(), .type = type});
    }
    return params;
}
std::vector<Params> makeKbv14Params(const std::filesystem::path& path)
{
    std::vector<Params> params;
    std::filesystem::directory_iterator dirIt{path};
    for (const auto& file : dirIt)
    {
        if (std::filesystem::is_regular_file(file))
        {
            params.emplace_back(Params{.sampleDir = file.path().parent_path(),
                                       .sampleName = file.path().filename().replace_extension("").string(),
                                       .type = Type::autoDetect,
                                       .inputFileName = file.path().filename().string(),
                                       .shiftView = "epa.medication-1.3.2"});
        }
    }
    return params;
}
}

INSTANTIATE_TEST_SUITE_P(medicationCompounding, Epa4AllTransformerGematikTestdataPrescription,
                         testing::ValuesIn(makeParams("medicationCompounding", Type::medicationCompounding)));

INSTANTIATE_TEST_SUITE_P(medicationFreeText, Epa4AllTransformerGematikTestdataPrescription,
                         testing::ValuesIn(makeParams("medicationFreeText", Type::medicationFreeText)));

INSTANTIATE_TEST_SUITE_P(medicationIngredient, Epa4AllTransformerGematikTestdataPrescription,
                         testing::ValuesIn(makeParams("medicationIngredient", Type::medicationIngredient)));

INSTANTIATE_TEST_SUITE_P(medicationPzn, Epa4AllTransformerGematikTestdataPrescription,
                         testing::ValuesIn(makeParams("medicationPzn", Type::medicationPzn)));

INSTANTIATE_TEST_SUITE_P(
    kbvItaErp1_4, Epa4AllTransformerGematikTestdataPrescription,
    testing::ValuesIn(makeKbv14Params(ResourceManager::getAbsoluteFilename("test/fhir/examples/kbv.ita.erp-1.4.0"))));


class Epa4AllTransformerGematikTestdataMedicationDispense : public Epa4AllTransformerTest,
                                                            public testing::WithParamInterface<Params>
{
};
TEST_P(Epa4AllTransformerGematikTestdataMedicationDispense, Test)
{
    const auto inputResourceXmlString = ResourceManager::instance().getStringResource(GetParam().inputFile().string());

    const auto medicationDispense = model::MedicationDispense::fromXmlNoValidation(inputResourceXmlString);
    model::Bundle medicationDispenseInputBundle;
    medicationDispenseInputBundle.addResource(std::nullopt, std::nullopt, std::nullopt,
                                              medicationDispense.jsonDocument());

    auto prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 123);
    auto authoredOn = model::Timestamp::now();

    std::optional<model::EPAOpProvideDispensationERPInputParameters> params;
    ASSERT_NO_THROW(params = Epa4AllTransformer::transformMedicationDispenseWithKBVMedication(
                        medicationDispenseInputBundle, prescriptionId, authoredOn,
                        model::TelematikId{telematikIdFromQes}, "Apotheke",
                        std::string{profession_oid::oid_oeffentliche_apotheke}));
    ASSERT_TRUE(params);
    std::cout << serialize(params->jsonDocument()) << std::endl;

    //    {
    //        std::filesystem::path outfilenameXml = "/tmp/" + String::replaceAll(GetParam().outputFile(), ".xml", "_ibm.xml");
    //        std::cout << "writing " << outfilenameXml << std::endl;
    //        auto outpath = outfilenameXml;
    //        std::filesystem::create_directories(outpath.remove_filename());
    //        std::ofstream ofs(outfilenameXml, std::ios::binary);
    //        ofs << params->serializeToXmlString();
    //        std::filesystem::path outfilenameJson = outfilenameXml.replace_extension("json");
    //        std::ofstream ofsJson(outfilenameJson, std::ios::binary);
    //        ofsJson << params->serializeToJsonString();
    //    }


    F_014.test("Eine Unterscheidung nach GKV und PKV im System findet beim Mapping nicht mehr statt.");
    auto dispenses = params->getMedicationDispenses();
    ASSERT_EQ(dispenses.size(), 1);
    std::cout << serialize(*dispenses[0]) << std::endl;
    checkMappedValues(*dispenses[0], {MappedValue{.property = "MedicationDispense.subject.identifier.system",
                                                  .targetValue = "http://fhir.de/sid/gkv/kvid-10"}});

    checkMappedValues(
        *params->getOrganization(),
        {MappedValue{.property = "Organization.type.0.coding.0.system",
                     .targetValue = "https://gematik.de/fhir/directory/CodeSystem/OrganizationProfessionOID"},
         MappedValue{.property = "Organization.type.0.coding.0.code",
                     .targetValue = std::string{profession_oid::oid_oeffentliche_apotheke}}});

    checkPropertiesNotInTarget(*dispenses[0], {"MedicationDispense.dosageInstruction.0.patientInstruction"});
    const rapidjson::Pointer patientInstructionPtr{"/dosageInstruction/0/patientInstruction"};
    const rapidjson::Pointer textInstructionPtr{"/dosageInstruction/0/text"};
    const auto* sourcePatientInstructionValue = patientInstructionPtr.Get(medicationDispense.jsonDocument());
    const auto* sourceTextInstructionValue = textInstructionPtr.Get(medicationDispense.jsonDocument());
    if (sourcePatientInstructionValue)
    {
        std::string expectedText;
        if (sourceTextInstructionValue)
        {
            expectedText = str(*sourceTextInstructionValue) + "; ";
        }
        expectedText += str(*sourcePatientInstructionValue);
        checkMappedValues(*dispenses[0], {MappedValue{.property = "MedicationDispense.dosageInstruction.0.text",
                                                      .targetValue = expectedText}});
    }

    auto sourceMedication = medicationDispense.containedResource<model::KbvMedicationGeneric>(0);
    ASSERT_TRUE(sourceMedication.has_value());
    static const rapidjson::Pointer metaProfilePtr{"/meta/profile/0"};
    auto profile =
        model::NumberAsStringParserDocument::getOptionalStringValue(sourceMedication->jsonDocument(), metaProfilePtr);
    ASSERT_TRUE(profile.has_value());
    auto medications = params->getMedications();
    ASSERT_EQ(medications.size(), 1);
    if (profile->starts_with("https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Medication_PZN"))
    {
        checkMedicationPzn(sourceMedication->jsonDocument(), *medications[0]);
    }
    else if (profile->starts_with("https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Medication_Compounding"))
    {
        checkMedicationCompounding(sourceMedication->jsonDocument(), *medications[0]);
    }
    else if (profile->starts_with("https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Medication_FreeText"))
    {
        checkMedicationFreeText(sourceMedication->jsonDocument(), *medications[0]);
    }
    else if (profile->starts_with("https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Medication_Ingredient"))
    {
        checkMedicationIngredient(sourceMedication->jsonDocument(), *medications[0]);
    }
    else
    {
        FAIL() << "unhandled medication profile: " << *profile;
    }

    validate(fhirtools::DefinitionKey{"https://gematik.de/fhir/epa-medication/StructureDefinition/"
                                      "epa-op-provide-dispensation-erp-input-parameters"},
             &params->jsonDocument());
    checkExpression("parameter[0].part[3].resource.medicationReference.reference.resolve()",
                    fhirtools::DefinitionKey{"https://gematik.de/fhir/epa-medication/StructureDefinition/"
                                             "epa-op-provide-prescription-erp-input-parameters"},
                    &params->jsonDocument());
}

INSTANTIATE_TEST_SUITE_P(medicationDispense, Epa4AllTransformerGematikTestdataMedicationDispense,
                         testing::ValuesIn(makeParams("medicationDispense", Type::medicationDispense)));
