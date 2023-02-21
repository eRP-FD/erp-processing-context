// (C) Copyright IBM Deutschland GmbH 2022
// (C) Copyright IBM Corp. 2022

#ifndef FHIRTOOLS_VALIDATION_INTERNAL_REFERENCEFINDER_H
#define FHIRTOOLS_VALIDATION_INTERNAL_REFERENCEFINDER_H

#include "fhirtools/validator/ValidationResult.hxx"
#include "fhirtools/validator/internal/ReferenceContext.hxx"

#include <list>
#include <map>
#include <set>
#include <string>
#include <string_view>

namespace fhirtools
{
class Element;
class FhirStructureDefinition;
class FhirStructureRepository;
class ProfiledElementTypeInfo;
class ValidatorOptions;

/// @brief Walks the document-tree and records all resources and references in a RefernceContext
class ReferenceFinder
{
public:
    struct FinderResult {
        ValidationResults validationResults;
        ReferenceContext referenceContext;
        void merge(FinderResult&&);
    };

    [[nodiscard]] static FinderResult find(const Element& element, std::set<ProfiledElementTypeInfo> profiles,
                                           const ValidatorOptions&, std::string_view elementFullPath);

    ReferenceFinder(const ReferenceFinder&) = delete;
    ReferenceFinder& operator=(const ReferenceFinder&) = delete;

private:
    enum class ResourceHandling
    {
        contained,
        expectedComposition,
        mustBeReferencedFromAnchor,
        other
    };
    enum class ElementType
    {
        bundle,
        bundleEntry,
        bundledResource,
        containedResource,
        other
    };

    using ReferenceInfo = ReferenceContext::ReferenceInfo;
    using ResourceInfo = ReferenceContext::ResourceInfo;
    ReferenceFinder(std::set<ProfiledElementTypeInfo> profiles, std::shared_ptr<ResourceInfo> currentResource,
                    bool followBundleEntry, bool isDocumentBundle, size_t bundleIndex, const ValidatorOptions&);

    void findInternal(const Element& element, std::string_view elementFullPath, const std::string& resourcePath);
    void processResource(const Element& element, std::set<ProfiledElementTypeInfo> allSubPets, ElementType elementType,
                         std::ostringstream&& elementFullPath);

    void processReference(const Element& element, std::string_view elementFullPath, std::string resourcePath);
    std::set<ProfiledElementTypeInfo> subProfiledElementTypes(const FhirStructureRepository& element,
                                                              std::string_view subFieldName,
                                                              std::string_view subFullPathBase);
    std::set<ProfiledElementTypeInfo> sliceProfiledElementTypes(const Element& element,
                                                                const ProfiledElementTypeInfo& profiledElementTypeInfo);
    std::set<ProfiledElementTypeInfo> profilesFromResource(const Element&, std::string_view elementFullPath);
    void addSliceProfiles(const Element& element, std::string_view elementFullPath);
    void addProfiles(const FhirStructureRepository& repo, const std::list<std::string>& newProfileUrls,
                     std::string_view elementFullPath, const FhirStructureDefinition* sourceProfile);
    void addProfiles(const fhirtools::FhirStructureRepository& repo,
                     const std::set<fhirtools::ProfiledElementTypeInfo>& newProfiles, std::string_view elementFullPath);

    static bool isDocumentBundle(bool isBundle, const Element& element);
    static ElementType getElementType(const FhirStructureRepository& repo, const ProfiledElementTypeInfo& elementInfo);
    ResourceHandling getResourceHandling(ElementType elementType) const;
    ReferenceContext::AnchorType referenceRequirement(ResourceHandling elementType);
    const FhirStructureDefinition* getTypeFromReferenceElement(const FhirStructureRepository& repo,
                                                               const Element& referenceElement,
                                                               const std::string_view elementFullPath);

    std::set<ProfiledElementTypeInfo> mProfiledElementTypes;
    FinderResult mResult;
    std::shared_ptr<ResourceInfo> mCurrentResource;
    bool mFollowBundleEntry = true;
    bool mIsDocumentBundle = false;
    size_t mBundleIndex;
    const ValidatorOptions& mOptions;
};

}

#endif// FHIRTOOLS_VALIDATION_INTERNAL_REFERENCEFINDER_H
