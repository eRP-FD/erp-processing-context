/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */
#include "fhirtools/repository/FhirCodeSystem.hxx"
#include "fhirtools/repository/FhirValueSet.hxx"
#include "fhirtools/repository/groups/FhirResourceGroupConst.hxx"
#include "fhirtools/repository/views/CodeCachingView.hxx"
#include "test/fhirtools/MockFhirResourceView.hxx"


using fhirtools::test::MockFhirResourceView;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::Return;


TEST(CodeCachingViewUndefTest, dontCacheUndefinedCodeSystem)
{
    using namespace fhirtools::version_literal;
    fhirtools::DefinitionKey testKey{"http://erp.test/CodeSystem/not-existing", "0.1"_ver};
    fhirtools::DefinitionKey testKeyNoVer{testKey.url, std::nullopt};
    auto baseView = MockFhirResourceView::create();
    EXPECT_CALL(*baseView, findCodeSystemCodes(Eq(testKey))).Times(2).WillRepeatedly(Return(nullptr));
    EXPECT_CALL(*baseView, findCodeSystemCodes(Eq(testKeyNoVer))).Times(2).WillRepeatedly(Return(nullptr));
    const auto cachedView = fhirtools::CodeCachingView::create("cached", baseView);
    EXPECT_EQ(cachedView->findCodeSystemCodes(testKey), nullptr);
    EXPECT_EQ(cachedView->findCodeSystemCodes(testKeyNoVer), nullptr);
    EXPECT_EQ(cachedView->findCodeSystemCodes(testKey), nullptr);
    EXPECT_EQ(cachedView->findCodeSystemCodes(testKeyNoVer), nullptr);
    testing::Mock::VerifyAndClearExpectations(baseView.get());
}

TEST(CodeCachingViewUndefTest, dontCacheUndefinedValueSet)
{
    using namespace fhirtools::version_literal;
    fhirtools::DefinitionKey testKey{"http://erp.test/ValueSet/not-existing", "0.1"_ver};
    fhirtools::DefinitionKey testKeyNoVer{testKey.url, std::nullopt};
    auto baseView = MockFhirResourceView::create();
    EXPECT_CALL(*baseView, findValueSetCodes(Eq(testKey))).Times(2).WillRepeatedly(Return(nullptr));
    EXPECT_CALL(*baseView, findValueSetCodes(Eq(testKeyNoVer))).Times(2).WillRepeatedly(Return(nullptr));
    const auto cachedView = fhirtools::CodeCachingView::create("cached", baseView);
    EXPECT_EQ(cachedView->findValueSetCodes(testKey), nullptr);
    EXPECT_EQ(cachedView->findValueSetCodes(testKeyNoVer), nullptr);
    EXPECT_EQ(cachedView->findValueSetCodes(testKey), nullptr);
    EXPECT_EQ(cachedView->findValueSetCodes(testKeyNoVer), nullptr);
    testing::Mock::VerifyAndClearExpectations(baseView.get());
}


class CodeCachingViewTest : public ::testing::Test
{
public:
    static inline const fhirtools::DefinitionKey codeSystemNoVerKey{"http://erp.test/CodeSystem/test", std::nullopt};
    static inline const fhirtools::DefinitionKey codeSystemV1Key{"http://erp.test/CodeSystem/test",
                                                                 fhirtools::FhirVersion{"1"}};
    static inline const fhirtools::DefinitionKey codeSystemV2Key{"http://erp.test/CodeSystem/test",
                                                                 fhirtools::FhirVersion{"2"}};
    static inline const fhirtools::DefinitionKey valueSetNoVerKey{"http://erp.test/ValueSet/test", std::nullopt};
    static inline const fhirtools::DefinitionKey valueSetV1Key{"http://erp.test/ValueSet/test",
                                                               fhirtools::FhirVersion{"1"}};
    static inline const fhirtools::DefinitionKey valueSetV2Key{"http://erp.test/ValueSet/test",
                                                               fhirtools::FhirVersion{"2"}};

    std::shared_ptr<fhirtools::FhirCodeSystem> codeSystemV1;
    std::shared_ptr<fhirtools::FhirCodeSystem> codeSystemV2;
    std::shared_ptr<fhirtools::FhirValueSet> valueSetV1;
    std::shared_ptr<fhirtools::FhirValueSet> valueSetV2;


    std::shared_ptr<MockFhirResourceView> baseView;

    void SetUp() override
    {
        fhirtools::FhirResourceGroupConst resolver1{"test1"};
        fhirtools::FhirResourceGroupConst resolver2{"test2"};
        fhirtools::FhirCodeSystem::Builder codeSystemBuilder;
        codeSystemBuilder.url(codeSystemV1Key.url).version(codeSystemV1Key.version.value());
        codeSystemBuilder.code("CodeSystemV1Code1").popConcept();
        codeSystemBuilder.code("CodeSystemV1Code2").popConcept();
        codeSystemBuilder.initGroup(resolver1);
        codeSystemV1 = std::make_shared<fhirtools::FhirCodeSystem>(codeSystemBuilder.getAndReset());
        codeSystemBuilder.url(codeSystemV2Key.url).version(codeSystemV2Key.version.value());
        codeSystemBuilder.code("CodeSystemV2Code1").popConcept();
        codeSystemBuilder.code("CodeSystemV2Code2").popConcept();
        codeSystemBuilder.initGroup(resolver2);
        codeSystemV2 = std::make_shared<fhirtools::FhirCodeSystem>(codeSystemBuilder.getAndReset());
        fhirtools::FhirValueSet::Builder valueSetBuilder;
        valueSetBuilder.url(valueSetV1Key.url).version(valueSetV1Key.version.value());
        valueSetBuilder.include();
        valueSetBuilder.includeCodeSystem(codeSystemV1Key);
        valueSetBuilder.initGroup(resolver1);
        valueSetV1 = std::make_shared<fhirtools::FhirValueSet>(valueSetBuilder.getAndReset());
        valueSetBuilder.url(valueSetV2Key.url).version(valueSetV2Key.version.value());
        valueSetBuilder.include();
        valueSetBuilder.includeCodeSystem(codeSystemV2Key);
        valueSetBuilder.initGroup(resolver2);
        valueSetV2 = std::make_shared<fhirtools::FhirValueSet>(valueSetBuilder.getAndReset());

        baseView = MockFhirResourceView::create();
        ON_CALL(*baseView, findCodeSystemCodes(Eq(codeSystemNoVerKey))).WillByDefault(Invoke([&]() {
            return std::make_shared<const fhirtools::FhirCodeSystemCodes>(codeSystemV2->getCodes(*baseView));
        }));
        ON_CALL(*baseView, findCodeSystemCodes(Eq(codeSystemV1Key))).WillByDefault(Invoke([&]() {
            return std::make_shared<const fhirtools::FhirCodeSystemCodes>(codeSystemV1->getCodes(*baseView));
        }));
        ON_CALL(*baseView, findCodeSystemCodes(Eq(codeSystemV2Key))).WillByDefault(Invoke([&]() {
            return std::make_shared<const fhirtools::FhirCodeSystemCodes>(codeSystemV2->getCodes(*baseView));
        }));
        ON_CALL(*baseView, findValueSetCodes(Eq(valueSetNoVerKey))).WillByDefault(Invoke([&]() {
            return fhirtools::FhirValueSetCodes::create(baseView.get(), valueSetV2.get());
        }));
        ON_CALL(*baseView, findValueSetCodes(Eq(valueSetV1Key))).WillByDefault(Invoke([&]() {
            return fhirtools::FhirValueSetCodes::create(baseView.get(), valueSetV1.get());
        }));
        ON_CALL(*baseView, findValueSetCodes(Eq(valueSetV2Key))).WillByDefault(Invoke([&]() {
            return fhirtools::FhirValueSetCodes::create(baseView.get(), valueSetV2.get());
        }));
        ON_CALL(*baseView, findSupplementers).WillByDefault(Return(std::set<const fhirtools::FhirCodeSystem*>{}));
        EXPECT_CALL(*baseView, findSupplementers).Times(::testing::AnyNumber());
    }
};

TEST_F(CodeCachingViewTest, singleVersionCodeSystem)
{
    const auto cachedView = fhirtools::CodeCachingView::create("cached", baseView);
    EXPECT_CALL(*baseView, findCodeSystemCodes).Times(1);
    {
        auto codes = cachedView->findCodeSystemCodes(codeSystemV1Key);
        ASSERT_NE(codes, nullptr);
        EXPECT_EQ(codes->key(), codeSystemV1Key);
    }
    {
        auto codes = cachedView->findCodeSystemCodes(codeSystemV1Key);
        ASSERT_NE(codes, nullptr);
        EXPECT_EQ(codes->key(), codeSystemV1Key);
    }
    testing::Mock::VerifyAndClearExpectations(baseView.get());
}

TEST_F(CodeCachingViewTest, multiVersionCodeSystem)
{
    const auto cachedView = fhirtools::CodeCachingView::create("cached", baseView);
    EXPECT_CALL(*baseView, findCodeSystemCodes).Times(::testing::AnyNumber());
    {
        auto codes = cachedView->findCodeSystemCodes(codeSystemV1Key);
        ASSERT_NE(codes, nullptr);
        EXPECT_EQ(codes->key(), codeSystemV1Key);
    }
    {
        auto codes = cachedView->findCodeSystemCodes(codeSystemV2Key);
        ASSERT_NE(codes, nullptr);
        EXPECT_EQ(codes->key(), codeSystemV2Key);
    }
    testing::Mock::VerifyAndClearExpectations(baseView.get());
}


TEST_F(CodeCachingViewTest, cacheCodeSystemFullKey)
{
    const auto cachedView = fhirtools::CodeCachingView::create("cached", baseView);
    EXPECT_CALL(*baseView, findCodeSystemCodes).Times(1);
    {
        auto codes = cachedView->findCodeSystemCodes(codeSystemNoVerKey);
        ASSERT_NE(codes, nullptr);
        EXPECT_EQ(codes->key(), codeSystemV2Key);
    }
    {
        auto codes = cachedView->findCodeSystemCodes(codeSystemV2Key);
        ASSERT_NE(codes, nullptr);
        EXPECT_EQ(codes->key(), codeSystemV2Key);
    }
    testing::Mock::VerifyAndClearExpectations(baseView.get());
}


TEST_F(CodeCachingViewTest, singleVersionValueSet)
{
    EXPECT_CALL(*baseView, findCodeSystemCodes).Times(::testing::AnyNumber());
    const auto cachedView = fhirtools::CodeCachingView::create("cached", baseView);
    EXPECT_CALL(*baseView, findValueSetCodes).Times(1);
    {
        auto codes = cachedView->findValueSetCodes(valueSetV1Key);
        ASSERT_NE(codes, nullptr);
        EXPECT_EQ(codes->key(), valueSetV1Key);
    }
    {
        auto codes = cachedView->findValueSetCodes(valueSetV1Key);
        ASSERT_NE(codes, nullptr);
        EXPECT_EQ(codes->key(), valueSetV1Key);
    }
    testing::Mock::VerifyAndClearExpectations(baseView.get());
}

TEST_F(CodeCachingViewTest, multiVersionValueSet)
{
    EXPECT_CALL(*baseView, findCodeSystemCodes).Times(::testing::AnyNumber());
    const auto cachedView = fhirtools::CodeCachingView::create("cached", baseView);
    EXPECT_CALL(*baseView, findValueSetCodes).Times(::testing::AnyNumber());
    {
        auto codes = cachedView->findValueSetCodes(valueSetV1Key);
        ASSERT_NE(codes, nullptr);
        EXPECT_EQ(codes->key(), valueSetV1Key);
    }
    {
        auto codes = cachedView->findValueSetCodes(valueSetV2Key);
        ASSERT_NE(codes, nullptr);
        EXPECT_EQ(codes->key(), valueSetV2Key);
    }
    testing::Mock::VerifyAndClearExpectations(baseView.get());
}


TEST_F(CodeCachingViewTest, cacheValueSetFullKey)
{
    EXPECT_CALL(*baseView, findCodeSystemCodes).Times(::testing::AnyNumber());
    const auto cachedView = fhirtools::CodeCachingView::create("cached", baseView);
    EXPECT_CALL(*baseView, findValueSetCodes).Times(1);
    {
        auto codes = cachedView->findValueSetCodes(valueSetNoVerKey);
        ASSERT_NE(codes, nullptr);
        EXPECT_EQ(codes->key(), valueSetV2Key);
    }
    {
        auto codes = cachedView->findValueSetCodes(valueSetV2Key);
        ASSERT_NE(codes, nullptr);
        EXPECT_EQ(codes->key(), valueSetV2Key);
    }
    testing::Mock::VerifyAndClearExpectations(baseView.get());
}
