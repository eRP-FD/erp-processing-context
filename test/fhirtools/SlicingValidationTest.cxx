
#include "erp/model/NumberAsStringParserDocument.hxx"
#include "test/fhirtools/SampleValidation.hxx"
#include "test/util/ResourceManager.hxx"

#include <gtest/gtest.h>
#include <algorithm>

#include "fhirtools/model/erp/ErpElement.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "fhirtools/validator/FhirPathValidator.hxx"

using namespace fhirtools;

class SlicingValidationTest : public ::testing::Test
{
public:
    void load(std::list<std::filesystem::path> c)
    {
        std::list<std::filesystem::path> files;
        std::ranges::transform(c, std::back_inserter(files), [&](const std::filesystem::path& filename) {
            return ResourceManager::getAbsoluteFilename(filename);
        });
        repo.load(files);
    }
    FhirStructureRepository repo;
};

class SlicingSampleValidationTest : public test::SampleValidationTest<SlicingSampleValidationTest>
{
public:

    static std::list<std::filesystem::path> fileList() {
        auto result = SampleValidationTest::fileList();
        result.merge({
        // clang-format off
        "test/fhir-path/slicing/profiles/slicing_base.xml",
        "test/fhir-path/slicing/profiles/slice_by_pattern.xml",
        "test/fhir-path/slicing/profiles/slice_by_fixed.xml",
        "test/fhir-path/slicing/profiles/slice_by_exists.xml",
        "test/fhir-path/slicing/profiles/ordered_slice.xml",
        "test/fhir-path/slicing/profiles/slice_by_profile.xml"});
        // clang-format on
        return result;
    };
};

TEST_F(SlicingValidationTest, minibundle)//NOLINT
{
    ASSERT_NO_THROW(load({"fhir/hl7.org/profiles-types.xml", "test/fhir-path/slicing/profiles/valid_minibundle.xml"}));
}

TEST_F(SlicingValidationTest, invalid_duplicate)//NOLINT
{
    ASSERT_THROW(load({"fhir/hl7.org/profiles-types.xml", "test/fhir-path/slicing/profiles/invalid_duplicate_fixed.xml"}),
                 std::logic_error);
    ASSERT_THROW(load({"fhir/hl7.org/profiles-types.xml", "test/fhir-path/slicing/profiles/invalid_duplicate_pattern.xml"}),
                 std::logic_error);
}

TEST_P(SlicingSampleValidationTest, run)//NOLINT
{
    ASSERT_NO_FATAL_FAILURE(run());
}
using Sample = fhirtools::test::Sample;
// clang-format off
INSTANTIATE_TEST_SUITE_P( samples, SlicingSampleValidationTest, ::testing::Values(
    Sample{"Sliceable", "test/fhir-path/slicing/samples/valid_slice_by_pattern.json",
            {
                {std::make_tuple(Severity::debug, "detected slice: slice1"), "Sliceable.sliced[0]"},
                {std::make_tuple(Severity::debug, "detected slice: slice2"), "Sliceable.sliced[1]"},
            }
        },
    Sample{"Sliceable", "test/fhir-path/slicing/samples/valid_slice_by_fixed.json",
            {
                {std::make_tuple(Severity::debug, "detected slice: slice1"), "Sliceable.sliced[0]"},
                {std::make_tuple(Severity::debug, "detected slice: slice2"), "Sliceable.sliced[1]"},
            }
        },
    Sample{"Sliceable", "test/fhir-path/slicing/samples/valid_slice_by_exists.json",
            {
                {std::make_tuple(Severity::debug, "detected slice: slice1"), "Sliceable.sliced[0]"},
                {std::make_tuple(Severity::debug, "detected slice: slice2"), "Sliceable.sliced[1]"},
            }
        },
    Sample{"Sliceable", "test/fhir-path/slicing/samples/valid_slice_ordered.json",
            {
                {std::make_tuple(Severity::debug, "detected slice: slice1"), "Sliceable.sliced[0]"},
                {std::make_tuple(Severity::debug, "detected slice: slice2"), "Sliceable.sliced[1]"},
            }
        },
    Sample{"Sliceable", "test/fhir-path/slicing/samples/invalid_slice_ordered.json",
            {
                {std::make_tuple(Severity::debug, "detected slice: slice2"), "Sliceable.sliced[0]"},
                {std::make_tuple(Severity::debug, "detected slice: slice1"), "Sliceable.sliced[1]"},
                {std::make_tuple(Severity::error, "slicing out of order"), "Sliceable.sliced[1]"}
            }
        },
    Sample{"SliceByProfileBase", "test/fhir-path/slicing/samples/valid_slice_by_profile.json",
            {
                {std::make_tuple(Severity::debug, "detected slice: slice1"), "SliceByProfileBase.sub[0]{Resource}"},
                {std::make_tuple(Severity::debug, "detected slice: slice2"), "SliceByProfileBase.sub[1]{Resource}"},
                {std::make_tuple(Severity::debug, "resource is: Resource"), "SliceByProfileBase.sub[0]{Resource}"},
                {std::make_tuple(Severity::debug, "resource is: Resource"), "SliceByProfileBase.sub[1]{Resource}"},
            }
        }
));
// clang-format on
