// (C) Copyright IBM Deutschland GmbH 2021, 2024
// (C) Copyright IBM Corp. 2021, 2024
//
// non-exclusively licensed to gematik GmbH

#ifndef FHIR_TOOLS_PROFILESOLVER_HXX
#define FHIR_TOOLS_PROFILESOLVER_HXX


#include <list>
#include <map>
#include <memory>

namespace fhirtools
{
class ProfiledElementTypeInfo;
class ProfileSetValidator;
class ProfileValidatorMapKey;
class ValidationData;
class ValidationResults;

/**
 * @brief Resolves the Validation result for elements, that are allowed to match one of multiple Profiles
 *
 * @see Element.type.profile in https://simplifier.net/packages/hl7.fhir.r4.core/4.0.1/files/81633
 */
class ProfileSolver
{
public:
    ProfileSolver();
    ~ProfileSolver();
    ProfileSolver(const ProfileSolver&) = delete;
    ProfileSolver(ProfileSolver&&) noexcept;
    ProfileSolver& operator =(const ProfileSolver&) = delete;
    ProfileSolver& operator =(ProfileSolver&&) noexcept;
    void requireOne(std::map<ProfiledElementTypeInfo, std::shared_ptr<const ValidationData>> profileData);
    [[nodiscard]] ValidationResults collectResults() const;
    bool fail(const ProfiledElementTypeInfo& mapKey);
    [[nodiscard]] bool failed() const;

    void merge(const ProfileSolver&);

private:
    class RequireOneSolver;
    bool mFailed = false;
    std::list<RequireOneSolver> mSolvers;
};

}

#endif// FHIR_TOOLS_PROFILESOLVER_HXX
