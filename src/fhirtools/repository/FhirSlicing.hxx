/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */
#ifndef FHIR_TOOLS_FHIR_FHIRSLICING_HXX
#define FHIR_TOOLS_FHIR_FHIRSLICING_HXX

#include "fhirtools/typemodel/ProfiledElementTypeInfo.hxx"
#include "fhirtools/util/Gsl.hxx"

#include <list>
#include <memory>
#include <string>
#include <vector>

namespace fhirtools
{

class Element;
class Expression;
class FhirElement;
class FhirSlice;
class FhirSliceDiscriminator;
class FhirStructureDefinition;
class FhirStructureRepositoryBackend;
class FhirStructureRepositoryView;
class ValueElement;
class ValidatorOptions;

using ExpressionPtr = std::shared_ptr<Expression>;

namespace internal
{
struct ElementType;
}

/**
 * @brief Provides information about slices for an element
 *
 * This doesn't represent the process of slicing. The word Slicing in the classname refers to a noun.
 */
class FhirSlicing
{
public:
    class Condition
    {
    public:
        virtual bool test(const std::shared_ptr<const FhirStructureRepositoryView>& view, const Element&,
                          const ValidatorOptions&) const = 0;
        virtual ~Condition() = default;
    };

    FhirSlicing();
    ~FhirSlicing();

    class Builder;
    enum class SlicingRules
    {
        open,
        reportOther,
        closed,
        openAtEnd
    };

    std::shared_ptr<const FhirElement> baseElement() const;
    [[nodiscard]] SlicingRules slicingRules() const;
    [[nodiscard]] bool ordered() const;

    [[nodiscard]] const std::vector<FhirSlice>& slices() const;
    [[nodiscard]] const std::list<FhirSliceDiscriminator>& discriminators() const;

    FhirSlicing& operator=(FhirSlicing&&) = delete;
    FhirSlicing& operator=(const FhirSlicing&) = delete;


private:
    FhirSlicing(const FhirSlicing&);
    FhirSlicing(FhirSlicing&&) noexcept;

    SlicingRules mSlicingRules = SlicingRules::open;
    bool mOrdered{};
    std::vector<FhirSlice> mSlices;
    std::list<FhirSliceDiscriminator> mDiscriminators;
    std::weak_ptr<const FhirElement> mBaseElement;
};


class FhirSlice
{
public:
    class Builder;
    [[nodiscard]] const std::string& name() const;
    [[nodiscard]] const FhirStructureDefinition& profile() const;
    std::shared_ptr<FhirSlicing::Condition> condition(gsl::not_null<const FhirStructureRepositoryBackend*> repo,
                                                      const std::list<FhirSliceDiscriminator>& discriminators) const;

    FhirSlice();
    FhirSlice(const FhirSlice&);
    FhirSlice(FhirSlice&&) noexcept;
    ~FhirSlice();

    FhirSlice& operator=(const FhirSlice&) = delete;
    FhirSlice& operator=(FhirSlice&&) = delete;

private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};

class FhirSliceDiscriminator
{
public:
    class Builder;
    enum class DiscriminatorType
    {
        value,
        exists,
        pattern,
        type,
        profile
    };
    [[nodiscard]] const std::string& path() const;
    [[nodiscard]] DiscriminatorType type() const;
    [[nodiscard]] std::shared_ptr<FhirSlicing::Condition>
    condition(gsl::not_null<const FhirStructureRepositoryBackend*> repo, const FhirStructureDefinition& def) const;

private:
    static std::list<ProfiledElementTypeInfo> collect(gsl::not_null<const FhirStructureRepositoryBackend*> repo,
                                                      const FhirStructureDefinition* def, std::string_view path);
    static std::list<ProfiledElementTypeInfo> collectElements(gsl::not_null<const FhirStructureRepositoryBackend*> repo,
                                                              const ProfiledElementTypeInfo& parentDef,
                                                              std::string_view path);
    static std::list<ProfiledElementTypeInfo>
    collectSubElements(gsl::not_null<const FhirStructureRepositoryBackend*> repo,
                       const ProfiledElementTypeInfo& parentDef, std::string_view path);

    static std::shared_ptr<const FhirElement> collectElement(const FhirStructureDefinition* def, std::string_view path);
    static std::list<ProfiledElementTypeInfo>
    collectFromValues(gsl::not_null<const FhirStructureRepositoryBackend*> repo,
                      const std::shared_ptr<const FhirElement>& element, std::string_view path);
    static std::list<std::shared_ptr<const ValueElement>> collectFromValues(std::shared_ptr<const ValueElement> element,
                                                                            std::string_view path);
    static std::list<ProfiledElementTypeInfo>
    collectFromResolve(gsl::not_null<const FhirStructureRepositoryBackend*> repo,
                       const ProfiledElementTypeInfo& parentDef, std::string_view path);

    [[nodiscard]] std::shared_ptr<FhirSlicing::Condition>
    valueishCondition(gsl::not_null<const fhirtools::FhirStructureRepositoryBackend*> repo,
                      std::list<ProfiledElementTypeInfo> elementInfos, const FhirStructureDefinition* def,
                      ExpressionPtr) const;

    // For a slicing the returned bools indicate if that slicing has a pattern, fixed, or binding condition or a
    // combination of those.
    [[nodiscard]] std::tuple<bool, bool, bool>
    getvalueishConditionProperties(const std::list<ProfiledElementTypeInfo>& elementInfos) const;

    DiscriminatorType mType = DiscriminatorType::value;
    std::string mPath;
};


class FhirSlicing::Builder
{
public:
    explicit Builder(const FhirSlicing& slicingTemplate, std::string slicePrefix);
    explicit Builder(const FhirSlicing& slicingTemplate);
    Builder();
    Builder(Builder&&) noexcept;
    Builder& operator = (Builder&&) noexcept;
    ~Builder();

    Builder& slicePrefix(std::string);
    [[nodiscard]] std::string slicePrefix() const;

    Builder& slicingRules(SlicingRules);
    Builder& ordered(bool);
    [[nodiscard]] bool addElement(const std::shared_ptr<const FhirElement>&,
                                  const std::list<internal::ElementType>& withTypes,
                                  const FhirStructureDefinition& containing);

    Builder& addDiscriminator(FhirSliceDiscriminator&&);

    Builder& baseElement(std::weak_ptr<const FhirElement> baseElement);

    Builder& repositoryBackend(gsl::not_null<const FhirStructureRepositoryBackend*> backend);

    std::shared_ptr<const FhirSlicing> getAndReset();

private:
    class ConstructFhirSlicing;

    std::string sliceId(const std::string& elementName);
    void commitSlice();

    std::string mSlicePrefix;
    std::string mSliceId;
    std::shared_ptr<FhirSlicing> mFhirSlicing;
    class FhirStructureDefinitionBuilder;
    std::unique_ptr<FhirStructureDefinitionBuilder> mFhirStructureBuilder;
    const FhirStructureRepositoryBackend* mRepositoryBackend = nullptr;
};

class FhirSlice::Builder
{
public:
    Builder();
    Builder& name(std::string n);
    Builder& profile(std::unique_ptr<FhirStructureDefinition> prof);
    Builder& profile(std::string url);

    [[nodiscard]] FhirSlice getAndReset();

    Builder(const Builder&) = delete;
    Builder(Builder&&) = delete;
    Builder& operator = (const Builder&) = delete;
    Builder& operator = (Builder&&) = delete;
private:
    std::unique_ptr<FhirSlice> mFhirSlice;
};

class FhirSliceDiscriminator::Builder
{
public:
    Builder();
    Builder& path(std::string p);
    Builder& type(DiscriminatorType dt);

    [[nodiscard]] FhirSliceDiscriminator getAndReset();

private:
    std::unique_ptr<FhirSliceDiscriminator> mSliceDiscriminator;
};

}
#endif// FHIR_TOOLS_FHIR_FHIRSLICING_HXX
