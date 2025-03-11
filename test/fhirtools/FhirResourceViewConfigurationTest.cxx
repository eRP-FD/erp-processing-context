/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */


#include "fhirtools/repository/FhirResourceGroupConfiguration.hxx"
#include "fhirtools/repository/FhirResourceGroupConst.hxx"
#include "fhirtools/repository/FhirResourceViewConfiguration.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "MockFhirResourceGroup.hxx"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <rapidjson/document.h>
#include <rapidjson/pointer.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/rapidjson.h>
#include <rapidjson/stringbuffer.h>
#include <test/util/ResourceManager.hxx>

namespace
{
struct View {
    std::string envName;
    std::string validFrom;
    std::string validUntil;
    std::initializer_list<std::string> groups;
};

}
class FhirResourceViewConfigurationTest : public testing::Test
{
public:
    FhirResourceViewConfigurationTest()
    {
        mDocument.SetObject();
    }

    static std::list<std::string> ids(const fhirtools::FhirResourceViewConfiguration::ViewList& views)
    {
        std::list<std::string> result;
        std::ranges::transform(views, std::back_inserter(result),
                               &fhirtools::FhirResourceViewConfiguration::ViewConfig::mId);
        result.sort();
        return result;
    }

    gsl::not_null<const rapidjson::Value*> viewConfig(std::initializer_list<View> views);

protected:
    rapidjson::Document mDocument;
};


gsl::not_null<const rapidjson::Value*> FhirResourceViewConfigurationTest::viewConfig(std::initializer_list<View> views)
{
    int idx = 0;
    for (const auto& view : views)
    {
        const std::string base{"/fhir-resource-views/" + std::to_string(idx) + "/"};
        rapidjson::Pointer ptEnv{base + "env-name"};
        ptEnv.Set(mDocument, view.envName);
        rapidjson::Pointer ptId{base + "id"};
        ptId.Set(mDocument, view.envName);
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
        for (const auto& group : view.groups)
        {
            rapidjson::Pointer ptTag{base + "groups/" + std::to_string(idxTags)};
            ptTag.Set(mDocument, group);
            ++idxTags;
        }
        ++idx;
    }
    const rapidjson::Pointer ptViews{"/fhir-resource-views"};
    return ptViews.Get(mDocument);
}

TEST_F(FhirResourceViewConfigurationTest, ViewsSelection)
{
    using strings = std::list<std::string>;
    MockResourceGroupResolver res;
    EXPECT_CALL(res, findGroupById(::testing::StrEq("darreichungsform_1_12")))
        .WillRepeatedly(::testing::Return(std::make_shared<MockResourceGroup>()));
    EXPECT_CALL(res, findGroupById(::testing::StrEq("darreichungsform_1_13")))
        .WillRepeatedly(::testing::Return(std::make_shared<MockResourceGroup>()));
    EXPECT_CALL(res, findGroupById(::testing::StrEq("darreichungsform_1_14")))
        .WillRepeatedly(::testing::Return(std::make_shared<MockResourceGroup>()));

    const auto viewCfgValue =
        viewConfig({{"ERP_DARREICHUNGSFORM_1_12", "", "2024-06-30", {"darreichungsform_1_12"}},
                    {"BEGIN_OVERLAPP", "", "2024-05-31", {"darreichungsform_1_13"}},
                    {"ERP_DARREICHUNGSFORM_1_13", "2024-07-01", "2024-09-30", {"darreichungsform_1_13"}},
                    {"MID_OVERLAPP", "2024-09-15", "2024-10-31", {"darreichungsform_1_13"}},
                    {"ERP_DARREICHUNGSFORM_1_14", "2024-10-01", "", {"darreichungsform_1_14"}},
                    {"END_OVERLAPP", "2024-11-15", "", {"darreichungsform_1_14"}}});

    std::optional<fhirtools::FhirResourceViewConfiguration> viewsConfiguration;
    ASSERT_NO_THROW(viewsConfiguration.emplace(res, viewCfgValue, date::days{}));
    EXPECT_EQ(ids(viewsConfiguration->getViewInfo(fhirtools::Date{"1969-01-01"})),
              (strings{"BEGIN_OVERLAPP", "ERP_DARREICHUNGSFORM_1_12"}));
    EXPECT_EQ(ids(viewsConfiguration->getViewInfo(fhirtools::Date{"2024-06-30"})),
              (strings{"ERP_DARREICHUNGSFORM_1_12"}));
    EXPECT_EQ(ids(viewsConfiguration->getViewInfo(fhirtools::Date{"2024-07-01"})),
              (strings{"ERP_DARREICHUNGSFORM_1_13"}));
    EXPECT_EQ(ids(viewsConfiguration->getViewInfo(fhirtools::Date{"2024-09-15"})),
              (strings{"ERP_DARREICHUNGSFORM_1_13", "MID_OVERLAPP"}));
    EXPECT_EQ(ids(viewsConfiguration->getViewInfo(fhirtools::Date{"2024-09-30"})),
              (strings{"ERP_DARREICHUNGSFORM_1_13", "MID_OVERLAPP"}));
    EXPECT_EQ(ids(viewsConfiguration->getViewInfo(fhirtools::Date{"2024-10-01"})),
              (strings{"ERP_DARREICHUNGSFORM_1_14", "MID_OVERLAPP"}));
    EXPECT_EQ(ids(viewsConfiguration->getViewInfo(fhirtools::Date{"2024-10-31"})),
              (strings{"ERP_DARREICHUNGSFORM_1_14", "MID_OVERLAPP"}));
    EXPECT_EQ(ids(viewsConfiguration->getViewInfo(fhirtools::Date{"2024-11-01"})),
              (strings{"ERP_DARREICHUNGSFORM_1_14"}));
    EXPECT_EQ(ids(viewsConfiguration->getViewInfo(fhirtools::Date{"2024-11-14"})),
              (strings{"ERP_DARREICHUNGSFORM_1_14"}));
    EXPECT_EQ(ids(viewsConfiguration->getViewInfo(fhirtools::Date{"2024-11-15"})),
              (strings{"END_OVERLAPP", "ERP_DARREICHUNGSFORM_1_14"}));
    EXPECT_EQ(ids(viewsConfiguration->getViewInfo(fhirtools::Date{"9999-12-31"})),
              (strings{"END_OVERLAPP", "ERP_DARREICHUNGSFORM_1_14"}));
}

TEST_F(FhirResourceViewConfigurationTest, ViewList)
{
    using namespace fhirtools::version_literal;
    using ViewList = fhirtools::FhirResourceViewConfiguration::ViewList;
    using ViewConfig = fhirtools::FhirResourceViewConfiguration::ViewConfig;
    using Date = fhirtools::Date;
    using K = fhirtools::DefinitionKey;

    fhirtools::FhirResourceGroupConst groupResBase{"groupBase"};
    fhirtools::FhirResourceGroupConst groupResA{"groupA"};
    fhirtools::FhirResourceGroupConst groupResB{"groupB"};
    const std::string codeSystemUrl{"http://erp-test.de/versiontest/CodeSystem"};
    const std::string structUrl{"http://erp-test.de/versiontest/StructureDefinition"};

    ::testing::NiceMock<MockResourceGroupResolver> res;
    {
        groupResBase.findGroup("http://hl7.org/fhir/StructureDefinition/string", "0.1"_ver, {});
        groupResA.findGroup(structUrl, "alpha"_ver, {});
        groupResB.findGroup(structUrl, "beta"_ver, {});
        groupResA.findGroup(codeSystemUrl, "A"_ver, {});
        groupResB.findGroup(codeSystemUrl, "B"_ver, {});

        auto groupBase = groupResBase.findGroupById("groupBase");
        auto groupA = groupResA.findGroupById("groupA");
        auto groupB = groupResB.findGroupById("groupB");
        std::map<std::string, std::shared_ptr<const fhirtools::FhirResourceGroup>> allGroups {
            {"groupBase", groupBase},
            {"groupA", groupA},
            {"groupB", groupB},
        };
        using ::testing::Return;
        using ::testing::_;
        using ::testing::Eq;
        using ::testing::StrEq;
        ON_CALL(res, findGroup(_, Eq("0.1"_ver), _)).WillByDefault(Return(groupBase));
        ON_CALL(res, findGroup(_, Eq("0.2"_ver), _)).WillByDefault(Return(groupBase));
        ON_CALL(res, findGroup(_, Eq("alpha"_ver), _)).WillByDefault(Return(groupA));
        ON_CALL(res, findGroup(_, Eq("beta"_ver), _)).WillByDefault(Return(groupB));
        ON_CALL(res, findGroup(_, Eq("A"_ver), _)).WillByDefault(Return(groupA));
        ON_CALL(res, findGroup(_, Eq("B"_ver), _)).WillByDefault(Return(groupB));

        ON_CALL(res, findGroupById("groupBase")).WillByDefault(Return(groupBase));
        ON_CALL(res, findGroupById("groupA")).WillByDefault(Return(groupA));
        ON_CALL(res, findGroupById("groupB")).WillByDefault(Return(groupB));

        ON_CALL(res, allGroups).WillByDefault(Return(allGroups));
    }
    fhirtools::FhirStructureRepositoryBackend backend;
    backend.load({ResourceManager::getAbsoluteFilename("test/fhir-path/samples/version_samples.xml")}, res);

    const auto viewCfgValue = viewConfig({
        {"viewA", "2024-05-01", "2024-08-01", {"groupA", "groupBase"}},
        {"viewB", "2024-07-01", "2024-10-01", {"groupB", "groupBase"}},
    });
    std::optional<fhirtools::FhirResourceViewConfiguration> viewsConfiguration;
    ASSERT_NO_THROW(viewsConfiguration.emplace(res, viewCfgValue, date::days{}));
    std::optional<ViewList> viewListViewA;
    ASSERT_NO_THROW(viewListViewA.emplace(viewsConfiguration->getViewInfo(Date{"2024-06-01"})));
    EXPECT_EQ(viewListViewA->size(), 1);
    {
        auto versions = viewListViewA->supportedVersions(&backend, {structUrl});
        EXPECT_EQ(versions, (std::set{K{structUrl, "alpha"_ver}}));
    }
    {
        const auto match = viewListViewA->match(&backend, "http://hl7.org/fhir/StructureDefinition/string", "0.1"_ver);
        ASSERT_NE(match, nullptr);
        const auto* codeSystem = match->findCodeSystem({codeSystemUrl, std::nullopt});
        ASSERT_NE(codeSystem, nullptr);
        EXPECT_EQ(codeSystem->getVersion(), "A"_ver);
    }
    {
        const auto match = viewListViewA->match(&backend, structUrl, "alpha"_ver);
        ASSERT_NE(match, nullptr);
        const auto* codeSystem = match->findCodeSystem({codeSystemUrl, std::nullopt});
        ASSERT_NE(codeSystem, nullptr);
        EXPECT_EQ(codeSystem->getVersion(), "A"_ver);
    }
    {
        const auto match = viewListViewA->match(&backend, structUrl, "beta"_ver);
        EXPECT_EQ(match, nullptr);
    }
    {
        const ViewConfig* latest = nullptr;
        ASSERT_NO_THROW(latest = std::addressof(viewListViewA->latest()));
        ASSERT_NE(latest, nullptr);
        EXPECT_EQ(latest->mId, "viewA");
        std::shared_ptr<fhirtools::FhirStructureRepository> repoView;
        ASSERT_NO_THROW(repoView = latest->view(&backend));
        ASSERT_NE(repoView, nullptr);
        const auto* codeSystem = repoView->findCodeSystem({codeSystemUrl, std::nullopt});
        ASSERT_NE(codeSystem, nullptr);
        EXPECT_EQ(codeSystem->getVersion(), "A"_ver);
    }

    std::optional<ViewList> viewListViewB;
    ASSERT_NO_THROW(viewListViewB.emplace(viewsConfiguration->getViewInfo(Date{"2024-09-01"})));
    EXPECT_EQ(viewListViewB->size(), 1);
    {
        auto versions = viewListViewB->supportedVersions(&backend, {structUrl});
        EXPECT_EQ(versions, (std::set{K{structUrl, "beta"_ver}}));
    }
    {
        const auto match = viewListViewB->match(&backend, "http://hl7.org/fhir/StructureDefinition/string", "0.1"_ver);
        ASSERT_NE(match, nullptr);
        const auto* codeSystem = match->findCodeSystem({codeSystemUrl, std::nullopt});
        EXPECT_EQ(codeSystem->getVersion(), "B"_ver);
    }
    {
        const auto match = viewListViewB->match(&backend, structUrl, "alpha"_ver);
        ASSERT_EQ(match, nullptr);
    }
    {
        const auto match = viewListViewB->match(&backend, structUrl, "beta"_ver);
        ASSERT_NE(match, nullptr);
        const auto* codeSystem = match->findCodeSystem({codeSystemUrl, std::nullopt});
        EXPECT_EQ(codeSystem->getVersion(), "B"_ver);
    }
    {
        const ViewConfig* latest = nullptr;
        ASSERT_NO_THROW(latest = std::addressof(viewListViewB->latest()));
        ASSERT_NE(latest, nullptr);
        EXPECT_EQ(latest->mId, "viewB");
        std::shared_ptr<fhirtools::FhirStructureRepository> repoView;
        ASSERT_NO_THROW(repoView = latest->view(&backend));
        ASSERT_NE(repoView, nullptr);
        const auto* codeSystem = repoView->findCodeSystem({codeSystemUrl, std::nullopt});
        EXPECT_EQ(codeSystem->getVersion(), "B"_ver);
    }

    std::optional<ViewList> viewListViewAandB;
    ASSERT_NO_THROW(viewListViewAandB.emplace(viewsConfiguration->getViewInfo(Date{"2024-07-15"})));
    EXPECT_EQ(viewListViewAandB->size(), 2);
    {
        auto versions = viewListViewAandB->supportedVersions(&backend, {structUrl});
        EXPECT_EQ(versions, (std::set{K{structUrl, "alpha"_ver}, K{structUrl, "beta"_ver}}));
    }
    {
        const auto match = viewListViewAandB->match(&backend, "http://hl7.org/fhir/StructureDefinition/string", "0.1"_ver);
        ASSERT_NE(match, nullptr);
        const auto* codeSystem = match->findCodeSystem({codeSystemUrl, std::nullopt});
        // Base is in both views so we expect to get the latest which is viewB
        EXPECT_EQ(codeSystem->getVersion(), "B"_ver);
    }
    {
        const auto match = viewListViewAandB->match(&backend, structUrl, "alpha"_ver);
        ASSERT_NE(match, nullptr);
        const auto* codeSystem = match->findCodeSystem({codeSystemUrl, std::nullopt});
        EXPECT_EQ(codeSystem->getVersion(), "A"_ver);
    }
    {
        const auto match = viewListViewAandB->match(&backend, structUrl, "beta"_ver);
        ASSERT_NE(match, nullptr);
        const auto* codeSystem = match->findCodeSystem({codeSystemUrl, std::nullopt});
        EXPECT_EQ(codeSystem->getVersion(), "B"_ver);
    }
    {
        const ViewConfig* latest = nullptr;
        ASSERT_NO_THROW(latest = std::addressof(viewListViewAandB->latest()));
        ASSERT_NE(latest, nullptr);
        EXPECT_EQ(latest->mId, "viewB");
        std::shared_ptr<fhirtools::FhirStructureRepository> repoView;
        ASSERT_NO_THROW(repoView = latest->view(&backend));
        ASSERT_NE(repoView, nullptr);
        const auto* codeSystem = repoView->findCodeSystem({codeSystemUrl, std::nullopt});
        EXPECT_EQ(codeSystem->getVersion(), "B"_ver);
    }

    std::optional<ViewList> viewListEmpty;
    ASSERT_NO_THROW(viewListEmpty.emplace(viewsConfiguration->getViewInfo(Date{"2024-12-01"})));
    EXPECT_TRUE(viewListEmpty->empty());
}


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
    MockResourceGroupResolver res;
    EXPECT_CALL(res, findGroupById(::testing::StrEq("darreichungsform_1_12")))
        .WillRepeatedly(::testing::Return(std::make_shared<MockResourceGroup>()));

    const auto viewCfgValue = viewConfig(
        {{"ERP_DARREICHUNGSFORM_1_12", GetParam().validFrom, GetParam().validUntil, {"darreichungsform_1_12"}}});

    std::optional<fhirtools::FhirResourceViewConfiguration> viewCfg;
    try
    {
        viewCfg.emplace(res, viewCfgValue, date::days{});
        FAIL() << "Expected exception on view inconsistency.";
    }
    catch (const std::runtime_error& re)
    {
        EXPECT_EQ(std::string{re.what()},
                  (std::ostringstream{} << "inconsistent view: "
                                        << "ERP_DARREICHUNGSFORM_1_12 Start: " << GetParam().validFrom
                                        << " End: " << GetParam().validUntil)
                      .str());
    }
    std::cout << fhirtools::Date{};
}

INSTANTIATE_TEST_SUITE_P(inconsistent, FhirResourceViewConfigurationTestInconsistent,
                         testing::Values(InconsistentParam{"2024-01-12", "2024-01-11"}));
