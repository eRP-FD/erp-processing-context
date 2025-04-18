/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */
#ifndef FHIR_TOOLS_FHIR_FHIRSLICING_HXX
#define FHIR_TOOLS_FHIR_FHIRSLICING_HXX

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
class FhirStructureRepository;
class ProfiledElementTypeInfo;
class ValueElement;
class ValidatorOptions;

using ExpressionPtr = std::shared_ptr<Expression>;

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
        virtual bool test(const Element&, const ValidatorOptions&) const = 0;
        virtual ~Condition() = default;
    };

    FhirSlicing();
    FhirSlicing(const FhirSlicing&);
    FhirSlicing(FhirSlicing&&) noexcept;
    ~FhirSlicing();

    class Builder;
    enum class SlicingRules
    {
        open,
        reportOther,
        closed,
        openAtEnd
    };

    [[nodiscard]] SlicingRules slicingRules() const;
    [[nodiscard]] bool ordered() const;

    [[nodiscard]] const std::vector<FhirSlice>& slices() const;
    [[nodiscard]] const std::list<FhirSliceDiscriminator>& discriminators() const;

    FhirSlicing& operator=(FhirSlicing&&) = delete;
    FhirSlicing& operator=(const FhirSlicing&) = delete;


private:
    SlicingRules mSlicingRules = SlicingRules::open;
    bool mOrdered{};
    std::vector<FhirSlice> mSlices;
    std::list<FhirSliceDiscriminator> mDiscriminators;
};


class FhirSlice
{
public:
    class Builder;
    [[nodiscard]] const std::string& name() const;
    [[nodiscard]] const FhirStructureDefinition& profile() const;
    std::shared_ptr<FhirSlicing::Condition> condition(const FhirStructureRepository& repo,
                                                      const std::list<FhirSliceDiscriminator>& discriminators) const;

    FhirSlice();
    ~FhirSlice();

    FhirSlice(const FhirSlice&);
    FhirSlice(FhirSlice&&) noexcept;
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
    [[nodiscard]] std::shared_ptr<FhirSlicing::Condition> condition(const FhirStructureRepository& repo,
                                                                    const FhirStructureDefinition* def) const;

private:
    static std::list<ProfiledElementTypeInfo> collect(const FhirStructureRepository& repo,
                                                      const FhirStructureDefinition* def, std::string_view path);
    static std::list<ProfiledElementTypeInfo> collectElements(const FhirStructureRepository& repo,
                                                              const ProfiledElementTypeInfo& parentDef,
                                                              std::string_view path);
    static std::list<ProfiledElementTypeInfo> collectSubElements(const FhirStructureRepository& repo,
                                                                 const ProfiledElementTypeInfo& parentDef,
                                                                 std::string_view path);

    static std::shared_ptr<const FhirElement> collectElement(const FhirStructureDefinition* def, std::string_view path);
    static std::list<ProfiledElementTypeInfo> collectFromValues(const FhirStructureRepository& repo,
                                                                const std::shared_ptr<const FhirElement>& element,
                                                                std::string_view path);
    static std::list<std::shared_ptr<const ValueElement>> collectFromValues(std::shared_ptr<const ValueElement> element,
                                                                            std::string_view path);
    static std::list<ProfiledElementTypeInfo> collectFromResolve(const FhirStructureRepository& repo,
                                                                 const ProfiledElementTypeInfo& parentDef,
                                                                 std::string_view path);

    [[nodiscard]] std::shared_ptr<FhirSlicing::Condition>
    valueishCondition(const FhirStructureRepository& repo, std::list<ProfiledElementTypeInfo> elementInfos,
                      const FhirStructureDefinition* def, ExpressionPtr) const;

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
    explicit Builder(FhirSlicing, std::string slicePrefix);
    Builder();
    explicit Builder(Builder&&) noexcept;
    ~Builder();

    Builder& slicePrefix(std::string);
    [[nodiscard]] std::string slicePrefix() const;

    Builder& slicingRules(SlicingRules);
    Builder& ordered(bool);
    [[nodiscard]] bool addElement(const std::shared_ptr<const FhirElement>&, std::list<std::string> withTypes,
                                  const FhirStructureDefinition& containing);

    Builder& addDiscriminator(FhirSliceDiscriminator&&);

    FhirSlicing getAndReset();

private:
    std::string sliceId(const std::string& elementName);
    void commitSlice();

    std::string mSlicePrefix;
    std::string mSliceId;
    std::unique_ptr<FhirSlicing> mFhirSlicing;
    class FhirStructureDefinitionBuilder;
    std::unique_ptr<FhirStructureDefinitionBuilder> mFhirStructureBuilder;
};

class FhirSlice::Builder
{
public:
    Builder();
    Builder& name(std::string n);
    Builder& profile(FhirStructureDefinition&& prof);
    Builder& profile(std::string url);

    [[nodiscard]] FhirSlice getAndReset();

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
