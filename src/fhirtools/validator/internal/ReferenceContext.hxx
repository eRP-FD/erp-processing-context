// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#ifndef FHIRTOOLS_VALIDATOR_INTERNAL_REFERENCECONTEXT_HXX
#define FHIRTOOLS_VALIDATOR_INTERNAL_REFERENCECONTEXT_HXX


#include "fhirtools/model/Element.hxx"

#include <iosfwd>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <type_traits>

namespace fhirtools
{

class Element;
class FhirStructureDefinition;
class ValidationResults;
class ValidatorOptions;
/// @brief Holds information about all references in FHIR-ReferenceContext
///
/// NOTE: A reference context ends at Bundle.entry, when a Bundle is contained in another Bundle
class ReferenceContext
{
public:
    enum class AnchorType
    {
        none = 0,
        contained = 0b01,
        composition = 0b10,
        all = contained | composition,
    };
    struct ResourceInfo;
    struct ReferenceInfo {
        Element::Identity identity;
        std::string elementFullPath;
        std::string localPath;
        std::shared_ptr<const Element> referencingElement;
        std::map<const FhirStructureDefinition*, std::set<const FhirStructureDefinition*>> targetProfileSets;
        bool mustBeResolvable = false;
        bool resolved = false;
        std::weak_ptr<ResourceInfo> referencedResource{};
    };
    struct ResourceInfo {
        Element::Identity identity;
        std::optional<std::string> fullUrl = std::nullopt;
        std::optional<std::string> id = std::nullopt;
        std::string elementFullPath;
        std::shared_ptr<const Element> resourceRoot;
        AnchorType anchorType = AnchorType::none;
        AnchorType referenceRequirement = AnchorType::none;
        AnchorType referencedByAnchor = AnchorType::none;
        std::list<ReferenceInfo> referenceTargets{};
        std::weak_ptr<const ResourceInfo> parent;
        std::list<std::shared_ptr<ResourceInfo>> children{}; // either from Bundle.entry or from DomainResource.contained
    };
    void merge(ReferenceContext&&);
    void addResource(std::shared_ptr<ResourceInfo> newResource);
    std::list<std::shared_ptr<ResourceInfo>>& resources();

    [[nodiscard]] ValidationResults finalize(const ValidatorOptions&);

    ReferenceContext() = default;
    ReferenceContext(const ReferenceContext&) = delete;
    ReferenceContext(ReferenceContext&&) = default;
    ReferenceContext& operator = (const ReferenceContext&) = delete;
    ReferenceContext& operator = (ReferenceContext&&) = default;
    void dumpToLog() const;

private:
    class Finalizer;
    std::list<std::shared_ptr<ResourceInfo>> mResources;
};

std::ostream& operator<<(std::ostream& out, ReferenceContext::AnchorType);
std::ostream& operator<<(std::ostream& out, const ReferenceContext::ResourceInfo&);


ReferenceContext::AnchorType operator|(ReferenceContext::AnchorType lhs, ReferenceContext::AnchorType rhs);
ReferenceContext::AnchorType operator&(ReferenceContext::AnchorType lhs, ReferenceContext::AnchorType rhs);
ReferenceContext::AnchorType operator^(ReferenceContext::AnchorType lhs, ReferenceContext::AnchorType rhs);
ReferenceContext::AnchorType operator~(ReferenceContext::AnchorType arg);
ReferenceContext::AnchorType& operator|=(ReferenceContext::AnchorType& lhs, ReferenceContext::AnchorType rhs);
ReferenceContext::AnchorType& operator&=(ReferenceContext::AnchorType& lhs, ReferenceContext::AnchorType rhs);


}// namespace fhirtools


#endif// FHIRTOOLS_VALIDATOR_INTERNAL_REFERENCECONTEXT_HXX
