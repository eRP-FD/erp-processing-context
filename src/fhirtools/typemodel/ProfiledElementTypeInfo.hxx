/**
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 **/


#ifndef FHIR_TOOLS_FHIR_PATH_FHIRELEMENTINFO_HXX
#define FHIR_TOOLS_FHIR_PATH_FHIRELEMENTINFO_HXX

#include <list>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace fhirtools
{
class FhirElement;
class FhirStructureRepository;
class FhirStructureDefinition;

/**
 * @class ProfiledElementTypeInfo
 * @brief Provides information about an Element in a FhirStructureDefinition
 *
 * FhirElement and FhirStructureDefinition are the main FHIR Type information
 * this class combines both and allows deriving information, that cannot be obtained
 * by using one of them alone.
 */
class ProfiledElementTypeInfo
{
public:
    explicit ProfiledElementTypeInfo(const FhirStructureDefinition* profile);
    explicit ProfiledElementTypeInfo(const FhirStructureDefinition* profile, std::shared_ptr<const FhirElement> element);
    explicit ProfiledElementTypeInfo(const FhirStructureRepository* repo, std::string_view elementId);

    [[nodiscard]] std::vector<ProfiledElementTypeInfo> subElements() const;
    [[nodiscard]] std::optional<ProfiledElementTypeInfo> subField(const std::string_view& name) const;
    [[nodiscard]] std::list<ProfiledElementTypeInfo> subDefinitions(const FhirStructureRepository&,
                                                                  std::string_view name) const;
    void typecast(const FhirStructureRepository& repo, const FhirStructureDefinition* structDef);

    [[nodiscard]] const FhirStructureDefinition* profile() const;
    [[nodiscard]] const std::shared_ptr<const FhirElement>& element() const;
    [[nodiscard]] bool isResource() const;
    [[nodiscard]] std::list<std::string> expandedNames(std::string_view name) const;
    [[nodiscard]] std::string_view elementPath() const;

    //NOLINTNEXTLINE(hicpp-use-nullptr,modernize-use-nullptr)
    auto operator<=>(const ProfiledElementTypeInfo&) const = default;

private:
    const FhirStructureDefinition* mProfile = nullptr;
    std::shared_ptr<const FhirElement> mElement;
};

std::ostream& operator<<(std::ostream&, const ProfiledElementTypeInfo&);
std::string to_string(const ProfiledElementTypeInfo&);

inline const FhirStructureDefinition* ProfiledElementTypeInfo::profile() const
{
    return mProfile;
}
inline const std::shared_ptr<const fhirtools::FhirElement>& ProfiledElementTypeInfo::element() const
{
    return mElement;
}
}

#endif// FHIR_TOOLS_FHIR_PATH_FHIRELEMENTINFO_HXX
