/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#include "FhirValueSet.hxx"

#include <boost/algorithm/string/case_conv.hpp>

#include "FhirStructureRepository.hxx"
#include "fhirtools/FPExpect.hxx"

namespace fhirtools
{

bool FhirValueSet::Code::operator==(const FhirValueSet::Code& other) const
{
    if (! codeSystem.empty() && ! other.codeSystem.empty() && codeSystem != other.codeSystem)
    {
        return false;
    }
    if (! caseSensitive && ! other.caseSensitive)
    {
        return boost::to_lower_copy(code) == boost::to_lower_copy(other.code);
    }
    return code == other.code;
}
std::strong_ordering FhirValueSet::Code::operator<=>(const FhirValueSet::Code& other) const
{
    auto order = caseSensitive || other.caseSensitive ? code <=> other.code
                                                      : boost::to_lower_copy(code) <=> boost::to_lower_copy(other.code);
    if (order == std::strong_ordering::equal && ! codeSystem.empty() && ! other.codeSystem.empty())
    {
        return codeSystem <=> other.codeSystem;
    }
    return order;
}

const std::string& FhirValueSet::getUrl() const
{
    return mUrl;
}
const std::string& FhirValueSet::getName() const
{
    return mName;
}
const std::vector<FhirValueSet::IncludeOrExclude>& FhirValueSet::getIncludes() const
{
    return mIncludes;
}
const std::vector<FhirValueSet::IncludeOrExclude>& FhirValueSet::getExcludes() const
{
    return mExcludes;
}
const std::vector<FhirValueSet::Expansion>& FhirValueSet::getExpands() const
{
    return mExpands;
}
const Version& FhirValueSet::getVersion() const
{
    return mVersion;
}

bool FhirValueSet::containsCode(const std::string& code) const
{
    return mCodes.contains(Code{.code = code, .caseSensitive = false, .codeSystem = {}});
}

bool FhirValueSet::containsCode(const std::string& code, const std::string& codeSystem) const
{
    return mCodes.contains(Code{.code = code, .caseSensitive = false, .codeSystem = codeSystem});
}

void FhirValueSet::finalize(FhirStructureRepository* repo)// NOLINT(misc-no-recursion)
{
    for (const auto& include : mIncludes)
    {
        const auto* codeSystem = include.codeSystemUrl ? repo->findCodeSystem(*include.codeSystemUrl, {}) : nullptr;
        FPExpect(! codeSystem || include.valueSets.empty(),
                 mName + ": Not Implemented case: ValueSet.compose.include with both CodeSystem and ValueSets");
        bool caseSensitive = codeSystem == nullptr || codeSystem->isCaseSensitive();
        finalizeIncludeValueSets(repo, include.valueSets);
        finalizeIncludeCodes(include.codes, caseSensitive, include.codeSystemUrl.value_or(""));
        finalizeIncludeFilters(include.filters, include.codeSystemUrl.value_or(""), codeSystem, caseSensitive);
        if (codeSystem && include.codes.empty() && include.filters.empty())
        {
            for (const auto& code : codeSystem->getCodes())
            {
                mCodes.insert(
                    Code{.code = code.code, .caseSensitive = caseSensitive, .codeSystem = *include.codeSystemUrl});
            }
        }
    }
    for (auto& exclude : mExcludes)
    {
        FPExpect(exclude.valueSets.empty(), mName + ": Not implemented: ValueSet.compose.exclude.ValueSet");
        FPExpect(exclude.codeSystemUrl, mName + ": ValueSet.compose.exclude.CodeSystem missing");
        const auto* codeSystem = exclude.codeSystemUrl ? repo->findCodeSystem(*exclude.codeSystemUrl, {}) : nullptr;
        bool caseSensitive = codeSystem == nullptr || codeSystem->isCaseSensitive();
        for (const auto& code : exclude.codes)
        {
            mCodes.erase(Code{.code = code, .caseSensitive = caseSensitive, .codeSystem = *exclude.codeSystemUrl});
        }
    }
    for (const auto& expand : mExpands)
    {
        const auto* codeSystem = repo->findCodeSystem(expand.codeSystemUrl, {});
        bool caseSensitive = codeSystem == nullptr || codeSystem->isCaseSensitive();
        mCodes.insert(Code{.code = expand.code, .caseSensitive = caseSensitive, .codeSystem = expand.codeSystemUrl});
    }
    if (mCodes.empty())
    {
        addError("ValueSet contains no codes after expansion");
        mCanValidate = false;
    }
    else
    {
        mCanValidate = true;
    }
    TVLOG(2) << "ValueSet \"" << mUrl << "\" finalized. Codes: " << mCodes.size() << ", canValidate: " << mCanValidate;
    if (! mValidationWarning.empty())
    {
        TLOG(WARNING) << "ValueSet \"" << mUrl << "\" warning(s): " << mValidationWarning;
    }
}
void FhirValueSet::finalizeIncludeFilters(const std::vector<FhirValueSet::Filter>& includeFilters,
                                          const std::string& codeSystemUrl, const FhirCodeSystem* codeSystem,
                                          bool caseSensitive)
{
    if (codeSystem && ! includeFilters.empty())
    {
        try
        {
            for (const auto& item : includeFilters)
            {
                std::vector<std::string> resolved;
                switch (item.mOp)
                {
                    case FilterOp::isA: {
                        resolved = codeSystem->resolveIsA(item.mValue, item.mProperty);
                        break;
                    }
                    case FilterOp::isNotA: {
                        resolved = codeSystem->resolveIsNotA(item.mValue, item.mProperty);
                        break;
                    }
                    case FilterOp::equals: {
                        resolved = codeSystem->resolveEquals(item.mValue, item.mProperty);
                        break;
                    }
                }
                for (const auto& code : resolved)
                {
                    mCodes.insert(Code{.code = code, .caseSensitive = caseSensitive, .codeSystem = codeSystemUrl});
                }
            }
        }
        catch (const std::runtime_error& re)
        {
            addError(re.what());
        }
    }
}
void FhirValueSet::finalizeIncludeCodes(const std::set<std::string>& codes, bool caseSensitive,
                                        const std::string& codeSystemUrl)
{
    if (! codes.empty())
    {
        for (const auto& code : codes)
        {
            mCodes.insert(Code{.code = code, .caseSensitive = caseSensitive, .codeSystem = codeSystemUrl});
        }
    }
}
// NOLINTNEXTLINE(misc-no-recursion)
void FhirValueSet::finalizeIncludeValueSets(FhirStructureRepository* repo, const std::set<std::string>& valueSets)
{
    for (const auto& valueSetUrl : valueSets)
    {
        auto* valueSet = repo->findValueSet(valueSetUrl, {});
        if (valueSet)
        {
            if (! valueSet->finalized())
            {
                valueSet->finalize(repo);
            }
            for (const auto& code : valueSet->getCodes())
            {
                mCodes.insert(code);
            }
        }
        else
        {
            addError("included ValueSet not found: " + valueSetUrl);
        }
    }
}

bool FhirValueSet::finalized() const
{
    return ! mCodes.empty();
}

bool FhirValueSet::canValidate() const
{
    return mCanValidate;
}

std::string FhirValueSet::getWarnings() const
{
    return mValidationWarning;
}

void FhirValueSet::addError(const std::string& error)
{
    if (! mValidationWarning.empty())
    {
        mValidationWarning += "; ";
    }
    mValidationWarning += error;
}

const std::set<FhirValueSet::Code>& FhirValueSet::getCodes() const
{
    return mCodes;
}

std::string FhirValueSet::codesToString() const
{
    std::ostringstream oss;
    std::string_view sep;
    for (const auto& item : mCodes)
    {
        oss << sep << item;
        sep = ", ";
    }
    return oss.str();
}


FhirValueSet::Builder::Builder()
    : mFhirValueSet(std::make_unique<FhirValueSet>())
{
}
FhirValueSet::Builder& FhirValueSet::Builder::url(const std::string& url)
{
    mFhirValueSet->mUrl = url;
    return *this;
}
FhirValueSet::Builder& FhirValueSet::Builder::name(const std::string& name)
{
    mFhirValueSet->mName = name;
    return *this;
}
FhirValueSet::Builder& FhirValueSet::Builder::version(const std::string& version)
{
    mFhirValueSet->mVersion = Version{version};
    return *this;
}
FhirValueSet::Builder& FhirValueSet::Builder::include()
{
    mFhirValueSet->mIncludes.emplace_back(FhirValueSet::IncludeOrExclude{});
    return *this;
}
FhirValueSet::Builder& FhirValueSet::Builder::includeCodeSystem(const std::string& system)
{
    mFhirValueSet->mIncludes.back().codeSystemUrl = system;
    return *this;
}
FhirValueSet::Builder& FhirValueSet::Builder::includeValueSet(const std::string& valueSet)
{
    mFhirValueSet->mIncludes.back().valueSets.insert(valueSet);
    return *this;
}
FhirValueSet::Builder& FhirValueSet::Builder::includeFilter()
{
    mFhirValueSet->mIncludes.back().filters.emplace_back();
    return *this;
}
FhirValueSet::Builder& FhirValueSet::Builder::includeFilterOp(const std::string& filter)
{
    if (filter == "is-a")
    {
        mFhirValueSet->mIncludes.back().filters.back().mOp = FilterOp::isA;
    }
    else if (filter == "is-not-a")
    {
        mFhirValueSet->mIncludes.back().filters.back().mOp = FilterOp::isNotA;
    }
    else if (filter == "=")
    {
        mFhirValueSet->mIncludes.back().filters.back().mOp = FilterOp::equals;
    }
    else
    {
        mFhirValueSet->addError("unsupported filter type: " + filter);
    }
    return *this;
}
FhirValueSet::Builder& FhirValueSet::Builder::includeFilterValue(const std::string& value)
{
    mFhirValueSet->mIncludes.back().filters.back().mValue = value;
    return *this;
}
FhirValueSet::Builder& FhirValueSet::Builder::includeFilterProperty(const std::string& value)
{
    mFhirValueSet->mIncludes.back().filters.back().mProperty = value;
    return *this;
}
FhirValueSet::Builder& FhirValueSet::Builder::includeCode(const std::string& code)
{
    auto [_, inserted] = mFhirValueSet->mIncludes.back().codes.insert(code);
    if (! inserted)
    {
        VLOG(2) << "duplicate includeCodeSystem code: " << code;
    }
    return *this;
}
FhirValueSet::Builder& FhirValueSet::Builder::exclude()
{
    mFhirValueSet->mExcludes.emplace_back(FhirValueSet::IncludeOrExclude{});
    return *this;
}
FhirValueSet::Builder& FhirValueSet::Builder::excludeCodeSystem(const std::string& system)
{
    mFhirValueSet->mExcludes.back().codeSystemUrl = system;
    return *this;
}
FhirValueSet::Builder& FhirValueSet::Builder::excludeCode(const std::string& code)
{
    auto [_, inserted] = mFhirValueSet->mExcludes.back().codes.insert(code);
    if (! inserted)
    {
        LOG(WARNING) << "duplicate excludeCodeSystem code: " << code;
    }
    return *this;
}
FhirValueSet::Builder& FhirValueSet::Builder::newExpand()
{
    mFhirValueSet->mExpands.emplace_back(Expansion{});
    return *this;
}
FhirValueSet::Builder& FhirValueSet::Builder::expandCode(const std::string& code)
{
    mFhirValueSet->mExpands.back().code = code;
    return *this;
}
FhirValueSet::Builder& FhirValueSet::Builder::expandSystem(const std::string& system)
{
    mFhirValueSet->mExpands.back().codeSystemUrl = system;
    return *this;
}
FhirValueSet FhirValueSet::Builder::getAndReset()
{
    FhirValueSet prev{std::move(*mFhirValueSet)};
    mFhirValueSet = std::make_unique<FhirValueSet>();
    return prev;
}

const std::string& FhirValueSet::Builder::getName() const
{
    return mFhirValueSet->getName();
}

std::ostream& operator<<(std::ostream& ostream, const fhirtools::FhirValueSet::Code& code)
{
    ostream << "[" << code.codeSystem << "]" << code.code;
    return ostream;
}
}
