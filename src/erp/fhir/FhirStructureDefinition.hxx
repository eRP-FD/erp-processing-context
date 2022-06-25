/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_FHIRSTRUCTUREDEFINITION_HXX
#define ERP_PROCESSING_CONTEXT_FHIRSTRUCTUREDEFINITION_HXX

#include "erp/fhir/FhirElement.hxx"

#include <iosfwd>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

class FhirStructureRepository;

/// @brief stores information for a single type read from FHIR definition files
/// also refer to FhirStructureRepository
class FhirStructureDefinition
{
public:
    class Builder;
    enum class Derivation {
        basetype,
        specialization,
        constraint,
    };

    enum class Kind {
        primitiveType,
        complexType,
        resource,
        logical,
        // system types are builtin and not read from StructureDefinition Files:
        systemBoolean,
        systemString,
        systemDouble,
        systemInteger,
    };


    FhirStructureDefinition();
    virtual ~FhirStructureDefinition();

    const std::string& typeId() const { return mTypeId; }
    const std::string& url() const { return mUrl; }
    const std::string& baseDefinition() const { return mBaseDefinition; }
    Derivation derivation() const { return mDerivation; }
    Kind kind() const { return mKind; }
    // system types are builtin and not read from StructureDefinition Files
    // see also Kind
    bool isSystemType() const;
    [[nodiscard]] const std::vector<std::shared_ptr<const FhirElement>>& elements() const
    {
        return mElements;
    }

    [[nodiscard]] std::shared_ptr<const FhirElement> findElement(const std::string& elementId) const;
    bool isDerivedFrom(const FhirStructureRepository& repo, const std::string_view& baseUrl) const;

    // immutable:
    FhirStructureDefinition(const FhirStructureDefinition&) = default;
    FhirStructureDefinition(FhirStructureDefinition&&) = default;
    FhirStructureDefinition& operator = (const FhirStructureDefinition&) = delete;
    FhirStructureDefinition& operator = (FhirStructureDefinition&&) = delete;
private:
    void validate();

    std::string mTypeId;
    std::string mUrl;
    std::string mBaseDefinition;
    Derivation mDerivation = Derivation::basetype;
    Kind mKind = Kind::primitiveType;
    std::vector<std::shared_ptr<const FhirElement>> mElements;
};


class FhirStructureDefinition::Builder
{
public:
    Builder();

    Builder& url(std::string url_);

    Builder& typeId(std::string type_);

    Builder& baseDefinition(std::string baseDefinition_);

    Builder& derivation(Derivation derivation_);

    Builder& kind(Kind kind_);

    Builder& addElement(std::shared_ptr<const FhirElement> element);

    FhirStructureDefinition getAndReset();

private:
    std::unique_ptr<FhirStructureDefinition> mStructureDefinition;
};

std::ostream& operator << (std::ostream&, const FhirStructureDefinition&);

const std::string& to_string(FhirStructureDefinition::Derivation);
FhirStructureDefinition::Derivation stringToDerivation(const std::string_view&);
std::ostream& operator << (std::ostream&, FhirStructureDefinition::Derivation);

const std::string& to_string(FhirStructureDefinition::Kind);
FhirStructureDefinition::Kind stringToKind(const std::string_view&);
std::ostream& operator << (std::ostream&, FhirStructureDefinition::Kind);


#endif // ERP_PROCESSING_CONTEXT_FHIRSTRUCTUREDEFINITION_HXX
