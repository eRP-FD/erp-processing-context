#ifndef ERP_TEST_FHIR_TOOLS_SAMPLEVALIDATION_HXX
#define ERP_TEST_FHIR_TOOLS_SAMPLEVALIDATION_HXX


#include "fhirtools/converter/internal/FhirSAXHandler.hxx"
#include "fhirtools/model/NumberAsStringParserDocument.hxx"
#include "fhirtools/model/erp/ErpElement.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "fhirtools/repository/groups/FhirResourceGroupConst.hxx"
#include "fhirtools/repository/views/FhirResourceViewGroupSet.hxx"
#include "fhirtools/repository/views/FhirStructureRepositoryView.hxx"
#include "fhirtools/validator/FhirPathValidator.hxx"
#include "fhirtools/validator/ValidationResult.hxx"
#include "test/util/ResourceManager.hxx"

#include <gtest/gtest.h>
#include <shared/fhir/Fhir.hxx>
#include <filesystem>
#include <memory>
#include <ranges>
namespace fhirtools::test
{

class Sample
{
public:
    std::string_view type;
    std::string_view filename;
    std::list<ValidationError> expect{};
    std::set<std::tuple<std::string_view, std::string_view>> expectedConstraintKeyElement{};
    static std::string name(const ::testing::TestParamInfo<Sample>& info);
};

inline std::ostream& operator<<(std::ostream& out, const Sample& samp)
{
    out << samp.filename;
    return out;
}

inline std::string Sample::name(const ::testing::TestParamInfo<Sample>& info)
{
    return std::filesystem::path{info.param.filename}.stem();
}

template<typename BaseT>
class SampleValidationTest : public ::testing::TestWithParam<Sample>
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
            // clang-format on
        };
    }
    void SetUp() override
    {
        ASSERT_NO_THROW((void) repo());
    }
    [[nodiscard]] static gsl::not_null<const FhirStructureRepositoryBackend*> backend()
    {
        static std::unique_ptr instance = BaseT::makeBackend();
        return std::to_address(instance);
    }
    [[nodiscard]] const FhirStructureRepositoryView& repo()
    {
        static std::shared_ptr instance = BaseT::makeRepo(*BaseT::backend());
        return *instance;
    }

protected:
    virtual ValidatorOptions validatorOptions()
    {
        return {};
    }

    void run()
    {
        auto fileContent = resourceManager.getStringResource(GetParam().filename);
        auto startPos = fileContent.find_first_of("<{");
        ASSERT_NE(startPos, std::string::npos);
        std::optional<model::NumberAsStringParserDocument> doc;
        if (fileContent[startPos] == '<')
        {
            doc.emplace(FhirSaxHandler::parseXMLintoJSON(repo(), fileContent, nullptr));
        }
        else
        {
            doc.emplace(model::NumberAsStringParserDocument::fromJson(fileContent));
        }
        auto rootElement = std::make_shared<ErpElement>(backend(), std::weak_ptr<const ErpElement>{},
                                                        std::string{GetParam().type}, &doc.value());
        auto validationResult = FhirPathValidator::validate(repo().shared_from_this(), {rootElement},
                                                            std::string{GetParam().type}, validatorOptions())
                                    .results();

        for (const auto& expected : GetParam().expect)
        {
            ASSERT_GE(expected.severity(), Severity::info)
                << "Don't define `debug` assertions. They are not compiled into Release-like builds.";
            auto isExpected = [&](ValidationError ve) {
                ve.profile = nullptr;
                return ve == expected;
            };
            EXPECT_GE(std::erase_if(validationResult, isExpected), 1) << expected;
        }
        for (const auto& expected : GetParam().expectedConstraintKeyElement)
        {
            bool found{};
            for (;;)
            {
                auto err = std::ranges::find_if(validationResult, [&](const ValidationError& ve) {
                    return holds_alternative<FhirConstraint>(ve.reason) && ve.fieldName == get<1>(expected) &&
                           std::get<FhirConstraint>(ve.reason).getKey() == get<0>(expected);
                });
                if (err == validationResult.end())
                {
                    break;
                }
                validationResult.erase(err);
                found = true;
            }
            EXPECT_TRUE(found) << get<1>(expected) << ": " << get<0>(expected);
        }

        ValidationResults reducedList;
        reducedList.merge(std::move(validationResult));
        EXPECT_LE(reducedList.highestSeverity(), Severity::debug);
        if (reducedList.highestSeverity() > Severity::debug)
        {
            TVLOG(0) << "Remaining unexpected results contain results with severity higher than debug";
            reducedList.dumpToLog();
        }
    }
    ResourceManager& resourceManager = ResourceManager::instance();

private:
    static std::unique_ptr<fhirtools::FhirStructureRepositoryBackend> makeBackend()
    {
        auto result = std::make_unique<fhirtools::FhirStructureRepositoryBackend>();
        std::list<std::filesystem::path> files = BaseT::fileList();
        std::list<std::filesystem::path> absolutfiles;
        std::ranges::transform(files, std::back_inserter(absolutfiles), &ResourceManager::getAbsoluteFilename);
        FhirResourceGroupConst resolver{"test"};
        result->load(absolutfiles, resolver);
        return result;
    }

    static std::shared_ptr<FhirStructureRepositoryView>
    makeRepo(const fhirtools::FhirStructureRepositoryBackend& backend)
    {
        std::set<gsl::not_null<std::shared_ptr<const FhirResourceGroup>>> allGroups;
        std::ranges::copy(backend.allGroups() | std::views::values, inserter(allGroups, allGroups.begin()));
        return FhirResourceViewGroupSet::create("test", std::move(allGroups), &backend);
    }
};
}

#endif// ERP_TEST_FHIR_TOOLS_SAMPLEVALIDATION_HXX
