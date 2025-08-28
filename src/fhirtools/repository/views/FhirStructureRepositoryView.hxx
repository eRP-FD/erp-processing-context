// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#ifndef FHIRTOOLS_FHIRSTRUCTUREREPOSITORYVIEW_HXX_INCLUDED
#define FHIRTOOLS_FHIRSTRUCTUREREPOSITORYVIEW_HXX_INCLUDED

#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <optional>

namespace fhirtools
{
struct DefinitionKey;
class FhirCodeSystem;
class FhirCodeSystemCodes;
class FhirElement;
class FhirResourceGroup;
class FhirStructureDefinition;
class FhirValueSet;
class FhirValueSetCodes;
class FhirVersion;
class ProfiledElementTypeInfo;

class FhirStructureRepositoryView : public std::enable_shared_from_this<FhirStructureRepositoryView>
{
public:
    virtual ~FhirStructureRepositoryView() = default;

    [[nodiscard]] virtual std::string_view id() const = 0;

    /// @brief gets a StructureDefinition by url\@value.
    /// This can also be a profile
    /// @returns the StructureDefinition if found; nullptr otherwise
    [[nodiscard]] virtual const FhirStructureDefinition* findStructure(const DefinitionKey& key) const = 0;

    /// @brief gets a StructureDefinition by type\@value
    /// @returns the StructureDefinition if found; nullptr otherwise
    [[nodiscard]] virtual const FhirStructureDefinition* findTypeById(const std::string& typeName) const = 0;

    [[nodiscard]] virtual const FhirValueSet* findValueSet(const DefinitionKey& key) const = 0;

    [[nodiscard]] virtual std::shared_ptr<const FhirValueSetCodes>
    findValueSetCodes(const DefinitionKey& key) const = 0;

    [[nodiscard]] virtual const FhirCodeSystem* findCodeSystem(const DefinitionKey& key) const = 0;

    [[nodiscard]] virtual std::shared_ptr<const FhirCodeSystemCodes>
    findCodeSystemCodes(const DefinitionKey& key) const = 0;

    [[nodiscard]] virtual std::set<const FhirCodeSystem*> findSupplementers(const std::string& url,
                                                                            const FhirVersion& version) const = 0;

    [[nodiscard]] virtual std::optional<DefinitionKey> findRenderVersion(const DefinitionKey& key) const = 0;

    /// these are the system (builtin) types their type and url start with 'http://hl7.org/fhirpath/System.'
    virtual const FhirStructureDefinition* systemTypeBoolean() const = 0;
    virtual const FhirStructureDefinition* systemTypeString() const = 0;
    virtual const FhirStructureDefinition* systemTypeDate() const = 0;
    virtual const FhirStructureDefinition* systemTypeTime() const = 0;
    virtual const FhirStructureDefinition* systemTypeDateTime() const = 0;
    virtual const FhirStructureDefinition* systemTypeDecimal() const = 0;
    virtual const FhirStructureDefinition* systemTypeInteger() const = 0;

    struct ContentReferenceResolution {
        const FhirStructureDefinition& baseType;
        const FhirStructureDefinition& elementType;
        const std::size_t elementIndex;
        const std::shared_ptr<const FhirElement> element;
    };

    [[nodiscard]] virtual ContentReferenceResolution resolveContentReference(const FhirResourceGroup& group,
                                                                             const FhirElement& element) const = 0;
    virtual std::tuple<ProfiledElementTypeInfo, std::size_t>
    resolveBaseContentReference(std::string_view elementId) const = 0;
};


}

#endif// FHIRTOOLS_FHIRSTRUCTUREREPOSITORYVIEW_HXX_INCLUDED
