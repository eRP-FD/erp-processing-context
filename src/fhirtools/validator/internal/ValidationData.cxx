// (C) Copyright IBM Deutschland GmbH 2022
// (C) Copyright IBM Corp. 2022

#include "ValidationData.hxx"

#include "fhirtools/FPExpect.hxx"
#include "fhirtools/validator/internal/ProfileValidator.hxx"

using namespace fhirtools;

fhirtools::ValidationData::ValidationData(std::unique_ptr<ProfileValidatorMapKey> initMapKey)
    : mMapKey{std::move(initMapKey)}
{
    FPExpect3(mMapKey != nullptr, "mapKey must not be null.", std::logic_error);
}

void fhirtools::ValidationData::add(fhirtools::FhirConstraint constraint, std::string elementFullPath,
                                    const fhirtools::FhirStructureDefinition* profile)
{
    if (constraint.getSeverity() >= Severity::error)
    {
        mFailed = true;
    }
    mResult.add(std::move(constraint), std::move(elementFullPath), profile);
}

void fhirtools::ValidationData::add(fhirtools::Severity severity, std::string message, std::string elementFullPath,
                                    const fhirtools::FhirStructureDefinition* profile)
{
    if (severity >= Severity::error)
    {
        mFailed = true;
    }
    mResult.add(severity, std::move(message), std::move(elementFullPath), profile);
}

void fhirtools::ValidationData::append(fhirtools::ValidationResultList resList)
{
    if (resList.highestSeverity() >= Severity::error)
    {
        mFailed = true;
    }
    mResult.append(std::move(resList));
}

bool fhirtools::ValidationData::isFailed() const
{
    return mFailed;
}

void fhirtools::ValidationData::fail()
{
    mFailed = true;
}

void fhirtools::ValidationData::merge(const fhirtools::ValidationData& other)
{
    mFailed = mFailed || other.mFailed;
    mResult.append(other.mResult);
}

const fhirtools::ProfileValidatorMapKey& fhirtools::ValidationData::mapKey()
{
    return *mMapKey;
}

const fhirtools::ValidationResultList&
fhirtools::ValidationData::results() const
{
    return mResult;
}
