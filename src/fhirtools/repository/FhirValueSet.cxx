/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "FhirValueSet.hxx"
#include "FhirResourceGroup.hxx"
#include "FhirStructureRepository.hxx"
#include "fhirtools/FPExpect.hxx"

#include <boost/algorithm/string/case_conv.hpp>

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
const FhirVersion& FhirValueSet::getVersion() const
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

void FhirValueSet::finalize(FhirStructureRepositoryBackend* repo)// NOLINT(misc-no-recursion)
{
    using namespace std::string_literals;
    Expect3(! mFinalized, "ValueSet finalized more than once!", std::logic_error);
    FPExpect3(mGroup, "Missing group in ValueSet: " + mUrl + '|' + to_string(mVersion), std::logic_error);
    finalizeIncludes(repo);
    finalizeExcludes(repo);
    std::set<DefinitionKey> unresolvedCodeSystems;
    for (const auto& expand : mExpands)
    {
        auto codeSystemKey = mGroup->find(expand.codeSystemKey).first;
        const auto* codeSystem = codeSystemKey.version.has_value()
                                     ? repo->findCodeSystem(codeSystemKey.url, *codeSystemKey.version)
                                     : nullptr;
        bool caseSensitive = codeSystem == nullptr || codeSystem->isCaseSensitive();
        mCodes.insert(
            Code{.code = expand.code, .caseSensitive = caseSensitive, .codeSystem = expand.codeSystemKey.url});
        if (! codeSystem && unresolvedCodeSystems.insert(expand.codeSystemKey).second)
        {
            addError("Unresolved CodeSystem " + to_string(expand.codeSystemKey));
            mHasErrors = true;
        }
    }
    if (mCodes.empty())
    {
        addError("ValueSet contains no codes after expansion");
        mCanValidate = false;
    }
    TVLOG(2) << "ValueSet \"" << mUrl << "|" << mVersion << "\" finalized. Codes: " << mCodes.size()
             << ", canValidate: " << mCanValidate << " Warning(s): " << mValidationWarning;
    mFinalized = true;
}

// NOLINTNEXTLINE(misc-no-recursion)
void FhirValueSet::finalizeIncludes(FhirStructureRepositoryBackend* repo)
{
    using namespace std::string_literals;
    for (const auto& include : mIncludes)
    {
        const FhirCodeSystem* codeSystem = nullptr;
        if (include.codeSystemKey)
        {
            auto codeSystemKey = mGroup->find(*include.codeSystemKey).first;
            codeSystem = codeSystemKey.version.has_value()
                             ? repo->findCodeSystem(codeSystemKey.url, *codeSystemKey.version)
                             : nullptr;
        }
        FPExpect(! codeSystem || include.valueSets.empty(),
                 mName + ": Not Implemented case: ValueSet.compose.include with both CodeSystem and ValueSets");
        bool caseSensitive = codeSystem == nullptr || codeSystem->isCaseSensitive();
        finalizeIncludeValueSets(repo, include.valueSets);
        finalizeIncludeCodes(include.codes, caseSensitive, include.codeSystemKey ? include.codeSystemKey->url : "");
        finalizeIncludeFilters(include.filters, include.codeSystemKey ? include.codeSystemKey->url : "", codeSystem,
                               caseSensitive);
        if (codeSystem && include.codes.empty() && include.filters.empty())
        {
            for (const auto& code : codeSystem->getCodes())
            {
                mCodes.insert(
                    Code{.code = code.code, .caseSensitive = caseSensitive, .codeSystem = include.codeSystemKey->url});
            }
            if (codeSystem->isSynthesized())
            {
                addError("CodeSystem " + to_string(*include.codeSystemKey) +
                         " was synthesized and may be incomplete and will not be used for validation");
                mCanValidate = false;
            }
        }
    }
}

void FhirValueSet::finalizeExcludes(FhirStructureRepositoryBackend* repo)
{
    using namespace std::string_literals;
    for (auto& exclude : mExcludes)
    {
        FPExpect(exclude.valueSets.empty(), mName + ": Not implemented: ValueSet.compose.exclude.ValueSet");
        FPExpect(exclude.codeSystemKey, mName + ": ValueSet.compose.exclude.CodeSystem missing");
        auto codeSystemKey = mGroup->find(*exclude.codeSystemKey).first;
        const FhirCodeSystem* codeSystem = nullptr;
        if (codeSystemKey.version.has_value())
        {
            codeSystem = repo->findCodeSystem(codeSystemKey.url, *codeSystemKey.version);
        }
        else
        {
            LOG(INFO) << "CodeSystem not found in group(" << mGroup->id() << "): " << exclude.codeSystemKey->url;
        }
        bool caseSensitive = codeSystem == nullptr || codeSystem->isCaseSensitive();
        for (const auto& code : exclude.codes)
        {
            mCodes.erase(Code{.code = code, .caseSensitive = caseSensitive, .codeSystem = exclude.codeSystemKey->url});
        }
    }
}

void FhirValueSet::finalizeIncludeFilters(const std::vector<FhirValueSet::Filter>& includeFilters,
                                          const std::string& codeSystemUrl, const FhirCodeSystem* codeSystem,
                                          bool caseSensitive)
{
    // empty or synthesized CodeSystems are incomplete, and will not be used for Validation.
    // using them for filtered includes, is not necessary and also produces some warnings
    // such as "CodeSystem: http://loinc.org: Unsupported property for =: SCALE_TYP" that don't matter
    // to us, because we don't use the CodeSystem anyway.
    if (codeSystem && ! codeSystem->isEmpty() && ! codeSystem->isSynthesized() && ! includeFilters.empty())
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
void FhirValueSet::finalizeIncludeValueSets(FhirStructureRepositoryBackend* repo,
                                            const std::set<DefinitionKey>& valueSets)
{
    using namespace std::string_literals;
    for (auto valueSetKey : valueSets)
    {
        valueSetKey = mGroup->find(valueSetKey).first;
        if (! valueSetKey.version)
        {
            std::ostringstream msg;
            msg << "ValueSet included by " << mUrl << '|' << mVersion << " not found in group(" << mGroup->id()
                << "): " << valueSetKey.url;
            LOG(INFO) << msg.view();
            addError(msg.str());
        }
        else if (auto* valueSet = repo->findValueSet(valueSetKey.url, *valueSetKey.version))
        {
            if (! valueSet->finalized())
            {
                valueSet->finalize(repo);
            }
            addError(valueSet->getWarnings());
            if (valueSet->hasErrors())
            {
                mHasErrors = true;
            }
            if (! valueSet->canValidate())
            {
                mCanValidate = false;
            }
            for (const auto& code : valueSet->getCodes())
            {
                mCodes.insert(code);
            }
        }
        else
        {
            addError("included ValueSet not found: " + to_string(valueSetKey));
        }
    }
}

bool FhirValueSet::finalized() const
{
    return mFinalized;
}

bool FhirValueSet::canValidate() const
{
    return mFinalized && mCanValidate;
}

bool fhirtools::FhirValueSet::hasErrors() const
{
    return mHasErrors;
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

std::shared_ptr<const FhirResourceGroup> fhirtools::FhirValueSet::resourceGroup() const
{
    return mGroup;
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
FhirValueSet::Builder& FhirValueSet::Builder::version(FhirVersion version)
{
    mFhirValueSet->mVersion = std::move(version);
    return *this;
}
FhirValueSet::Builder& FhirValueSet::Builder::initGroup(const FhirResourceGroupResolver& resolver,
                                                        const std::filesystem::path& source)
{
    mFhirValueSet->mGroup = resolver.findGroup(mFhirValueSet->mUrl, mFhirValueSet->mVersion, source);
    return *this;
}
FhirValueSet::Builder& FhirValueSet::Builder::include()
{
    mFhirValueSet->mIncludes.emplace_back(FhirValueSet::IncludeOrExclude{});
    return *this;
}
FhirValueSet::Builder& FhirValueSet::Builder::includeCodeSystem(DefinitionKey system)
{
    mFhirValueSet->mIncludes.back().codeSystemKey = std::move(system);
    return *this;
}
FhirValueSet::Builder& FhirValueSet::Builder::includeValueSet(DefinitionKey valueSet)
{
    mFhirValueSet->mIncludes.back().valueSets.emplace(std::move(valueSet));
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
        TVLOG(2) << "duplicate includeCodeSystem code: " << code;
    }
    return *this;
}
FhirValueSet::Builder& FhirValueSet::Builder::exclude()
{
    mFhirValueSet->mExcludes.emplace_back(FhirValueSet::IncludeOrExclude{});
    return *this;
}
FhirValueSet::Builder& FhirValueSet::Builder::excludeCodeSystem(DefinitionKey system)
{
    mFhirValueSet->mExcludes.back().codeSystemKey = std::move(system);
    return *this;
}
FhirValueSet::Builder& FhirValueSet::Builder::excludeCode(const std::string& code)
{
    auto [_, inserted] = mFhirValueSet->mExcludes.back().codes.insert(code);
    if (! inserted)
    {
        TLOG(WARNING) << "duplicate excludeCodeSystem code: " << code;
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
FhirValueSet::Builder& FhirValueSet::Builder::expandSystem(DefinitionKey system)
{
    mFhirValueSet->mExpands.back().codeSystemKey = std::move(system);
    return *this;
}
FhirValueSet FhirValueSet::Builder::getAndReset()
{
    FhirValueSet prev{std::move(*mFhirValueSet)};
    mFhirValueSet = std::make_unique<FhirValueSet>();
    Expect(prev.mGroup, "Missing group in ValueSet:" + prev.getUrl() + '|' + to_string(prev.getVersion()));
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
