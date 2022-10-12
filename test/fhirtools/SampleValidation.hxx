#ifndef ERP_TEST_FHIR_TOOLS_SAMPLEVALIDATION_HXX
#define ERP_TEST_FHIR_TOOLS_SAMPLEVALIDATION_HXX


#include "erp/model/NumberAsStringParserDocument.hxx"
#include "test/util/ResourceManager.hxx"

#include <gtest/gtest.h>
#include <filesystem>
#include <memory>
#include <ranges>

#include "fhirtools/model/erp/ErpElement.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "fhirtools/validator/FhirPathValidator.hxx"
#include "fhirtools/validator/ValidationResult.hxx"
#include <erp/fhir/Fhir.hxx>
namespace fhirtools::test
{

class Sample
{
public:
    std::string_view type;
    std::string_view filename;
    std::list<ValidationError> expect{};
    std::set<std::tuple<std::string_view, std::string_view>> expectedConstraintKeyElement{};
};

inline std::ostream& operator<<(std::ostream& out, const Sample& samp)
{
    out << samp.filename;
    return out;
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
    [[nodiscard]] const FhirStructureRepository& repo()
    {
        static auto instance = makeRepo();
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
            doc.emplace(Fhir::instance().converter().xmlStringToJson(fileContent));
        }
        else
        {
            doc.emplace(model::NumberAsStringParserDocument::fromJson(fileContent));
        }
        auto rootElement = std::make_shared<ErpElement>(&repo(), std::weak_ptr<const ErpElement>{},
                                                        std::string{GetParam().type}, &doc.value());
        auto validationResult =
            FhirPathValidator::validate({rootElement}, std::string{GetParam().type}, validatorOptions()).results();

        for (const auto& expected : GetParam().expect)
        {
            auto isExpected = [&](ValidationError ve) {
                ve.profile = nullptr;
                return ve == expected;
            };
            EXPECT_GE(validationResult.remove_if(isExpected), 1) << expected;
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

        ValidationResultList reducedList;
        reducedList.append(std::move(validationResult));
        EXPECT_LE(reducedList.highestSeverity(), Severity::debug);
        if (reducedList.highestSeverity() > Severity::debug)
        {
            TVLOG(0) << "Remaining unexpected results contain results with severity higher than debug";
            reducedList.dumpToLog();
        }
    }
    ResourceManager& resourceManager = ResourceManager::instance();

private:
    static std::unique_ptr<FhirStructureRepository> makeRepo()
    {
        auto result = std::make_unique<FhirStructureRepository>();
        std::list<std::filesystem::path> files = BaseT::fileList();
        std::list<std::filesystem::path> absolutfiles;
        std::ranges::transform(files, std::back_inserter(absolutfiles), &ResourceManager::getAbsoluteFilename);
        result->load(absolutfiles);
        return result;
    }
};
}

#endif // ERP_TEST_FHIR_TOOLS_SAMPLEVALIDATION_HXX
