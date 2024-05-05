
#include "erp/model/NumberAsStringParserDocument.hxx"
#include "fhirtools/model/erp/ErpElement.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "fhirtools/validator/FhirPathValidator.hxx"
#include "test/fhirtools/DefaultFhirStructureRepository.hxx"
#include "test/fhirtools/SampleValidation.hxx"
#include "test/util/ResourceManager.hxx"

#include <gtest/gtest.h>
#include <algorithm>


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
    FhirStructureRepositoryBackend repo;
};

class SlicingSampleValidationTest : public test::SampleValidationTest<SlicingSampleValidationTest>
{
public:
    static std::list<std::filesystem::path> fileList()
    {
        auto result = SampleValidationTest::fileList();
        result.merge({
            // clang-format off
        "test/fhir-path/slicing/profiles/slicing_base.xml",
        "test/fhir-path/slicing/profiles/slice_by_pattern.xml",
        "test/fhir-path/slicing/profiles/slice_by_fixed.xml",
        "test/fhir-path/slicing/profiles/slice_by_exists.xml",
        "test/fhir-path/slicing/profiles/ordered_slice.xml",
        "test/fhir-path/slicing/profiles/slice_by_profile.xml",
        "test/fhir-path/slicing/profiles/slice_by_profile_field_resolve.xml",
        "test/fhir-path/slicing/profiles/slice_by_profile_resolve.xml",
        "test/fhir-path/slicing/profiles/slice_by_profile_resolve_field.xml",
        "test/fhir-path/slicing/profiles/slice_by_primitve_binding.xml",
        "test/fhir-path/slicing/profiles/test_value_set.xml",
        "test/fhir-path/slicing/profiles/test_code_system.xml",
        "test/fhir-path/slicing/profiles/slice_by_coding_binding.xml",
        "test/fhir-path/slicing/profiles/slice_by_codeable_concept_binding.xml",
            // clang-format on
        });
        return result;
    };
};

TEST(SlicingRepoValidationTest, invalid_duplicate_fixed)//NOLINT
{
    FhirStructureRepositoryBackend repo;

    auto profiles = DefaultFhirStructureRepository::defaultProfileFiles();
    profiles.merge(
        {ResourceManager::getAbsoluteFilename("test/fhir-path/slicing/profiles/invalid_duplicate_fixed.xml")});
    ASSERT_THROW(repo.load(profiles), std::logic_error);
}

TEST(SlicingRepoValidationTest, invalid_duplicate_pattern)//NOLINT
{
    FhirStructureRepositoryBackend repo;

    auto profiles = DefaultFhirStructureRepository::defaultProfileFiles();
    profiles.merge(
        {ResourceManager::getAbsoluteFilename("test/fhir-path/slicing/profiles/invalid_duplicate_pattern.xml")});
    ASSERT_THROW(repo.load(profiles), std::logic_error);
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
                {std::make_tuple(Severity::warning, "Can not validate CodeSystem http://erp.test/slice2/code, it has not been loaded."), "Sliceable.sliced[1].smallstructure.coding"},
                {std::make_tuple(Severity::warning, "Can not validate CodeSystem http://erp.test/slice1/code, it has not been loaded."), "Sliceable.sliced[0].smallstructure.coding"},
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
        },
    Sample{"Bundle", "test/fhir-path/slicing/samples/valid_slice_by_profile_field_resolve.json",
            {
                {std::make_tuple(Severity::debug, "detected slice: slice1"),
                        "Bundle.entry[0].resource{SliceByProfileFieldResolveBase}.sliced[0]"},
                {std::make_tuple(Severity::debug, "detected slice: slice2"),
                        "Bundle.entry[0].resource{SliceByProfileFieldResolveBase}.sliced[1]"},
            }
        },
    Sample{"Bundle", "test/fhir-path/slicing/samples/valid_slice_by_profile_resolve.json",
            {
                {std::make_tuple(Severity::debug, "detected slice: slice1"),
                        "Bundle.entry[0].resource{SliceByProfileResolveBase}.subReference[0]"},
                {std::make_tuple(Severity::debug, "detected slice: slice2"),
                        "Bundle.entry[0].resource{SliceByProfileResolveBase}.subReference[1]"},
            }
        },
    Sample{"Bundle", "test/fhir-path/slicing/samples/valid_slice_by_profile_resolve_field.json",
            {
                {std::make_tuple(Severity::debug, "detected slice: slice1"),
                        "Bundle.entry[0].resource{SliceByProfileResolveFieldBase}.subReference[0]"},
                {std::make_tuple(Severity::debug, "detected slice: slice2"),
                        "Bundle.entry[0].resource{SliceByProfileResolveFieldBase}.subReference[1]"},
            }
        },
    Sample{"Sliceable", "test/fhir-path/slicing/samples/valid_slice_by_primitive_binding.json",
            {
                {std::make_tuple(Severity::debug, "detected slice: slice"), "Sliceable.sliced[0]"},
                {std::make_tuple(Severity::debug, "detected slice: slice"), "Sliceable.sliced[2]"},
            }
        },
    Sample{"Sliceable", "test/fhir-path/slicing/samples/valid_slice_by_coding_binding.json",
            {
                {std::make_tuple(Severity::debug, "detected slice: slice"), "Sliceable.sliced[0]"},
                {std::make_tuple(Severity::debug, "detected slice: slice"), "Sliceable.sliced[2]"},
                {std::make_tuple(Severity::error, "Code WrongCode is not part of CodeSystem http://erp.test/TestCodeSystem"), "Sliceable.sliced[3].smallstructure.coding"},
                {std::make_tuple(Severity::warning, "Can not validate CodeSystem http://erp.test/WrongCodeSystem, it has not been loaded."), "Sliceable.sliced[1].smallstructure.coding"},
            }
        },
    Sample{"Sliceable", "test/fhir-path/slicing/samples/valid_slice_by_codeable_concept_binding.json",
            {
                {std::make_tuple(Severity::debug, "detected slice: slice"), "Sliceable.sliced[0]"},
                {std::make_tuple(Severity::debug, "detected slice: slice"), "Sliceable.sliced[2]"},
                {std::make_tuple(Severity::error, "Code WrongCode is not part of CodeSystem http://erp.test/TestCodeSystem"), "Sliceable.sliced[1].smallstructure.codeableConcept.coding[1]"},
                {std::make_tuple(Severity::warning, "Can not validate CodeSystem http://erp.test/WrongCodeSystem, it has not been loaded."), "Sliceable.sliced[3].smallstructure.codeableConcept.coding[0]"},
            }
        }

));
// clang-format on
