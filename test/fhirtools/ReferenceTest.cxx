/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#include "erp/model/NumberAsStringParserDocument.hxx"
#include "erp/model/ResourceNames.hxx"
#include "fhirtools/model/erp/ErpElement.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "fhirtools/validator/FhirPathValidator.hxx"
#include "fhirtools/validator/internal/ReferenceContext.hxx"
#include "fhirtools/validator/internal/ReferenceFinder.hxx"
#include "test/fhirtools/DefaultFhirStructureRepository.hxx"
#include "test/util/ResourceManager.hxx"

#include <gtest/gtest.h>
#include <regex>

using namespace fhirtools;
using AnchorType = ReferenceContext::AnchorType;
namespace
{
struct Sample {
    struct Expected {
        std::string elementFullPath;
        std::string targetIdentity;
        bool mustBeResolvable = false;
        std::set<std::string> profileUrls;
        std::set<const FhirStructureDefinition*> profiles{};
        bool found = false;
    };
    std::filesystem::path file;
    std::list<Expected> expected;
};

std::ostream& operator<<(std::ostream& out, const Sample& sample)
{
    out << sample.file;
    return out;
}

std::ostream& operator<<(std::ostream& out, const Sample::Expected& expected)
{
    out << R"({ "elementFullPath": ")" << expected.elementFullPath << '"';
    out << R"(, "targetUrl": ")" << expected.targetIdentity << '"';
    out << R"(, "profileUrl": [)";
    std::string_view sep;
    for (const auto& url : expected.profileUrls)
    {
        out << sep << '"' << url << '"';
        sep = ", ";
    }
    out << "]}";
    return out;
}

class PrintProfileUrls
{
public:
    PrintProfileUrls(const std::set<const FhirStructureDefinition*>& profiles)
        : mProfiles{profiles}
    {
    }

    const std::set<const FhirStructureDefinition*>& mProfiles;
};

std::ostream& operator<<(std::ostream& out, const PrintProfileUrls& print)
{
    const auto& profiles = print.mProfiles;
    out << '[';
    std::string_view sep;
    for (const auto* profile : profiles)
    {
        out << sep << '"' << profile->url() << '|' << profile->version() << '"';
        sep = ", ";
    }
    out << ']';
    return out;
}
}// anonymous namespace

class ReferenceTest : public ::testing::Test
{
protected:
    std::shared_ptr<ErpElement> loadResource(const std::filesystem::path& resourcePath)
    {
        using namespace model::resource;
        using JsonDoc = model::NumberAsStringParserDocument;
        struct ResultHelper : public ErpElement {
            using ErpElement::ErpElement;
            std::unique_ptr<model::NumberAsStringParserDocument> doc;
        };
        static const rapidjson::Pointer resourceTypePointer{ElementName::path(elements::resourceType)};
        const auto& fileContent = resourceManager.getStringResource(resourcePath.native());
        auto doc = std::make_unique<JsonDoc>(JsonDoc::fromJson(fileContent));
        std::string resourceType{doc->getStringValueFromPointer(resourceTypePointer)};
        auto result = std::make_shared<ResultHelper>(&repo(), std::weak_ptr<ErpElement>{}, resourceType, doc.get());
        result->doc = std::move(doc);
        return result;
    }

    static std::list<std::filesystem::path> getProfileList()
    {
        static constexpr auto mkres = &ResourceManager::getAbsoluteFilename;
        auto profileList = DefaultFhirStructureRepository::defaultProfileFiles();
        profileList.merge({mkres("test/fhir-path/profiles/ReferencesDomainResource.xml"),
                           mkres("test/fhir-path/profiles/ReferenceExtension.xml")});
        return profileList;
    }
    static const FhirStructureRepository& repo()
    {
        static std::unique_ptr<const FhirStructureRepository> instance =
            DefaultFhirStructureRepository::create(getProfileList());
        Expect3(instance != nullptr, "Failed to load repo", std::logic_error);
        return *instance;
    }
    ResourceManager& resourceManager = ResourceManager::instance();
};

TEST_F(ReferenceTest, invalidReferenced)
{
    auto element = loadResource("test/fhir-path/samples/invalid_references_target_profile.json");
    auto result = FhirPathValidator::validate(element, element->definitionPointer().element()->name());
    const auto& resultList = result.results();
    auto match = std::ranges::any_of(resultList, [](const ValidationError& res) {
        return res.severity() == Severity::error && res.fieldName == "DomainResource.extension[0].valueReference" &&
               std::holds_alternative<ValidationError::MessageReason>(res.reason) &&
               std::get<std::string>(std::get<ValidationError::MessageReason>(res.reason)) ==
                   "Cannot match profile to Element 'Resource': "
                   "http://hl7.org/fhir/StructureDefinition/DomainResource|4.0.1 "
                   "(referenced resource DomainResource.contained[0]{Resource} must match one of: "
                   R"-(["http://hl7.org/fhir/StructureDefinition/DomainResource|4.0.1"]))-";
    });
    if (! match)
    {
        result.dumpToLog();
    }
    EXPECT_TRUE(match) << "Expected error message not found.";
}

class ReferenceFinderTest : public ReferenceTest, public ::testing::WithParamInterface<Sample>
{
protected:
    void SetUp() override
    {
        expected = GetParam().expected;
        for (auto& exp : expected)
        {
            // resolve profiles and fill Expected::profiles field:
            std::ranges::transform(
                exp.profileUrls, std::inserter(exp.profiles, exp.profiles.end()), [&](const auto& profileUrl) {
                    const auto* profile = repo().findDefinitionByUrl(profileUrl);
                    Expect3(profile != nullptr, "profile not found: " + profileUrl, std::logic_error);
                    return profile;
                });
        }
    }

    auto findMatchingExpect(std::string_view elementFullPath, const std::set<const FhirStructureDefinition*>& profiles)
    {
        for (auto it = expected.begin(), end = expected.end(); it != end; ++it)
        {
            if (it->elementFullPath == elementFullPath && it->profiles == profiles)
            {
                return it;
            }
        }
        return expected.end();
    }
    std::list<Sample::Expected> expected;
};


TEST_P(ReferenceFinderTest, run)
{
    auto doc = loadResource(GetParam().file);
    auto finderResult = ReferenceFinder::find(*doc, {}, {}, doc->definitionPointer().element()->name());
    finderResult.validationResults.dumpToLog();
    for (const auto& resource : finderResult.referenceContext.resources())
    {
        for (const auto& refTargets : resource->referenceTargets)
        {
            for (const auto& profileSets : refTargets.targetProfileSets)
            {
                const auto& elementExpected = findMatchingExpect(refTargets.elementFullPath, profileSets.second);
                if (elementExpected == expected.end())
                {
                    ADD_FAILURE() << "unexpected referenceInfo element: " << refTargets.elementFullPath
                                  << " references: " << refTargets.identity << PrintProfileUrls(profileSets.second);
                    continue;
                }
                elementExpected->found = true;
                EXPECT_EQ(refTargets.mustBeResolvable, elementExpected->mustBeResolvable);
                EXPECT_EQ(to_string(refTargets.identity), elementExpected->targetIdentity);
            }
        }
    }
    for (const auto& exp : expected)
    {
        EXPECT_TRUE(exp.found) << "expectation not met: " << exp;
    }
}
// clang-format off
INSTANTIATE_TEST_SUITE_P(samples, ReferenceFinderTest,
                         ::testing::Values(
   Sample
   {
        .file = "test/fhir-path/samples/references_non-bundled.json",
        .expected
        {
            {
                .elementFullPath = "DomainResource.extension[0].valueReference",
                .targetIdentity ="DomainResource/root#contained1",
                .profileUrls{}
            },
            {
                .elementFullPath = "DomainResource.contained[0]{DomainResource}.extension[0].valueReference",
                .targetIdentity ="DomainResource/root#contained2",
                .profileUrls{}
            },
            {
                .elementFullPath = "DomainResource.contained[1]{DomainResource}.extension[0].valueReference",
                .targetIdentity ="DomainResource/root",
                .profileUrls{}
            },
        }
    },
    Sample{
        .file = "test/fhir-path/samples/references_bundled.json",
        .expected
        {
            {
                .elementFullPath = "Bundle.entry[0].resource{DomainResource}.extension[0].valueReference",
                .targetIdentity = "http://erp.test/DomainResource/4711",
                .profileUrls{}
            },
            {
                .elementFullPath = "Bundle.entry[1].resource{DomainResource}.extension[0].valueReference",
                .targetIdentity = "http://erp.test/DomainResource/4711",
                .profileUrls{}
            }
        }
    },
    Sample{
        .file = "test/fhir-path/samples/references_nested_bundle.json",
        .expected
        {
            {
                .elementFullPath = "Bundle.entry[0].resource{DomainResource}.extension[0].valueReference",
                .targetIdentity = "http://erp.test/DomainResource/4711",
                .profileUrls{}
            },
            {
                .elementFullPath = "Bundle.entry[1].resource{Bundle}.link[0].extension[0].valueReference",
                .targetIdentity = "http://erp.test/DomainResource/4711",
                .profileUrls{}
            }
        }
    },
    Sample{
        .file = "test/fhir-path/samples/references_target_profile.json",
        .expected
        {
            {
                .elementFullPath = "DomainResource.extension[0].valueReference",
                .targetIdentity = "DomainResource/root#contained",
                .profileUrls{}
            },
            {
                .elementFullPath = "DomainResource.extension[0].valueReference",
                .targetIdentity = "DomainResource/root#contained",
                .profileUrls
                {
                    "http://hl7.org/fhir/StructureDefinition/DomainResource"
                }
            }
        }
    },
    Sample{
        .file = "test/fhir-path/samples/references_document_bundle.json",
        .expected
        {
            {
                .elementFullPath = "Bundle.entry[0].resource{Composition}.author[0]",
                .targetIdentity = "http://erp.test/Device/0",
                .mustBeResolvable = true,
                .profileUrls{
                    "http://hl7.org/fhir/StructureDefinition/Practitioner|4.0.1",
                    "http://hl7.org/fhir/StructureDefinition/PractitionerRole|4.0.1",
                    "http://hl7.org/fhir/StructureDefinition/Device|4.0.1",
                    "http://hl7.org/fhir/StructureDefinition/Patient|4.0.1",
                    "http://hl7.org/fhir/StructureDefinition/RelatedPerson|4.0.1",
                    "http://hl7.org/fhir/StructureDefinition/Organization|4.0.1",
                }
            },
            {
                .elementFullPath = "Bundle.entry[0].resource{Composition}.author[0]",
                .targetIdentity = "http://erp.test/Device/0",
                .mustBeResolvable = true,
                .profileUrls{},
            }
        }
    }
), [](const auto& info){ return regex_replace(info.param.file.stem().string(), std::regex{"[^A-Za-z0-9]"}, "_");});
// clang-format on


namespace
{
struct BundleSample {

    struct Expected {
        std::string resourceFullUrl;
        std::string elementFullPath;
        std::set<std::string> referenceTargets{};
        AnchorType anchorType = AnchorType::none;
        AnchorType referenceRequirement = AnchorType::none;
        bool operator==(const ReferenceContext::ResourceInfo&) const;
    };
    std::filesystem::path file;
    std::vector<Expected> expected;
};

bool BundleSample::Expected::operator==(const ReferenceContext::ResourceInfo& res) const
{
    std::set<Element::Identity> referenceTargetIds;
    std::ranges::transform(referenceTargets, std::inserter(referenceTargetIds, referenceTargetIds.end()),
                           [&](const auto& refStr) {
                               return Element::IdentityAndResult::fromReferenceString(refStr, elementFullPath).identity;
                           });
    std::set<Element::Identity> resReferenceTargetIds;
    std::ranges::transform(res.referenceTargets, std::inserter(resReferenceTargetIds, resReferenceTargetIds.end()),
                           &ReferenceContext::ReferenceInfo::identity);
    // clang-format off
    return resourceFullUrl == to_string(res.identity) &&
           elementFullPath == res.elementFullPath &&
           anchorType == res.anchorType &&
           referenceTargetIds == resReferenceTargetIds &&
           referenceRequirement == res.referenceRequirement;
    // clang-format on
}

std::ostream& operator<<(std::ostream& out, const BundleSample::Expected& resInfo)
{
    out << resInfo.elementFullPath << ": "
        << R"({ "resourceFullUrl": ")" << resInfo.resourceFullUrl;
    out << R"(", "anchorType": ")" << resInfo.anchorType;
    out << R"(", "referenceRequirement": ")" << resInfo.referenceRequirement;
    out << R"(", "referenceTargets": [)";
    std::string_view sep;
    for (const auto& target : resInfo.referenceTargets)
    {
        out << sep << '"' << target << '"';
        sep = ", ";
    }
    out << "]}";
    return out;
}
}

class ReferenceFinderResourcesTest : public ReferenceTest, public ::testing::WithParamInterface<BundleSample>
{
};

TEST_P(ReferenceFinderResourcesTest, run)
{
    auto resource = loadResource(GetParam().file);
    fhirtools::FhirPathValidator::validate(resource, resource->resourceType()).dumpToLog();
    auto finderResult = ReferenceFinder::find(*resource, {}, {}, resource->definitionPointer().element()->name());
    const auto& expects = GetParam().expected;
    std::vector<bool> foundExpected(expects.size(), false);
    for (const auto& res : finderResult.referenceContext.resources())
    {
        bool ok = false;
        for (size_t expectIdx = 0; expectIdx < expects.size(); ++expectIdx)
        {
            if (*res == expects[expectIdx])
            {
                ok = true;
                foundExpected[expectIdx] = true;
                break;
            }
        }
        EXPECT_TRUE(ok) << *res;
        TVLOG(0) << *res;
    }
    for (size_t expectIdx = 0; expectIdx < expects.size(); ++expectIdx)
    {
        EXPECT_TRUE(foundExpected[expectIdx]) << expectIdx << ": " << expects[expectIdx];
    }
}

// clang-format off
INSTANTIATE_TEST_SUITE_P(samples, ReferenceFinderResourcesTest,
                         ::testing::Values(

    BundleSample{
        .file{"test/fhir-path/samples/references_nested_bundle.json"},
        .expected
        {
            {
                .resourceFullUrl = "Bundle/root",
                .elementFullPath = "Bundle",
            },
            {
                .resourceFullUrl = "http://erp.test/DomainResource/0815",
                .elementFullPath = "Bundle.entry[0].resource{DomainResource}",
                .referenceTargets{"http://erp.test/DomainResource/4711"},
            },
            {
                .resourceFullUrl = "http://erp.test/Bundle/4711",
                .elementFullPath = "Bundle.entry[1].resource{Bundle}",
                .referenceTargets{"http://erp.test/DomainResource/4711"},
            }
        }
    },
    BundleSample{
        .file{"test/fhir-path/samples/references_document_bundle.json"},
        .expected
        {
            {
                .resourceFullUrl = "Bundle/320cade4-557a-4871-b23c-710fef08273f",
                .elementFullPath = "Bundle",
            },
            {
                .resourceFullUrl = "urn:uuid:9ec4ed88-2492-48b5-9021-b04874a904c4",
                .elementFullPath = "Bundle.entry[0].resource{Composition}",
                .referenceTargets{"http://erp.test/Device/0"},
                .anchorType = AnchorType::composition,
            },
            {
                .resourceFullUrl = "http://erp.test/Device/0",
                .elementFullPath = "Bundle.entry[1].resource{Device}",
                .referenceRequirement = AnchorType::composition
            }
        }
    },
    BundleSample{
        .file{"test/fhir-path/samples/references_non-bundled.json"},
        .expected
        {
            {
                .resourceFullUrl = "DomainResource/root",
                .elementFullPath = "DomainResource",
                .referenceTargets{"DomainResource/root#contained1"}
            },
            {
                .resourceFullUrl = "DomainResource/root#contained1",
                .elementFullPath = "DomainResource.contained[0]{DomainResource}",
                .referenceTargets{"DomainResource/root#contained2"},
                .anchorType = AnchorType::contained,
                .referenceRequirement = AnchorType::contained,
            },
            {
                .resourceFullUrl = "DomainResource/root#contained2",
                .elementFullPath = "DomainResource.contained[1]{DomainResource}",
                .referenceTargets{"DomainResource/root"},
                .referenceRequirement = AnchorType::contained,
            }
        }
    }
));

// clang-format on
