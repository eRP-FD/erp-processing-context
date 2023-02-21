// (C) Copyright IBM Deutschland GmbH 2022
// (C) Copyright IBM Corp. 2022

#include "ValidationData.hxx"
#include "fhirtools/FPExpect.hxx"
#include "fhirtools/validator/internal/ProfileValidator.hxx"

#include <boost/algorithm/string/split.hpp>

using namespace fhirtools;

fhirtools::ValidationData::ValidationData(std::unique_ptr<ProfiledElementTypeInfo> initMapKey)
    : mMapKey{std::move(initMapKey)}
{
    FPExpect3(mMapKey != nullptr, "mapKey must not be null.", std::logic_error);
}

void fhirtools::ValidationData::add(FhirConstraint constraint, std::string elementFullPath,
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

void fhirtools::ValidationData::append(fhirtools::ValidationResults resList)
{
    if (resList.highestSeverity() >= Severity::error)
    {
        mFailed = true;
    }
    mResult.merge(std::move(resList));
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
    mResult.merge(other.mResult);
}

const fhirtools::ProfiledElementTypeInfo& fhirtools::ValidationData::mapKey()
{
    return *mMapKey;
}

const fhirtools::ValidationResults& fhirtools::ValidationData::results() const
{
    return mResult;
}
