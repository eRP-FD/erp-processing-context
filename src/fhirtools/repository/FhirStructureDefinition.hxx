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
#include "fhirtools/util/Gsl.hxx"

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
class FhirStructureRepositoryBackend;
class FhirStructureRepositoryView;
class FhirStructureRepositoryFixer;

/// @brief stores information for a single type read from FHIR definition files
/// also refer to FhirStructureRepositoryView
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

    DefinitionKey key() const;

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
    const std::filesystem::path& sourceFile() const;

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
    const FhirStructureDefinition* parentType() const;
    bool isDerivedFrom(const std::string_view& baseUrl) const;
    bool isDerivedFrom(const FhirStructureDefinition& baseProfile) const;
    const FhirStructureDefinition* baseType() const;
    const FhirStructureDefinition& primitiveToSystemType() const;
    const std::shared_ptr<const FhirResourceGroup>& resourceGroup() const;

    const FhirStructureRepositoryBackend& repositoryBackend() const;

    // immutable:
    FhirStructureDefinition(FhirStructureDefinition&&) = delete;
    FhirStructureDefinition& operator=(const FhirStructureDefinition&) = delete;
    FhirStructureDefinition& operator=(FhirStructureDefinition&&) = delete;

private:
    FhirStructureDefinition(const FhirStructureDefinition&) = default;

    void validate();

    std::string mTypeId;
    std::string mName;
    std::string mUrl;
    FhirVersion mVersion = FhirVersion::notVersioned;
    std::filesystem::path mSourceFile;
    std::string mBaseDefinition;
    Derivation mDerivation = Derivation::basetype;
    Kind mKind = Kind::primitiveType;
    std::vector<std::shared_ptr<const FhirElement>> mElements;
    std::shared_ptr<const FhirResourceGroup> mResourceGroup;
    const FhirStructureRepositoryBackend* mRepositoryBackend = nullptr;

    friend class FhirStructureRepositoryFixer;
};


class FhirStructureDefinition::Builder
{
public:
    Builder();

    /// clears @p other ready to build a new structure
    //NOLINTNEXTLINE(hicpp-noexcept-move, performance-noexcept-move-constructor) - construction of FhirStructureDefinition might throw
    Builder(Builder&& other) noexcept;

    ~Builder();

    Builder& url(std::string url_);

    Builder& initGroup(const FhirResourceGroupResolver& resolver);

    Builder& group(std::shared_ptr<const FhirResourceGroup> group);

    Builder& version(FhirVersion version_);

    Builder& sourceFile(std::filesystem::path path);

    Builder& typeId(std::string type_);

    Builder& name(std::string name_);

    Builder& baseDefinition(std::string baseDefinition_);

    Builder& derivation(Derivation derivation_);

    Builder& kind(Kind kind_);

    Builder& addElement(FhirElement::Builder elementBuilder, std::list<std::string> withTypes);

    Builder& repositoryBackend(gsl::not_null<const FhirStructureRepositoryBackend*> backend);

    void reset();
    std::unique_ptr<FhirStructureDefinition> getAndReset();

    Builder(const Builder&) = delete;
    Builder& operator = (const Builder&) = delete;
    Builder& operator = (Builder&&) = delete;

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
