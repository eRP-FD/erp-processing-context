/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/KbvBundle.hxx"
#include "erp/model/ResourceFactory.hxx"
#include "erp/util/Base64.hxx"
#include "erp/util/FileHelper.hxx"
#include "erp/validation/XmlValidator.hxx"
#include "fhirtools/validator/ValidationResult.hxx"
#include "fhirtools/validator/ValidatorOptions.hxx"
#include "test/util/ResourceManager.hxx"
#include "test/util/StaticData.hxx"
#include "test/util/TestUtils.hxx"

#include <gtest/gtest.h>
#include <magic_enum.hpp>

#include "test_config.h"

namespace fs = std::filesystem;

static fs::path resourceTestOutputDir()
{
    return std::filesystem::canonical(std::filesystem::path(ResourceManager::resourceBinaryDir()) /
                                      "../../resources/test");
}

struct KonnektathonData {
    fs::path basePath;
    fs::path path;
    model::ResourceVersion::FhirProfileBundleVersion bundle;

};
std::ostream& operator<<(std::ostream& os, const KonnektathonData& params)
{
    const auto str = fs::relative(params.path, resourceTestOutputDir() / params.basePath).string();
    os << str;
    return os;
}


std::vector<KonnektathonData> makeParams(const std::string_view& path,
                                         model::ResourceVersion::FhirProfileBundleVersion bundle)
{
    std::vector<KonnektathonData> params;
    for (const auto& dirEntry : fs::recursive_directory_iterator(resourceTestOutputDir() / path))
    {
        if (dirEntry.is_regular_file() && dirEntry.path().has_extension() &&
            String::toLower(dirEntry.path().extension().string()) == ".xml")
        {
            const auto document = FileHelper::readFileAsString(dirEntry.path());
            if (document.find("<profile value=\"https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Bundle|") !=
                std::string::npos)
            {
                params.push_back({.basePath = path, .path = dirEntry.path(), .bundle = bundle});
            }
        }
    }
    return params;
}

class SampleDataTest : public testing::TestWithParam<KonnektathonData>
{
protected:

    SampleDataTest()
    {
        invalidList = {
            {resourceTestOutputDir() / "Konnektathon/7. Konnektathon/Medatixx2_Konnektathon-02-03-11/generierte "
                                       "eRezepte_medatixx2/20211103_103111_03799601.xml",
             "error: Element '{http://hl7.org/fhir}coding': "
             "Missing child element(s). Expected is ( {http://hl7.org/fhir}code ).; "
             "Bundle.entry[4].resource{Medication}.form: "
             "error: Expected exactly one system and one code sub-element "
             "(from profile: https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Medication_PZN|1.0.2); "
             "Bundle.entry[4].resource{Medication}.form.coding[0].code: "
             "error: missing mandatory element "
             "(from profile: https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Medication_PZN|1.0.2); "},
            {resourceTestOutputDir() / "Konnektathon/7. Konnektathon/Medatixx2_Konnektathon-02-03-11/generierte "
                                       "eRezepte_medatixx2/20211103_103237_03799601.xml",
             "error: Element '{http://hl7.org/fhir}coding': "
             "Missing child element(s). Expected is ( {http://hl7.org/fhir}code ).; "
             "Bundle.entry[4].resource{Medication}.form: "
             "error: Expected exactly one system and one code sub-element "
             "(from profile: https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Medication_PZN|1.0.2); "
             "Bundle.entry[4].resource{Medication}.form.coding[0].code: "
             "error: missing mandatory element "
             "(from profile: https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Medication_PZN|1.0.2); "},
            {resourceTestOutputDir() / "Konnektathon/7. Konnektathon/Medatixx2_Konnektathon-02-03-11/generierte "
                                       "eRezepte_medatixx2/20211103_103515_03799601.xml",
             "error: Element '{http://hl7.org/fhir}coding': "
             "Missing child element(s). Expected is ( {http://hl7.org/fhir}code ).; "
             "Bundle.entry[4].resource{Medication}.form: "
             "error: Expected exactly one system and one code sub-element "
             "(from profile: https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Medication_PZN|1.0.2); "
             "Bundle.entry[4].resource{Medication}.form.coding[0].code: "
             "error: missing mandatory element "
             "(from profile: https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Medication_PZN|1.0.2); "},
            {resourceTestOutputDir() / "Konnektathon/7. "
                                       "Konnektathon/MedVision_Konnektathon7/Output/160_000_074_295_222_21.xml",
             "Element '{http://hl7.org/fhir}postalCode', attribute 'value': [facet 'minLength'] The value '' has a "
             "length of '0'; this underruns the allowed minimum length of '1'."},
            {resourceTestOutputDir() / "Konnektathon/7. "
                                       "Konnektathon/MedVision_Konnektathon7/Output/160_000_074_295_888_60.xml",
             "Element '{http://hl7.org/fhir}birthDate', attribute 'value': '1940-00-00' is not a valid value of the "
             "union type '{http://hl7.org/fhir}date-primitive'."},
            {resourceTestOutputDir() /
                 "Konnektathon/8. Konnektathon/Konnektathon8.ADV/160.000.080.727.847.05.20211119/02-Medication.xml",
             "Bundle.entry[6].resource{Coverage}: error: "
             "Versichertenart-Pflicht: Die Versichertenart ist nicht vorhanden, sie ist aber bei den "
             "Kostentraegern vom Typ 'GKV', 'BG', 'SKT', 'PKV' oder 'UK' ein Pflichtelement. "
             "(from profile: https://fhir.kbv.de/StructureDefinition/KBV_PR_FOR_Coverage|1.0.3); "},
            {resourceTestOutputDir() / "Kostbarkeiten_der_Abgabe_Abrechnung/Kostbarkeiten_der_Abgabe_Abrechnung/IKK_BB/"
                                       "IKK_BB_109519005_Beispiel_frei_2.xml",
             "Opening and ending tag mismatch: practitioner line 213 and Practitioner"},
            {resourceTestOutputDir() / "Kostbarkeiten_der_Abgabe_Abrechnung/Kostbarkeiten_der_Abgabe_Abrechnung/AOK_NO/"
                                       "AOK_NO_109519005_Kostbarkeit_Zuzahlungsfrei.xml",
             "Element '{http://hl7.org/fhir}birthDate', attribute 'value': '2015-02-29' is not a valid value of the "
             "union type '{http://hl7.org/fhir}date-primitive'."}};
    }

    std::map<fs::path, std::string> invalidList;
    std::vector<EnvironmentVariableGuard> envGuards  = testutils::getOverlappingFhirProfileEnvironment();
};

TEST_P(SampleDataTest, SampleDataTest)
{
    const auto candidate = invalidList.find(GetParam().path);
    bool shallFail = candidate != invalidList.end();

    const auto document = FileHelper::readFileAsString(GetParam().path);

    fhirtools::ValidatorOptions options{.allowNonLiteralAuthorReference = true,
                                        .levels{.unreferencedBundledResource = fhirtools::Severity::warning,
                                                .mandatoryResolvableReferenceFailure = fhirtools::Severity::warning}};

    try
    {
        using KbvBundleFactory = model::ResourceFactory<model::KbvBundle>;
        auto bundle = KbvBundleFactory::fromXml(document, *StaticData::getXmlValidator(), {.validatorOptions = options})
                .getValidated(SchemaType::KBV_PR_ERP_Bundle, *StaticData::getXmlValidator(), *StaticData::getInCodeValidator(), {GetParam().bundle});
        ASSERT_FALSE(shallFail);
    }
    catch (const ErpException& erp)
    {
        ASSERT_TRUE(shallFail) << erp.what() << erp.diagnostics().value_or("");
        EXPECT_EQ(erp.diagnostics().value_or(""), candidate->second) << erp.what();
    }
}

INSTANTIATE_TEST_SUITE_P(Konnektathon, SampleDataTest,
                         testing::ValuesIn(makeParams("Konnektathon",
                                                      model::ResourceVersion::FhirProfileBundleVersion::v_2022_01_01)));

INSTANTIATE_TEST_SUITE_P(Kostbarkeiten_der_Abgabe_Abrechnung, SampleDataTest,
                         testing::ValuesIn(makeParams("Kostbarkeiten_der_Abgabe_Abrechnung",
                                                      model::ResourceVersion::FhirProfileBundleVersion::v_2022_01_01)));

INSTANTIATE_TEST_SUITE_P(MVO_KBV_1_0_2_, SampleDataTest,
                         testing::ValuesIn(makeParams("MVO_KBV_1.0.2_",
                                                      model::ResourceVersion::FhirProfileBundleVersion::v_2022_01_01)));
