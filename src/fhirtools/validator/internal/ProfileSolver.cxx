// (C) Copyright IBM Deutschland GmbH 2022
// (C) Copyright IBM Corp. 2022

#include "fhirtools/validator/internal/ProfileSolver.hxx"

#include "fhirtools/FPExpect.hxx"
#include "fhirtools/validator/ValidationResult.hxx"
#include "fhirtools/validator/internal/ProfileSetValidator.hxx"
#include "fhirtools/validator/internal/ProfileValidator.hxx"
#include "fhirtools/validator/internal/ValidationData.hxx"

using namespace fhirtools;


class ProfileSolver::RequireOneSolver
{
public:
    explicit RequireOneSolver(std::map<ProfileValidatorMapKey, std::shared_ptr<ValidationData>> profileData)
        : mGoodProfiles{std::move(profileData)}
    {
    }
    ValidationResultList collectResults() const
    {
        ValidationResultList result;
        TVLOG(3) << __PRETTY_FUNCTION__ << " good: " << mGoodProfiles.size() << ", failed: " << mFailedProfiles.size();
        const auto& collectFrom = mFailed ? mFailedProfiles : mGoodProfiles;
        for (const auto& resData : collectFrom)
        {
            TVLOG(4) << "    adding results from:" << resData.first;
            result.append(resData.second->results());
        }

        return result;
    }
    bool fail(const ProfileValidator::MapKey& key)
    {
        auto insertRes = mFailedProfiles.insert(mGoodProfiles.extract(key));
        if (insertRes.inserted)
        {
            TVLOG(4) << __PRETTY_FUNCTION__ << "failing: " << key;
            mFailed = mGoodProfiles.empty();
        }
        return mFailed;
    }
    [[nodiscard]] bool failed()
    {
        return mFailed;
    }

private:
    bool mFailed = false;
    std::map<ProfileValidatorMapKey, std::shared_ptr<ValidationData>> mGoodProfiles;
    std::map<ProfileValidatorMapKey, std::shared_ptr<ValidationData>> mFailedProfiles;
};

fhirtools::ProfileSolver& fhirtools::ProfileSolver::operator=(fhirtools::ProfileSolver&&) noexcept = default;
fhirtools::ProfileSolver::ProfileSolver() = default;
fhirtools::ProfileSolver::ProfileSolver(fhirtools::ProfileSolver&&) noexcept = default;
fhirtools::ProfileSolver::~ProfileSolver() = default;

void fhirtools::ProfileSolver::merge(const ProfileSolver& other)
{
    for (const auto& otherSolver : other.mSolvers)
    {
        mSolvers.emplace_back(otherSolver);
    }
}

void fhirtools::ProfileSolver::requireOne(std::map<ProfileValidatorMapKey, std::shared_ptr<ValidationData>> profileData)
{
    FPExpect3(! profileData.empty(), "requireOne profile-set must not be empty.", std::logic_error);
    mSolvers.emplace_back(std::move(profileData));
}

fhirtools::ValidationResultList fhirtools::ProfileSolver::collectResults() const
{
    ValidationResultList result;
    for (const auto& solver : mSolvers)
    {
        result.append(solver.collectResults());
    }
    return result;
}

bool fhirtools::ProfileSolver::fail(const ProfileValidator::MapKey& mapKeys)
{
    for (auto&& solver : mSolvers)
    {
        if (! solver.fail(mapKeys))
        {
            mFailed = true;
        }
    }
    return mFailed;
}

bool ProfileSolver::failed() const
{
    return mFailed;
}