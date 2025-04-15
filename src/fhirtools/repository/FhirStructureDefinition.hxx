/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef FHIR_TOOLS_FHIRSTRUCTUREDEFINITION_HXX
#define FHIR_TOOLS_FHIRSTRUCTUREDEFINITION_HXX

#include "fhirtools/repository/FhirConstraint.hxx"
#include "fhirtools/repository/FhirElement.hxx"
#include "fhirtools/repository/FhirVersion.hxx"

#include <filesystem>
#include <iosfwd>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace fhirtools {

class FhirResourceGroup;
class FhirResourceGroupResolver;
class FhirStructureRepository;
class FhirStructureRepositoryFixer;

/// @brief stores information for a single type read from FHIR definition files
/// also refer to FhirStructureRepository
class FhirStructureDefinition
{
public:
    class Builder;

    // http://hl7.org/fhir/valueset-type-derivation-rule.html
    enum class Derivation
    {
        basetype,
        specialization,
        constraint,
    };

    // http://hl7.org/fhir/valueset-structure-definition-kind.html
    enum class Kind
    {
        primitiveType,
        complexType,
        resource,
        logical,
        // system types are builtin and not read from StructureDefinition Files:
        systemBoolean,
        systemString,
        systemDouble,
        systemInteger,
        systemDate,
        systemTime,
        systemDateTime,
        // used when generating slicing types
        slice,
    };


    FhirStructureDefinition();
    virtual ~FhirStructureDefinition();

    std::string urlAndVersion() const;

    DefinitionKey definitionKey() const;

    const std::string& typeId() const
    {
        return mTypeId;
    }
    const std::string& getName() const
    {
        return mName;
    }
    const std::string& url() const
    {
        return mUrl;
    }
    const FhirVersion& version() const
    {
        return mVersion;
    }
    const std::string& baseDefinition() const
    {
        return mBaseDefinition;
    }
    Derivation derivation() const
    {
        return mDerivation;
    }
    Kind kind() const
    {
        return mKind;
    }
    // system types are builtin and not read from StructureDefinition Files
    // see also Kind
    bool isSystemType() const;
    [[nodiscard]] const std::vector<std::shared_ptr<const FhirElement>>& elements() const
    {
        return mElements;
    }

    const std::shared_ptr<const FhirElement>& rootElement() const
    {
        return mElements.at(0);
    }

    [[nodiscard]] std::shared_ptr<const FhirElement> findElement(std::string_view elementId) const;
    std::tuple<std::shared_ptr<const FhirElement>, size_t> findElementAndIndex(std::string_view elementId) const;
    const FhirStructureDefinition* parentType(const FhirStructureRepository& repo) const;
    bool isDerivedFrom(const FhirStructureRepository& repo, const std::string_view& baseUrl) const;
    bool isDerivedFrom(const FhirStructureRepository& repo, const FhirStructureDefinition& baseProfile) const;
    const FhirStructureDefinition* baseType(const FhirStructureRepository& repo) const;
    const FhirStructureDefinition& primitiveToSystemType(const FhirStructureRepository& repo) const;
    const std::shared_ptr<const FhirResourceGroup>& resourceGroup() const;

    // immutable:
    FhirStructureDefinition(const FhirStructureDefinition&) = default;
    FhirStructureDefinition(FhirStructureDefinition&&) = default;
    FhirStructureDefinition& operator=(const FhirStructureDefinition&) = delete;
    FhirStructureDefinition& operator=(FhirStructureDefinition&&) = delete;

private:
    void validate();

    std::string mTypeId;
    std::string mName;
    std::string mUrl;
    FhirVersion mVersion = FhirVersion::notVersioned;
    std::string mBaseDefinition;
    Derivation mDerivation = Derivation::basetype;
    Kind mKind = Kind::primitiveType;
    std::vector<std::shared_ptr<const FhirElement>> mElements;
    std::shared_ptr<const FhirResourceGroup> mResourceGroup;

    friend class FhirStructureRepositoryFixer;
};


class FhirStructureDefinition::Builder
{
public:
    Builder();
    ~Builder();

    Builder& url(std::string url_);

    Builder& initGroup(const FhirResourceGroupResolver& resolver, const std::filesystem::path& source);

    Builder& group(std::shared_ptr<const FhirResourceGroup> group);

    Builder& version(FhirVersion version_);

    Builder& typeId(std::string type_);

    Builder& name(std::string name_);

    Builder& baseDefinition(std::string baseDefinition_);

    Builder& derivation(Derivation derivation_);

    Builder& kind(Kind kind_);

    Builder& addElement(std::shared_ptr<const FhirElement>, std::list<std::string> withTypes);

    FhirStructureDefinition getAndReset();

private:
    class FhirSlicingBuilder;
    [[nodiscard]]
    bool addElementInternal(std::shared_ptr<const FhirElement>, std::list<std::string> withTypes);
    [[nodiscard]]
    bool addElementInternal(std::shared_ptr<const FhirElement> element);
    [[nodiscard]]
    bool addSliceElement(const std::shared_ptr<const FhirElement>&, std::list<std::string> withTypes);
    void commitSlicing(size_t elementIdx);
    FhirSlicingBuilder& getBuilder(size_t elementIdx);
    bool ensureSliceBaseElement(const std::shared_ptr<const FhirElement>&);
    struct SplitSlicedNameResult {
        std::string_view baseElement;
        std::string_view sliceName;
        std::string_view suffix;
    };
    static SplitSlicedNameResult splitSlicedName(std::string_view name);

    std::unique_ptr<FhirStructureDefinition> mStructureDefinition;
    std::map<size_t, FhirSlicingBuilder> mFhirSlicingBuilders;

    friend class FhirStructureDefinitionParser;
};

std::ostream& operator<<(std::ostream&, const FhirStructureDefinition&);

const std::string& to_string(FhirStructureDefinition::Derivation);
FhirStructureDefinition::Derivation stringToDerivation(const std::string_view&);
std::ostream& operator<<(std::ostream&, FhirStructureDefinition::Derivation);

const std::string& to_string(FhirStructureDefinition::Kind);
FhirStructureDefinition::Kind stringToKind(const std::string_view&);
std::ostream& operator<<(std::ostream&, FhirStructureDefinition::Kind);
}

#endif// FHIR_TOOLS_FHIRSTRUCTUREDEFINITION_HXX
