/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/KbvBundle.hxx"
#include "erp/model/KbvMedicationBase.hxx"
#include "erp/model/KbvMedicationRequest.hxx"
#include "erp/model/KbvOrganization.hxx"
#include "erp/model/KbvPractitioner.hxx"
#include "erp/model/MedicationDispense.hxx"
#include "erp/model/MedicationDispenseId.hxx"
#include "erp/model/Patient.hxx"
#include "exporter/Epa4AllTransformer.hxx"
#include "exporter/ExporterRequirements.hxx"
#include "test/exporter/Epa4AllTransformerTest.hxx"
#include "test/util/ResourceManager.hxx"

#include <fstream>

/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
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
    medicationDispense
};
struct Params {
    std::filesystem::path sampleDir;
    std::string sampleName;
    std::filesystem::path inputFile() const
    {
        return sampleDir / "input.xml";
    }
    std::filesystem::path outputFile() const
    {
        return sampleDir / "output.xml";
    }
    Type type;
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
};

TEST_P(Epa4AllTransformerGematikTestdataPrescription, Test)
{
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
    switch (GetParam().type)
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
    }
    checkOrganization(kbvInputBundle.getUniqueResourceByType<model::KbvOrganization>().jsonDocument(),
                      *params->getOrganization());
    checkPractitioner(kbvInputBundle.getResourcesByType<model::KbvPractitioner>()[0].jsonDocument(),
                      *params->getPractitioner());

    validate(fhirtools::DefinitionKey{"https://gematik.de/fhir/epa-medication/StructureDefinition/"
                                      "epa-op-provide-prescription-erp-input-parameters|1.0.3"},
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
}

INSTANTIATE_TEST_SUITE_P(medicationCompounding, Epa4AllTransformerGematikTestdataPrescription,
                         testing::ValuesIn(makeParams("medicationCompounding", Type::medicationCompounding)));

INSTANTIATE_TEST_SUITE_P(medicationFreeText, Epa4AllTransformerGematikTestdataPrescription,
                         testing::ValuesIn(makeParams("medicationFreeText", Type::medicationFreeText)));

INSTANTIATE_TEST_SUITE_P(medicationIngredient, Epa4AllTransformerGematikTestdataPrescription,
                         testing::ValuesIn(makeParams("medicationIngredient", Type::medicationIngredient)));

INSTANTIATE_TEST_SUITE_P(medicationPzn, Epa4AllTransformerGematikTestdataPrescription,
                         testing::ValuesIn(makeParams("medicationPzn", Type::medicationPzn)));


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
                                      "epa-op-provide-dispensation-erp-input-parameters|1.0.3"},
             &params->jsonDocument());
    checkExpression("parameter[0].part[3].resource.medicationReference.reference.resolve()",
                    fhirtools::DefinitionKey{"https://gematik.de/fhir/epa-medication/StructureDefinition/"
                                             "epa-op-provide-prescription-erp-input-parameters|1.0.3"},
                    &params->jsonDocument());
}

INSTANTIATE_TEST_SUITE_P(medicationDispense, Epa4AllTransformerGematikTestdataMedicationDispense,
                         testing::ValuesIn(makeParams("medicationDispense", Type::medicationDispense)));
