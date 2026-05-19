/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/Communication.hxx"
#include "fhirtools/model/erp/ErpElement.hxx"
#include "fhirtools/parser/FhirPathParser.hxx"
#include "fhirtools/repository/FhirValueSet.hxx"
#include "fhirtools/repository/views/FhirStructureRepositoryView.hxx"
#include "fhirtools/validator/FhirPathValidator.hxx"
#include "shared/model/Bundle.hxx"
#include "test/fhirtools/SampleValidation.hxx"
#include "test/util/ResourceManager.hxx"

#include <boost/algorithm/string/case_conv.hpp>
#include <gtest/gtest.h>
#include <iostream>

using namespace fhirtools;

class FhirPathValidatorTest : public fhirtools::test::SampleValidationTest<FhirPathValidatorTest>
{
public:
    static std::list<std::filesystem::path> fileList()
    {
        return {
            // clang-format off
            "fhir/hl7.org/profiles-types.xml",
            "fhir/hl7.org/profiles-resources.xml",
            "fhir/hl7.org/extension-definitions.xml",
            "fhir/hl7.org/valuesets.xml",
            "fhir/hl7.org/v3-codesystems.xml",
            "fhir/profiles/hl7.terminology.r4-6.3.0/package/CodeSystem-v2-0066.xml",
            "fhir/profiles/hl7.terminology.r4-6.3.0/package-legacy-validation/CodeSystem-v2-0203.xml",
            "fhir/profiles/hl7.fhir.uv.extensions.r4-5.2.0/package/ValueSet-practitionerrole-employmentStatus.xml",
            "fhir/profiles/de.fhir.medication-1.0.3/package",
            "fhir/profiles/kbv.ita.erp-1.4.2/package",
            "fhir/profiles/kbv.ita.for-1.3.1/package",
            "fhir/profiles/de.basisprofil.r4-1.5.2/package",
            "fhir/profiles/kbv.basis-1.7.0/package",
            "fhir/profiles/fhir.kbv.de/KBV_CS_SFHIR_BAR2_ARZTNRFACHGRUPPE_V1.03.xml",
            "fhir/profiles/fhir.kbv.de/KBV_CS_SFHIR_BAR2_WBO_V1.16.xml",
            "fhir/profiles/fhir.kbv.de/KBV_CS_SFHIR_BMP_DOSIEREINHEIT_V1.01.xml",
            "fhir/profiles/fhir.kbv.de/KBV_CS_SFHIR_ICD_DIAGNOSESICHERHEIT_V1.00.xml",
            "fhir/profiles/fhir.kbv.de/KBV_CS_SFHIR_ICD_SEITENLOKALISATION_V1.00.xml",
            "fhir/profiles/fhir.kbv.de/KBV_CS_SFHIR_ITA_WOP_V1.00.xml",
            "fhir/profiles/fhir.kbv.de/KBV_CS_SFHIR_KBV_DMP_V1.06.xml",
            "fhir/profiles/fhir.kbv.de/KBV_CS_SFHIR_KBV_NORMGROESSE_V1.00.xml",
            "fhir/profiles/fhir.kbv.de/KBV_CS_SFHIR_KBV_PERSONENGRUPPE_V1.02.xml",
            "fhir/profiles/fhir.kbv.de/KBV_CS_SFHIR_KBV_PKV_TARIFF_V1.01.xml",
            "fhir/profiles/fhir.kbv.de/KBV_CS_SFHIR_KBV_STATUSKENNZEICHEN_V1.01.xml",
            "fhir/profiles/fhir.kbv.de/KBV_CS_SFHIR_KBV_VERSICHERTENSTATUS_V1.02.xml",
            "fhir/profiles/fhir.kbv.de/KBV_VS_SFHIR_BAR2_ARZTNRFACHGRUPPE_V1.03.xml",
            "fhir/profiles/fhir.kbv.de/KBV_VS_SFHIR_BMP_DOSIEREINHEIT_V1.01.xml",
            "fhir/profiles/fhir.kbv.de/KBV_VS_SFHIR_ICD_DIAGNOSESICHERHEIT_V1.00.xml",
            "fhir/profiles/fhir.kbv.de/KBV_VS_SFHIR_ICD_SEITENLOKALISATION_V1.00.xml",
            "fhir/profiles/fhir.kbv.de/KBV_VS_SFHIR_ITA_WOP_V1.00.xml",
            "fhir/profiles/fhir.kbv.de/KBV_VS_SFHIR_KBV_DMP_V1.06.xml",
            "fhir/profiles/fhir.kbv.de/KBV_VS_SFHIR_KBV_NORMGROESSE_V1.00.xml",
            "fhir/profiles/fhir.kbv.de/KBV_VS_SFHIR_KBV_PERSONENGRUPPE_V1.02.xml",
            "fhir/profiles/fhir.kbv.de/KBV_VS_SFHIR_KBV_PKV_TARIFF_V1.01.xml",
            "fhir/profiles/fhir.kbv.de/KBV_VS_SFHIR_KBV_STATUSKENNZEICHEN_V1.01.xml",
            "fhir/profiles/fhir.kbv.de/KBV_VS_SFHIR_KBV_VERSICHERTENSTATUS_V1.02.xml",
            "fhir/profiles/fhir.kbv.de/KBV_CS_SFHIR_KBV_DARREICHUNGSFORM_V1.16.xml",
            "fhir/profiles/fhir.kbv.de/KBV_CS_SFHIR_KBV_FORMULAR_ART_V1.02.xml",
            "fhir/profiles/fhir.kbv.de/KBV_VS_SFHIR_KBV_DARREICHUNGSFORM_V1.16.xml",
            "fhir/profiles/fhir.kbv.de/KBV_VS_SFHIR_KBV_FORMULAR_ART_V1.02.xml",
            "fhir/profiles/fhir.kbv.de/KBV_CS_SFHIR_KBV_NARCOTIC_LABEL_V1.00.xml",
            "fhir/profiles/fhir.kbv.de/KBV_VS_SFHIR_KBV_NARCOTIC_LABEL_V1.00.xml",

            // clang-format on
        };

    }
    ValidatorOptions validatorOptions() override
    {
        return {.reportUnknownExtensions = fhirtools::ValidatorOptions::ReportUnknownExtensionsMode::closeSlicing};
    }
};

using Sample = fhirtools::test::Sample;
using Severity = fhirtools::Severity;

// clang-format on


class FhirPathValidatorTestSamples : public ::fhirtools::test::SampleValidationTest<FhirPathValidatorTestSamples>
{

public:
    static std::list<std::filesystem::path> fileList()
    {
        auto result = SampleValidationTest::fileList();
        result.merge({"test/fhir-path/profiles/alternative_profiles.xml"});
        return result;
    }
    ValidatorOptions validatorOptions() override
    {
        auto opt = SampleValidationTest::validatorOptions();
        opt.reportUnknownExtensions = fhirtools::ValidatorOptions::ReportUnknownExtensionsMode::closeSlicing;
        return opt;
    }
};


TEST_P(FhirPathValidatorTestSamples, run)//NOLINT
{
    ASSERT_NO_FATAL_FAILURE(run());
}

// clang-format off
INSTANTIATE_TEST_SUITE_P(samples, FhirPathValidatorTestSamples,  ::testing::Values(
        Sample{"AlternativeProfiles", "test/fhir-path/samples/valid_AlternativeProfiles1.json", { } },
        Sample{"AlternativeProfiles", "test/fhir-path/samples/valid_AlternativeProfiles2.json", { } },
        Sample{"AlternativeProfiles", "test/fhir-path/samples/invalid_AlternativeProfiles1.json", {}, {
            {"Alternative1", "AlternativeProfiles.resource[1]{Resource}"},
            {"Alternative2", "AlternativeProfiles.resource[1]{Resource}"},
        }},
        Sample{"AlternativeProfiles", "test/fhir-path/samples/invalid_AlternativeProfiles2.json", {}, {
            {"Alternative1", "AlternativeProfiles.resource[0]{Resource}"},
            {"Alternative2", "AlternativeProfiles.resource[0]{Resource}"},
        }},
        Sample{"Bundle", "test/fhir-path/samples/ERP-11476-contentReferenceTypedField.xml"}
));
// clang-format on

TEST_F(FhirPathValidatorTest, OperationalValueSets)
{
    using namespace fhirtools::version_literal;
    {
        const auto* valueSet =
            repo().findValueSet({"https://fhir.kbv.de/ValueSet/KBV_VS_ERP_Accident_Type", "1.4.2"_ver});
        ASSERT_TRUE(valueSet);
        auto codes = fhirtools::FhirValueSetCodes::create(std::addressof(repo()), valueSet);
        ASSERT_TRUE(codes);
        EXPECT_TRUE(codes->canValidate());
        EXPECT_TRUE(codes->containsCode("1", "https://fhir.kbv.de/CodeSystem/KBV_CS_FOR_Ursache_Type"));
        EXPECT_TRUE(codes->containsCode("2", "https://fhir.kbv.de/CodeSystem/KBV_CS_FOR_Ursache_Type"));
        EXPECT_FALSE(codes->containsCode("3", "https://fhir.kbv.de/CodeSystem/KBV_CS_FOR_Ursache_Type"));
        EXPECT_TRUE(codes->containsCode("4", "https://fhir.kbv.de/CodeSystem/KBV_CS_FOR_Ursache_Type"));
    }
    {
        const auto* valueSet =
            repo().findValueSet({"https://fhir.kbv.de/ValueSet/KBV_VS_ERP_Medication_Category", "1.4.2"_ver});
        ASSERT_TRUE(valueSet);
        auto codes = fhirtools::FhirValueSetCodes::create(std::addressof(repo()), valueSet);
        EXPECT_TRUE(codes->canValidate());
        EXPECT_TRUE(codes->containsCode("00", "https://fhir.kbv.de/CodeSystem/KBV_CS_ERP_Medication_Category"));
        EXPECT_TRUE(codes->containsCode("01", "https://fhir.kbv.de/CodeSystem/KBV_CS_ERP_Medication_Category"));
        EXPECT_TRUE(codes->containsCode("02", "https://fhir.kbv.de/CodeSystem/KBV_CS_ERP_Medication_Category"));
    }
    {
        const auto* valueSet =
            repo().findValueSet({"https://fhir.kbv.de/ValueSet/KBV_VS_ERP_StatusCoPayment", "1.4.2"_ver});
        ASSERT_TRUE(valueSet);
        auto codes = fhirtools::FhirValueSetCodes::create(std::addressof(repo()), valueSet);
        ASSERT_TRUE(codes);
        EXPECT_TRUE(codes->canValidate());
        EXPECT_TRUE(codes->containsCode("0", "https://fhir.kbv.de/CodeSystem/KBV_CS_FOR_StatusCoPayment"));
        EXPECT_TRUE(codes->containsCode("1", "https://fhir.kbv.de/CodeSystem/KBV_CS_FOR_StatusCoPayment"));
        EXPECT_TRUE(codes->containsCode("2", "https://fhir.kbv.de/CodeSystem/KBV_CS_FOR_StatusCoPayment"));
    }
    {
        const auto* valueSet = repo().findValueSet({"https://fhir.kbv.de/ValueSet/KBV_VS_FOR_Payor_type", "1.3.1"_ver});
        ASSERT_TRUE(valueSet);
        auto codes = fhirtools::FhirValueSetCodes::create(std::addressof(repo()), valueSet);
        ASSERT_TRUE(codes);
        EXPECT_TRUE(codes->canValidate());
        EXPECT_TRUE(codes->containsCode("GKV", "http://fhir.de/CodeSystem/versicherungsart-de-basis"));
        EXPECT_TRUE(codes->containsCode("PKV", "http://fhir.de/CodeSystem/versicherungsart-de-basis"));
        EXPECT_TRUE(codes->containsCode("BG", "http://fhir.de/CodeSystem/versicherungsart-de-basis"));
        EXPECT_TRUE(codes->containsCode("SEL", "http://fhir.de/CodeSystem/versicherungsart-de-basis"));
        EXPECT_FALSE(codes->containsCode("SOZ", "http://fhir.de/CodeSystem/versicherungsart-de-basis"));
        EXPECT_FALSE(codes->containsCode("GPV", "http://fhir.de/CodeSystem/versicherungsart-de-basis"));
        EXPECT_FALSE(codes->containsCode("PPV", "http://fhir.de/CodeSystem/versicherungsart-de-basis"));
        EXPECT_FALSE(codes->containsCode("BEI", "http://fhir.de/CodeSystem/versicherungsart-de-basis"));
    }
    {
        const auto* valueSet =
            repo().findValueSet({"https://fhir.kbv.de/ValueSet/KBV_VS_FOR_Qualification_Type", "1.3.1"_ver});
        ASSERT_TRUE(valueSet);
        auto codes = fhirtools::FhirValueSetCodes::create(std::addressof(repo()), valueSet);
        ASSERT_TRUE(codes);
        EXPECT_TRUE(codes->canValidate());
        EXPECT_TRUE(codes->containsCode("00", "https://fhir.kbv.de/CodeSystem/KBV_CS_FOR_Qualification_Type"));
        EXPECT_TRUE(codes->containsCode("01", "https://fhir.kbv.de/CodeSystem/KBV_CS_FOR_Qualification_Type"));
        EXPECT_TRUE(codes->containsCode("02", "https://fhir.kbv.de/CodeSystem/KBV_CS_FOR_Qualification_Type"));
        EXPECT_TRUE(codes->containsCode("03", "https://fhir.kbv.de/CodeSystem/KBV_CS_FOR_Qualification_Type"));
        EXPECT_TRUE(codes->containsCode("04", "https://fhir.kbv.de/CodeSystem/KBV_CS_FOR_Qualification_Type"));
    }
    {
        const auto* valueSet =
            repo().findValueSet({"https://fhir.kbv.de/ValueSet/KBV_VS_Base_Apgar_Score_Identifier_LOINC", "1.7.0"_ver});
        ASSERT_TRUE(valueSet);
        auto codes = fhirtools::FhirValueSetCodes::create(std::addressof(repo()), valueSet);
        ASSERT_TRUE(codes);
        EXPECT_TRUE(codes->canValidate());
        EXPECT_TRUE(codes->containsCode("9274-2", "http://loinc.org"));
        EXPECT_TRUE(codes->containsCode("9271-8", "http://loinc.org"));
        EXPECT_FALSE(codes->containsCode("0", "http://loinc.org"));
    }
    {
        const auto* valueSet = repo().findValueSet(
            {"https://fhir.kbv.de/ValueSet/KBV_VS_Base_Apgar_Score_Identifier_SNOMED_CT", "1.7.0"_ver});
        ASSERT_TRUE(valueSet);
        auto codes = fhirtools::FhirValueSetCodes::create(std::addressof(repo()), valueSet);
        ASSERT_TRUE(codes);
        EXPECT_TRUE(codes->canValidate());
        EXPECT_TRUE(codes->containsCode("169922007", "http://snomed.info/sct"));
        EXPECT_TRUE(codes->containsCode("169909004", "http://snomed.info/sct"));
        EXPECT_FALSE(codes->containsCode("0", "http://snomed.info/sct"));
    }
    {
        const auto* valueSet =
            repo().findValueSet({"https://fhir.kbv.de/ValueSet/KBV_VS_Base_Apgar_Score_Value", "1.7.0"_ver});
        ASSERT_TRUE(valueSet);
        auto codes = fhirtools::FhirValueSetCodes::create(std::addressof(repo()), valueSet);
        ASSERT_TRUE(codes);
        EXPECT_TRUE(codes->canValidate());
        EXPECT_TRUE(codes->containsCode("77714001", "http://snomed.info/sct"));
        EXPECT_TRUE(codes->containsCode("43610007", "http://snomed.info/sct"));
        EXPECT_TRUE(codes->containsCode("39910003", "http://snomed.info/sct"));
        EXPECT_TRUE(codes->containsCode("26635001", "http://snomed.info/sct"));
        EXPECT_TRUE(codes->containsCode("38107004", "http://snomed.info/sct"));
        EXPECT_TRUE(codes->containsCode("24388001", "http://snomed.info/sct"));
        EXPECT_TRUE(codes->containsCode("10318004", "http://snomed.info/sct"));
        EXPECT_TRUE(codes->containsCode("12431004", "http://snomed.info/sct"));
        EXPECT_TRUE(codes->containsCode("64198003", "http://snomed.info/sct"));
        EXPECT_TRUE(codes->containsCode("64198003", "http://snomed.info/sct"));
        EXPECT_FALSE(codes->containsCode("169909004", "http://snomed.info/sct"));
    }
    {
        const auto* valueSet =
            repo().findValueSet({"http://fhir.de/ValueSet/VitalSignDE_Body_Height_Loinc", "1.5.2"_ver});
        ASSERT_TRUE(valueSet);
        auto codes = fhirtools::FhirValueSetCodes::create(std::addressof(repo()), valueSet);
        ASSERT_TRUE(codes);
        EXPECT_TRUE(codes->canValidate());
        EXPECT_TRUE(codes->containsCode("89269-5", "http://loinc.org"));
    }
    {
        const auto* valueSet =
            repo().findValueSet({"https://fhir.kbv.de/ValueSet/KBV_VS_Base_Body_Weight_Method_SNOMED_CT", "1.7.0"_ver});
        ASSERT_TRUE(valueSet);
        auto codes = fhirtools::FhirValueSetCodes::create(std::addressof(repo()), valueSet);
        ASSERT_TRUE(codes);
        EXPECT_TRUE(codes->canValidate());
        EXPECT_TRUE(codes->containsCode("363808001", "http://snomed.info/sct"));
        EXPECT_TRUE(codes->containsCode("786458005", "http://snomed.info/sct"));
    }
    {
        const auto* valueSet =
            repo().findValueSet({"http://fhir.de/ValueSet/VitalSignDE_Body_Weigth_UCUM", "1.5.2"_ver});
        ASSERT_TRUE(valueSet);
        auto codes = fhirtools::FhirValueSetCodes::create(std::addressof(repo()), valueSet);
        ASSERT_TRUE(codes);
        EXPECT_TRUE(codes->canValidate());
        EXPECT_TRUE(codes->containsCode("g", "http://unitsofmeasure.org"));
        EXPECT_TRUE(codes->containsCode("kg", "http://unitsofmeasure.org"));
        EXPECT_FALSE(codes->containsCode("t", "http://unitsofmeasure.org"));
    }
    {
        const auto* valueSet =
            repo().findValueSet({"https://fhir.kbv.de/ValueSet/KBV_VS_Base_Deuev_Anlage_8", "1.7.0"_ver});
        ASSERT_TRUE(valueSet);
        auto codes = fhirtools::FhirValueSetCodes::create(std::addressof(repo()), valueSet);
        ASSERT_TRUE(codes);
        EXPECT_TRUE(codes->canValidate());
        EXPECT_TRUE(codes->containsCode("D", "http://fhir.de/CodeSystem/deuev/anlage-8-laenderkennzeichen"));
        EXPECT_TRUE(codes->containsCode("AFG", "http://fhir.de/CodeSystem/deuev/anlage-8-laenderkennzeichen"));
        // ...
        EXPECT_TRUE(codes->containsCode("CY", "http://fhir.de/CodeSystem/deuev/anlage-8-laenderkennzeichen"));
        EXPECT_FALSE(codes->containsCode("DE", "http://fhir.de/CodeSystem/deuev/anlage-8-laenderkennzeichen"));
    }
    {
        const auto* valueSet = repo().findValueSet(
            {"https://fhir.kbv.de/ValueSet/KBV_VS_Base_Head_Circumference_BodySite_SNOMED_CT", "1.7.0"_ver});
        ASSERT_TRUE(valueSet);
        auto codes = fhirtools::FhirValueSetCodes::create(std::addressof(repo()), valueSet);
        ASSERT_TRUE(codes);
        EXPECT_TRUE(codes->canValidate());
        EXPECT_TRUE(codes->containsCode("302548004", "http://snomed.info/sct"));
        EXPECT_TRUE(codes->containsCode("362865009", "http://snomed.info/sct"));
    }
    {
        const auto* valueSet =
            repo().findValueSet({"https://fhir.kbv.de/ValueSet/KBV_VS_Base_Stage_Life", "1.7.0"_ver});
        ASSERT_TRUE(valueSet);
        auto codes = fhirtools::FhirValueSetCodes::create(std::addressof(repo()), valueSet);
        ASSERT_TRUE(codes);
        EXPECT_TRUE(codes->canValidate());
        EXPECT_TRUE(codes->containsCode("41847000", "http://snomed.info/sct"));
        EXPECT_TRUE(codes->containsCode("263659003", "http://snomed.info/sct"));
        EXPECT_TRUE(codes->containsCode("255398004", "http://snomed.info/sct"));
        EXPECT_TRUE(codes->containsCode("713153009", "http://snomed.info/sct"));
        EXPECT_TRUE(codes->containsCode("3658006", "http://snomed.info/sct"));
        EXPECT_TRUE(codes->containsCode("255407002", "http://snomed.info/sct"));
        EXPECT_TRUE(codes->containsCode("271872005", "http://snomed.info/sct"));
    }
    {
        const auto* valueSet =
            repo().findValueSet({"https://fhir.kbv.de/ValueSet/KBV_VS_SFHIR_KBV_STATUSKENNZEICHEN", "1.01"_ver});
        ASSERT_TRUE(valueSet);
        auto codes = fhirtools::FhirValueSetCodes::create(std::addressof(repo()), valueSet);
        ASSERT_TRUE(codes);
        EXPECT_TRUE(codes->canValidate());
        EXPECT_TRUE(codes->containsCode("00", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_STATUSKENNZEICHEN"));
        EXPECT_TRUE(codes->containsCode("00", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_STATUSKENNZEICHEN"));
        EXPECT_TRUE(codes->containsCode("01", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_STATUSKENNZEICHEN"));
        EXPECT_TRUE(codes->containsCode("04", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_STATUSKENNZEICHEN"));
        EXPECT_TRUE(codes->containsCode("07", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_STATUSKENNZEICHEN"));
        EXPECT_TRUE(codes->containsCode("10", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_STATUSKENNZEICHEN"));
        EXPECT_TRUE(codes->containsCode("11", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_STATUSKENNZEICHEN"));
        EXPECT_TRUE(codes->containsCode("14", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_STATUSKENNZEICHEN"));
        EXPECT_TRUE(codes->containsCode("17", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_STATUSKENNZEICHEN"));
    }
    {
        const auto* valueSet =
            repo().findValueSet({"https://fhir.kbv.de/ValueSet/KBV_VS_SFHIR_KBV_PKV_TARIFF", "1.01"_ver});
        ASSERT_TRUE(valueSet);
        auto codes = fhirtools::FhirValueSetCodes::create(std::addressof(repo()), valueSet);
        ASSERT_TRUE(codes);
        EXPECT_TRUE(codes->canValidate());
        EXPECT_TRUE(codes->containsCode("01", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_PKV_TARIFF"));
        EXPECT_TRUE(codes->containsCode("02", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_PKV_TARIFF"));
        EXPECT_TRUE(codes->containsCode("03", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_PKV_TARIFF"));
        EXPECT_TRUE(codes->containsCode("04", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_PKV_TARIFF"));
    }
    {
        const auto* valueSet =
            repo().findValueSet({"https://fhir.kbv.de/ValueSet/KBV_VS_SFHIR_KBV_PERSONENGRUPPE", "1.02"_ver});
        ASSERT_TRUE(valueSet);
        auto codes = fhirtools::FhirValueSetCodes::create(std::addressof(repo()), valueSet);
        ASSERT_TRUE(codes);
        EXPECT_TRUE(codes->canValidate());
        EXPECT_TRUE(codes->containsCode("00", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_PERSONENGRUPPE"));
        EXPECT_TRUE(codes->containsCode("04", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_PERSONENGRUPPE"));
        EXPECT_TRUE(codes->containsCode("06", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_PERSONENGRUPPE"));
        EXPECT_TRUE(codes->containsCode("07", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_PERSONENGRUPPE"));
        EXPECT_TRUE(codes->containsCode("08", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_PERSONENGRUPPE"));
        EXPECT_TRUE(codes->containsCode("09", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_PERSONENGRUPPE"));
    }
    {
        const auto* valueSet = repo().findValueSet({"https://fhir.kbv.de/ValueSet/KBV_VS_SFHIR_KBV_DMP", "1.06"_ver});
        ASSERT_TRUE(valueSet);
        auto codes = fhirtools::FhirValueSetCodes::create(std::addressof(repo()), valueSet);
        ASSERT_TRUE(codes);
        EXPECT_TRUE(codes->canValidate());
        EXPECT_TRUE(codes->containsCode("00", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DMP"));
        EXPECT_TRUE(codes->containsCode("01", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DMP"));
        EXPECT_TRUE(codes->containsCode("02", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DMP"));
        EXPECT_TRUE(codes->containsCode("03", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DMP"));
        EXPECT_TRUE(codes->containsCode("04", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DMP"));
        EXPECT_TRUE(codes->containsCode("05", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DMP"));
        EXPECT_TRUE(codes->containsCode("06", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DMP"));
        EXPECT_TRUE(codes->containsCode("07", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DMP"));
        EXPECT_TRUE(codes->containsCode("08", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DMP"));
        EXPECT_TRUE(codes->containsCode("09", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DMP"));
        EXPECT_TRUE(codes->containsCode("10", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DMP"));
        EXPECT_TRUE(codes->containsCode("11", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DMP"));
        // New in 1.106
        EXPECT_TRUE(codes->containsCode("12", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DMP"));
        EXPECT_TRUE(codes->containsCode("30", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DMP"));
        // ...
        EXPECT_TRUE(codes->containsCode("57", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DMP"));
        EXPECT_TRUE(codes->containsCode("58", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DMP"));
    }
    {
        const auto* valueSet =
            repo().findValueSet({"https://fhir.kbv.de/ValueSet/KBV_VS_SFHIR_KBV_VERSICHERTENSTATUS", "1.02"_ver});
        ASSERT_TRUE(valueSet);
        auto codes = fhirtools::FhirValueSetCodes::create(std::addressof(repo()), valueSet);
        ASSERT_TRUE(codes);
        EXPECT_TRUE(codes->canValidate());
        EXPECT_TRUE(codes->containsCode("1", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_VERSICHERTENSTATUS"));
        EXPECT_TRUE(codes->containsCode("3", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_VERSICHERTENSTATUS"));
        EXPECT_TRUE(codes->containsCode("5", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_VERSICHERTENSTATUS"));
    }
    {
        const auto* valueSet = repo().findValueSet({"https://fhir.kbv.de/ValueSet/KBV_VS_SFHIR_ITA_WOP", "1.00"_ver});
        ASSERT_TRUE(valueSet);
        auto codes = fhirtools::FhirValueSetCodes::create(std::addressof(repo()), valueSet);
        ASSERT_TRUE(codes);
        EXPECT_TRUE(codes->canValidate());
        EXPECT_TRUE(codes->containsCode("00", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_ITA_WOP"));
        EXPECT_TRUE(codes->containsCode("01", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_ITA_WOP"));
        // ...
        EXPECT_TRUE(codes->containsCode("98", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_ITA_WOP"));
    }
    {
        const auto* valueSet =
            repo().findValueSet({"https://fhir.kbv.de/ValueSet/KBV_VS_SFHIR_ICD_DIAGNOSESICHERHEIT", "1.00"_ver});
        ASSERT_TRUE(valueSet);
        auto codes = fhirtools::FhirValueSetCodes::create(std::addressof(repo()), valueSet);
        ASSERT_TRUE(codes);
        EXPECT_TRUE(codes->canValidate());
        EXPECT_TRUE(codes->containsCode("A", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_ICD_DIAGNOSESICHERHEIT"));
        EXPECT_TRUE(codes->containsCode("G", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_ICD_DIAGNOSESICHERHEIT"));
        EXPECT_TRUE(codes->containsCode("V", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_ICD_DIAGNOSESICHERHEIT"));
        EXPECT_TRUE(codes->containsCode("Z", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_ICD_DIAGNOSESICHERHEIT"));
    }
    {
        const auto* valueSet =
            repo().findValueSet({"https://fhir.kbv.de/ValueSet/KBV_VS_SFHIR_KBV_NORMGROESSE", "1.00"_ver});
        ASSERT_TRUE(valueSet);
        auto codes = fhirtools::FhirValueSetCodes::create(std::addressof(repo()), valueSet);
        ASSERT_TRUE(codes);
        EXPECT_TRUE(codes->canValidate());
        EXPECT_TRUE(codes->containsCode("KA", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_NORMGROESSE"));
        EXPECT_TRUE(codes->containsCode("KTP", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_NORMGROESSE"));
        EXPECT_TRUE(codes->containsCode("N1", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_NORMGROESSE"));
        EXPECT_TRUE(codes->containsCode("N2", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_NORMGROESSE"));
        EXPECT_TRUE(codes->containsCode("N3", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_NORMGROESSE"));
        EXPECT_TRUE(codes->containsCode("NB", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_NORMGROESSE"));
        EXPECT_TRUE(codes->containsCode("Sonstiges", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_NORMGROESSE"));
    }
    {
        const auto* valueSet =
            repo().findValueSet({"https://fhir.kbv.de/ValueSet/KBV_VS_SFHIR_ICD_SEITENLOKALISATION", "1.00"_ver});
        ASSERT_TRUE(valueSet);
        auto codes = fhirtools::FhirValueSetCodes::create(std::addressof(repo()), valueSet);
        ASSERT_TRUE(codes);
        EXPECT_TRUE(codes->canValidate());
        EXPECT_TRUE(codes->containsCode("B", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_ICD_SEITENLOKALISATION"));
        EXPECT_TRUE(codes->containsCode("L", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_ICD_SEITENLOKALISATION"));
        EXPECT_TRUE(codes->containsCode("R", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_ICD_SEITENLOKALISATION"));
    }
    {
        const auto* valueSet =
            repo().findValueSet({"https://fhir.kbv.de/ValueSet/KBV_VS_SFHIR_KBV_DARREICHUNGSFORM", "1.16"_ver});
        ASSERT_TRUE(valueSet);
        auto codes = fhirtools::FhirValueSetCodes::create(std::addressof(repo()), valueSet);
        ASSERT_TRUE(codes);
        EXPECT_TRUE(codes->canValidate());
        EXPECT_TRUE(codes->containsCode("AEO", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DARREICHUNGSFORM"));
        EXPECT_TRUE(codes->containsCode("AMP", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DARREICHUNGSFORM"));
        // ...
        EXPECT_TRUE(codes->containsCode("PFT", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DARREICHUNGSFORM"));
        EXPECT_TRUE(codes->containsCode("ZPA", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DARREICHUNGSFORM"));
        // New in 1.14:
        EXPECT_TRUE(codes->containsCode("IID", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DARREICHUNGSFORM"));
        EXPECT_TRUE(codes->containsCode("LIV", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DARREICHUNGSFORM"));
        // New in 1.15:
        EXPECT_TRUE(codes->containsCode("LYE", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DARREICHUNGSFORM"));
        EXPECT_TRUE(codes->containsCode("PUE", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DARREICHUNGSFORM"));
        // New in 1.16:
        EXPECT_TRUE(codes->containsCode("RKT", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DARREICHUNGSFORM"));
        EXPECT_TRUE(codes->containsCode("SUF", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DARREICHUNGSFORM"));
    }
    {
        const auto* valueSet =
            repo().findValueSet({"https://fhir.kbv.de/ValueSet/KBV_VS_SFHIR_KBV_FORMULAR_ART", "1.02"_ver});
        ASSERT_TRUE(valueSet);
        auto codes = fhirtools::FhirValueSetCodes::create(std::addressof(repo()), valueSet);
        ASSERT_TRUE(codes);
        EXPECT_TRUE(codes->canValidate());
        EXPECT_TRUE(codes->containsCode("e010", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_FORMULAR_ART"));
        EXPECT_TRUE(codes->containsCode("e011", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_FORMULAR_ART"));
        EXPECT_TRUE(codes->containsCode("e012", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_FORMULAR_ART"));
        EXPECT_TRUE(codes->containsCode("e013", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_FORMULAR_ART"));
        EXPECT_TRUE(codes->containsCode("e16A", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_FORMULAR_ART"));
        EXPECT_TRUE(codes->containsCode("e16D", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_FORMULAR_ART"));
    }
    {
        // only Valueset that uses compose.exclude.filter with property
        const auto* valueSet = repo().findValueSet({"http://hl7.org/fhir/ValueSet/practitionerrole-employmentStatus", "5.2.0"_ver});
        ASSERT_TRUE(valueSet);
        auto codes = fhirtools::FhirValueSetCodes::create(std::addressof(repo()), valueSet);
        ASSERT_TRUE(codes);
        EXPECT_TRUE(codes->canValidate());
        EXPECT_TRUE(codes->containsCode("1", "http://terminology.hl7.org/CodeSystem/v2-0066"));
        EXPECT_FALSE(codes->containsCode("F", "http://terminology.hl7.org/CodeSystem/v2-0066"));
        EXPECT_FALSE(codes->containsCode("...", "http://terminology.hl7.org/CodeSystem/v2-0066"));
        EXPECT_TRUE(codes->containsCode("2", "http://terminology.hl7.org/CodeSystem/v2-0066"));
        EXPECT_FALSE(codes->containsCode("P", "http://terminology.hl7.org/CodeSystem/v2-0066"));
        EXPECT_TRUE(codes->containsCode("4", "http://terminology.hl7.org/CodeSystem/v2-0066"));
        EXPECT_FALSE(codes->containsCode("D", "http://terminology.hl7.org/CodeSystem/v2-0066"));
        EXPECT_TRUE(codes->containsCode("C", "http://terminology.hl7.org/CodeSystem/v2-0066"));
        EXPECT_TRUE(codes->containsCode("L", "http://terminology.hl7.org/CodeSystem/v2-0066"));
        EXPECT_TRUE(codes->containsCode("T", "http://terminology.hl7.org/CodeSystem/v2-0066"));
        EXPECT_TRUE(codes->containsCode("3", "http://terminology.hl7.org/CodeSystem/v2-0066"));
        EXPECT_TRUE(codes->containsCode("5", "http://terminology.hl7.org/CodeSystem/v2-0066"));
        EXPECT_TRUE(codes->containsCode("6", "http://terminology.hl7.org/CodeSystem/v2-0066"));
        EXPECT_TRUE(codes->containsCode("O", "http://terminology.hl7.org/CodeSystem/v2-0066"));
        EXPECT_TRUE(codes->containsCode("9", "http://terminology.hl7.org/CodeSystem/v2-0066"));
    }
}


struct FhirPathValidatorTestMiniSamplesParams {
    std::string_view name;
};

class FhirPathValidatorTestMiniSamples : public ::fhirtools::test::SampleValidationTest<FhirPathValidatorTestMiniSamples>
{

public:
    static std::list<std::filesystem::path> fileList()
    {
        return {
            "test/fhir-path/profiles/minifhirtypes.xml",
            "test/fhir-path/profiles/indirect_alternative_profiles.xml",
        };
    }

};

TEST_P(FhirPathValidatorTestMiniSamples, samples)
{
    ASSERT_NO_FATAL_FAILURE(run());
}


INSTANTIATE_TEST_SUITE_P(
    samples, FhirPathValidatorTestMiniSamples,
    ::testing::ValuesIn(std::list<fhirtools::test::Sample>{
        {"Resource", "test/fhir-path/samples/valid_IndirectAlternativeProfiles1.json"},
        {"Resource", "test/fhir-path/samples/valid_IndirectAlternativeProfiles2.json"},
        {
            "Resource",
            "test/fhir-path/samples/invalid_IndirectAlternativeProfiles.json",
            {
                {{Severity::error,
                  R"(value must match fixed value: "http://fhir-tools.test/IndirectAlternativeProfile1/Extension" )"
                  R"((but is "http://fhir-tools.test/UNEXPECTED_URL"))"},
                 "Resource.contained[0]{Resource}.extension[0].url"},
                {{Severity::error,
                  R"(value must match fixed value: "http://fhir-tools.test/IndirectAlternativeProfile2/Extension" )"
                  R"((but is "http://fhir-tools.test/UNEXPECTED_URL"))"},
                 "Resource.contained[0]{Resource}.extension[0].url"},
            },
        },
        { "Resource", "test/fhir-path/issues/ERP-26128-ValueUrlExtension.xml"},
        { "Resource", "test/fhir-path/issues/ERP-33234-Invalid-Codes-data-absent.json"},
        { "Resource", "test/fhir-path/issues/ERP-33234-Invalid-Codes.json",
            {
            {{ Severity::error, R"(code must not start with whitespace.)"}, "Resource.extension[0].valueCode"}
            }
        },
    }));

