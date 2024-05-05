/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */


#include "fhirtools/repository/FhirResourceViewConfiguration.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"

#include <gtest/gtest.h>
#include <rapidjson/document.h>
#include <rapidjson/pointer.h>
#include <rapidjson/rapidjson.h>

struct Tag {
    std::string tag;
    std::string version;
    std::initializer_list<std::string> urls;
};
struct View {
    std::string envName;
    std::string validFrom;
    std::string validUntil;
    std::initializer_list<std::string> tags;
};

class FhirResourceViewConfigurationTest : public testing::Test
{
public:
    FhirResourceViewConfigurationTest()
    {
        mDocument.SetObject();
    }

    gsl::not_null<const rapidjson::Value*> tagConfig(std::initializer_list<Tag> tags);
    gsl::not_null<const rapidjson::Value*> viewConfig(std::initializer_list<View> views);

protected:
    rapidjson::Document mDocument;
};

gsl::not_null<const rapidjson::Value*> FhirResourceViewConfigurationTest::tagConfig(std::initializer_list<Tag> tags)
{
    int idx = 0;
    for (const auto& tag : tags)
    {
        const std::string base{"/fhir-resource-tags/" + std::to_string(idx) + "/"};
        rapidjson::Pointer ptTag{base + "tag"};
        ptTag.Set(mDocument, tag.tag);
        rapidjson::Pointer ptVersion{base + "version"};
        ptVersion.Set(mDocument, tag.version);
        const std::string baseUrls{base + "urls/"};
        int idxUrls = 0;
        for (const auto& url : tag.urls)
        {
            rapidjson::Pointer ptUrl{baseUrls + std::to_string(idxUrls)};
            ptUrl.Set(mDocument, url);
            ++idxUrls;
        }
        ++idx;
    }

    const rapidjson::Pointer ptTags{"/fhir-resource-tags"};
    return ptTags.Get(mDocument);
}

gsl::not_null<const rapidjson::Value*> FhirResourceViewConfigurationTest::viewConfig(std::initializer_list<View> views)
{
    int idx = 0;
    for (const auto& view : views)
    {
        const std::string base{"/fhir-resource-views/" + std::to_string(idx) + "/"};
        rapidjson::Pointer ptEnv{base + "env-name"};
        ptEnv.Set(mDocument, view.envName);
        if (! view.validFrom.empty())
        {
            rapidjson::Pointer ptValidFrom{base + "valid-from"};
            ptValidFrom.Set(mDocument, view.validFrom);
        }
        if (! view.validUntil.empty())
        {
            rapidjson::Pointer ptValidUntil{base + "valid-until"};
            ptValidUntil.Set(mDocument, view.validUntil);
        }
        int idxTags = 0;
        for (const auto& tag : view.tags)
        {
            rapidjson::Pointer ptTag{base + "tags/" + std::to_string(idxTags)};
            ptTag.Set(mDocument, tag);
            ++idxTags;
        }
        ++idx;
    }

    const rapidjson::Pointer ptViews{"/fhir-resource-views"};
    return ptViews.Get(mDocument);
}

TEST_F(FhirResourceViewConfigurationTest, DarreichungsformViews)
{
    const auto tagCfg = tagConfig({{"darreichungsform_1_12",
                                    "1.12",
                                    {"https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DARREICHUNGSFORM",
                                     "https://fhir.kbv.de/ValueSet/KBV_VS_SFHIR_KBV_DARREICHUNGSFORM"}},
                                   {"darreichungsform_1_13",
                                    "1.13",
                                    {"https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DARREICHUNGSFORM",
                                     "https://fhir.kbv.de/ValueSet/KBV_VS_SFHIR_KBV_DARREICHUNGSFORM"}}});
    const auto viewCfg = viewConfig({{"ERP_DARREICHUNGSFORM_1_12", "", "2024-06-30", {"darreichungsform_1_12"}},
                                     {"ERP_DARREICHUNGSFORM_1_13", "2024-07-01", "", {"darreichungsform_1_13"}}});

    std::optional<fhirtools::FhirResourceViewConfiguration> cfg;
    ASSERT_NO_FATAL_FAILURE(cfg.emplace(tagCfg, viewCfg));

    EXPECT_EQ(cfg->getValidVersion("https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DARREICHUNGSFORM",
                                   fhirtools::Date{"2024-01-12"}),
              "1.12");
    EXPECT_EQ(cfg->getValidVersion("https://fhir.kbv.de/ValueSet/KBV_VS_SFHIR_KBV_DARREICHUNGSFORM",
                                   fhirtools::Date{"2024-01-12"}),
              "1.12");
    EXPECT_FALSE(cfg->getValidVersion("https://fhir.kbv.de/ValueSet/XXX", fhirtools::Date{"2025-01-12"}));

    EXPECT_EQ(cfg->getValidVersion("https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DARREICHUNGSFORM",
                                   fhirtools::Date{"2025-01-12"}),
              "1.13");
    EXPECT_EQ(cfg->getValidVersion("https://fhir.kbv.de/ValueSet/KBV_VS_SFHIR_KBV_DARREICHUNGSFORM",
                                   fhirtools::Date{"2025-01-12"}),
              "1.13");
    EXPECT_FALSE(cfg->getValidVersion("https://fhir.kbv.de/ValueSet/XXX", fhirtools::Date{"2024-01-12"}));
}

struct OverlapParam {
    std::string validFrom1;
    std::string validUntil1;
    std::string validFrom2;
    std::string validUntil2;
};
class FhirResourceViewConfigurationTestOverlap : public FhirResourceViewConfigurationTest,
                                                 public ::testing::WithParamInterface<OverlapParam>
{
};

TEST_P(FhirResourceViewConfigurationTestOverlap, ViewsOverlapping)
{
    const auto tagCfg = tagConfig({{"darreichungsform_1_12",
                                    "1.12",
                                    {"https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DARREICHUNGSFORM",
                                     "https://fhir.kbv.de/ValueSet/KBV_VS_SFHIR_KBV_DARREICHUNGSFORM"}},
                                   {"darreichungsform_1_13",
                                    "1.13",
                                    {"https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DARREICHUNGSFORM",
                                     "https://fhir.kbv.de/ValueSet/KBV_VS_SFHIR_KBV_DARREICHUNGSFORM"}}});
    const auto viewCfg = viewConfig(
        {{"ERP_DARREICHUNGSFORM_1_12", GetParam().validFrom1, GetParam().validUntil1, {"darreichungsform_1_12"}},
         {"ERP_DARREICHUNGSFORM_1_13", GetParam().validFrom2, GetParam().validUntil2, {"darreichungsform_1_13"}}});

    std::optional<fhirtools::FhirResourceViewConfiguration> cfg;
    try
    {
        cfg.emplace(tagCfg, viewCfg);
        FAIL() << "Expected exception on view validity overlap.";
    }
    catch (const std::runtime_error& re)
    {
        EXPECT_EQ(std::string{re.what()}, "fhir-resource-views must not overlap in validity. overlapping: "
                                          "https://fhir.kbv.de/ValueSet/KBV_VS_SFHIR_KBV_DARREICHUNGSFORM|1.12 and "
                                          "https://fhir.kbv.de/ValueSet/KBV_VS_SFHIR_KBV_DARREICHUNGSFORM|1.13");
    }
}

INSTANTIATE_TEST_SUITE_P(overlap, FhirResourceViewConfigurationTestOverlap,
                         testing::Values(OverlapParam{"", "2024-01-12", "2024-01-12", ""}, OverlapParam{"", "", "", ""},
                                         OverlapParam{"", "2024-01-11", "", ""},
                                         OverlapParam{"2023-01-12", "2024-01-12", "2024-01-12", ""},
                                         OverlapParam{"2023-01-12", "2024-01-12", "2024-01-12", "2025-01-12"}));

struct InconsistentParam {
    std::string validFrom;
    std::string validUntil;
};
class FhirResourceViewConfigurationTestInconsistent : public FhirResourceViewConfigurationTest,
                                                      public ::testing::WithParamInterface<InconsistentParam>
{
};

TEST_P(FhirResourceViewConfigurationTestInconsistent, ViewInconsistent)
{
    const auto tagCfg = tagConfig({
        {"darreichungsform_1_12", "1.12", {"https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DARREICHUNGSFORM"}},
    });
    const auto viewCfg = viewConfig(
        {{"ERP_DARREICHUNGSFORM_1_12", GetParam().validFrom, GetParam().validUntil, {"darreichungsform_1_12"}}});

    std::optional<fhirtools::FhirResourceViewConfiguration> cfg;
    try
    {
        cfg.emplace(tagCfg, viewCfg);
        FAIL() << "Expected exception on view inconsistency.";
    }
    catch (const std::runtime_error& re)
    {
        EXPECT_EQ(std::string{re.what()},
                  (std::ostringstream{} << "inconsistent view: " << "Start: " << GetParam().validFrom
                                        << " End: " << GetParam().validUntil << " Version: 1.12")
                      .str());
    }
    std::cout << fhirtools::Date{};
}

INSTANTIATE_TEST_SUITE_P(inconsistent, FhirResourceViewConfigurationTestInconsistent,
                         testing::Values(InconsistentParam{"2024-01-12", "2024-01-11"}));
