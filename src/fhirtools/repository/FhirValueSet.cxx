/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "FhirValueSet.hxx"
#include "FhirCodeSystem.hxx"
#include "fhirtools/FPExpect.hxx"
#include "groups/FhirResourceGroup.hxx"
#include "views/FhirStructureRepositoryView.hxx"

#include <boost/algorithm/string/case_conv.hpp>

namespace fhirtools
{

FhirCodeSystemCodes FhirValueSet::Filter::filter(const FhirCodeSystemCodes& codes) const
{
    switch (mOp)
    {
        case FilterOp::isA: {
            return codes.resolveIsA(mValue, mProperty);
        }
        case FilterOp::isNotA: {
            return codes.resolveIsNotA(mValue, mProperty);
        }
        case FilterOp::equals: {
            return codes.resolveEquals(mValue, mProperty);
        }
    }
    return {codes.key(), {}, codes.caseSensitive(), codes.synthesized()};
}

bool FhirValueSetCodes::Code::operator==(const FhirValueSetCodes::Code& other) const
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
std::strong_ordering FhirValueSetCodes::Code::operator<=>(const FhirValueSetCodes::Code& other) const
{
    auto order = caseSensitive || other.caseSensitive ? code <=> other.code
                                                      : boost::to_lower_copy(code) <=> boost::to_lower_copy(other.code);
    if (order == std::strong_ordering::equal && ! codeSystem.empty() && ! other.codeSystem.empty())
    {
        return codeSystem <=> other.codeSystem;
    }
    return order;
}

DefinitionKey FhirValueSet::key() const
{
    return {mUrl, mVersion};
}

const std::string& FhirValueSet::getUrl() const
{
    return mUrl;
}
const std::string& FhirValueSet::getName() const
{
    return mName;
}
const std::filesystem::path& fhirtools::FhirValueSet::sourceFile() const
{
    return mSourceFile;
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

std::shared_ptr<const FhirValueSetCodes>
FhirValueSetCodes::create(gsl::not_null<const FhirStructureRepositoryView*> view,
                          gsl::not_null<const FhirValueSet*> valueSet)
{
    struct Construct : FhirValueSetCodes {
        Construct(gsl::not_null<const FhirStructureRepositoryView*> view, gsl::not_null<const FhirValueSet*> valueSet)
            : FhirValueSetCodes{std::move(view), std::move(valueSet)}
        {
        }
    };
    return std::make_shared<Construct>(std::move(view), std::move(valueSet));
}

fhirtools::FhirValueSetCodes::FhirValueSetCodes(gsl::not_null<const FhirStructureRepositoryView*> view,
                                                gsl::not_null<const FhirValueSet*> valueSet)
    : mValueSet{std::move(valueSet)}
    , mValidationWarning{mValueSet->getWarnings()}
    , mCanValidate{mValueSet->canValidate()}
    , mHasErrors{mValueSet->hasErrors()}
{
    finalize(view);
}

DefinitionKey FhirValueSetCodes::key() const
{
    return mValueSet->key();
}

bool FhirValueSetCodes::containsCode(const std::string& code) const
{
    return mCodes.contains(Code{.code = code, .caseSensitive = false, .codeSystem = {}});
}

bool FhirValueSetCodes::containsCode(const std::string& code, const std::string& codeSystem) const
{
    return mCodes.contains(Code{.code = code, .caseSensitive = false, .codeSystem = codeSystem});
}

void FhirValueSetCodes::addError(const std::string& error)
{
    if (! mValidationWarning.empty())
    {
        mValidationWarning += "; ";
    }
    mValidationWarning += error;
}

void FhirValueSetCodes::finalize(const FhirStructureRepositoryView* repo)// NOLINT(misc-no-recursion)
{
    using namespace std::string_literals;
    FPExpect3(mValueSet->resourceGroup(), "Missing group in ValueSet: " + to_string(mValueSet->key()), std::logic_error);
    finalizeIncludes(repo);
    finalizeExcludes(repo);
    std::set<DefinitionKey> unresolvedCodeSystems;
    for (const auto& expand : mValueSet->getExpands())
    {
        const auto* codeSystem = repo->findCodeSystem(expand.codeSystemKey);
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
    TVLOG(2) << "ValueSet \"" << mValueSet->key() << "\" finalized. Codes: " << mCodes.size()
             << ", canValidate: " << mCanValidate << " Warning(s): " << mValidationWarning;
}

// NOLINTNEXTLINE(misc-no-recursion)
void FhirValueSetCodes::finalizeIncludes(const FhirStructureRepositoryView* repo)
{
    using namespace std::string_literals;
    for (const auto& include : mValueSet->getIncludes())
    {
        std::shared_ptr<const FhirCodeSystemCodes> codeSystem = nullptr;
        if (include.codeSystemKey)
        {
            codeSystem = repo->findCodeSystemCodes(*include.codeSystemKey);
        }
        FPExpect(! codeSystem || include.valueSets.empty(),
                 to_string(mValueSet->key()) +
                     ": Not Implemented case: ValueSet.compose.include with both CodeSystem and ValueSets");
        bool caseSensitive = codeSystem == nullptr || codeSystem->caseSensitive();
        finalizeIncludeValueSets(repo, include.valueSets);
        finalizeIncludeCodes(include.codes, caseSensitive, include.codeSystemKey ? include.codeSystemKey->url : "");
        finalizeIncludeFilters(include.filters, codeSystem.get());
        if (codeSystem && include.codes.empty() && include.filters.empty())
        {
            for (const auto& code : *codeSystem)
            {
                mCodes.insert(
                    Code{.code = code.code, .caseSensitive = caseSensitive, .codeSystem = include.codeSystemKey->url});
            }
            if (codeSystem->synthesized())
            {
                addError("include CodeSystem " + to_string(*include.codeSystemKey) +
                         " was synthesized and may be incomplete and will not be used for validation");
                mCanValidate = false;
            }
        }
    }
}

void FhirValueSetCodes::finalizeExcludes(const FhirStructureRepositoryView* repo)
{
    using namespace std::string_literals;
    for (const auto& exclude : mValueSet->getExcludes())
    {
        FPExpect(exclude.valueSets.empty(), to_string(mValueSet->key()) + ": Not implemented: ValueSet.compose.exclude.ValueSet");
        FPExpect(exclude.codeSystemKey, to_string(mValueSet->key()) + ": ValueSet.compose.exclude.CodeSystem missing");
        const auto& codes = repo->findCodeSystemCodes(*exclude.codeSystemKey);
        if (! codes)
        {
            LOG(INFO) << "CodeSystem excluded from " << mValueSet->key() <<
                " not found in view(" << repo->id() << "): " << exclude.codeSystemKey->url;
        }
        const bool caseSensitive = codes == nullptr || codes->caseSensitive();
        finalizeExcludeCodes(exclude.codes, caseSensitive, exclude.codeSystemKey->url);
        finalizeExcludeFilters(exclude.filters, codes.get());
        if (codes && exclude.codes.empty() && exclude.filters.empty())
        {
            for (const auto& code : *codes)
            {
                mCodes.erase(
                    Code{.code = code.code, .caseSensitive = caseSensitive, .codeSystem = exclude.codeSystemKey->url});
            }
            if (codes->synthesized())
            {
                addError("exclude CodeSystem " + to_string(*exclude.codeSystemKey) +
                         " was synthesized and may be incomplete and will not be used for validation");
                mCanValidate = false;
            }
        }
    }
}

void FhirValueSetCodes::finalizeIncludeFilters(const std::vector<FhirValueSet::Filter>& includeFilters,
                                               const FhirCodeSystemCodes* codes)
{
    // empty or synthesized CodeSystems are incomplete, and will not be used for Validation.
    // using them for filtered includes, is not necessary and also produces some warnings
    // such as "CodeSystem: http://loinc.org: Unsupported property for =: SCALE_TYP" that don't matter
    // to us, because we don't use the CodeSystem anyway.
    if (codes && ! codes->empty() && ! codes->synthesized() && ! includeFilters.empty())
    {
        try
        {
            for (const auto& filter : includeFilters)
            {
                for (const auto& code : filter.filter(*codes))
                {
                    mCodes.insert(Code{.code = code.code,
                                       .caseSensitive = codes->caseSensitive(),
                                       .codeSystem = codes->key().url});
                }
            }
        }
        catch (const std::runtime_error& re)
        {
            addError(re.what());
        }
    }
}
void FhirValueSetCodes::finalizeIncludeCodes(const std::set<std::string>& codes, bool caseSensitive,
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
void FhirValueSetCodes::finalizeIncludeValueSets(const FhirStructureRepositoryView* repo,
                                            const std::set<DefinitionKey>& valueSets)
{
    using namespace std::string_literals;
    for (const auto& valueSetKey : valueSets)
    {
        if (const auto& codes = repo->findValueSetCodes(valueSetKey))
        {
            addError(codes->getWarnings());
            if (codes->hasErrors())
            {
                mHasErrors = true;
            }
            if (! codes->canValidate())
            {
                mCanValidate = false;
            }
            for (const auto& code : codes->getCodes())
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

void FhirValueSetCodes::finalizeExcludeCodes(const std::set<std::string>& codes, bool caseSensitive,
                                        const std::string& codeSystemUrl)
{
    for (const auto& code : codes)
    {
        mCodes.erase(Code{.code = code, .caseSensitive = caseSensitive, .codeSystem = codeSystemUrl});
    }
}

void FhirValueSetCodes::finalizeExcludeFilters(const std::vector<FhirValueSet::Filter>& excludeFilters,
                                               const class FhirCodeSystemCodes* codes)
{
    if (codes && ! codes->empty() && ! codes->synthesized() && ! excludeFilters.empty())
    {
        try
        {
            for (const auto& filter : excludeFilters)
            {
                for (const auto& code : filter.filter(*codes))
                {
                    mCodes.erase(Code{.code = code.code,
                                      .caseSensitive = codes->caseSensitive(),
                                      .codeSystem = codes->key().url});
                }
            }
        }
        catch (const std::runtime_error& re)
        {
            addError(re.what());
        }
    }
}

bool FhirValueSet::canValidate() const
{
    return mCanValidate;
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

const std::set<FhirValueSetCodes::Code>& FhirValueSetCodes::getCodes() const
{
    return mCodes;
}

std::string FhirValueSetCodes::codesToString() const
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

bool fhirtools::FhirValueSetCodes::canValidate() const
{
    return mCanValidate;
}

std::string fhirtools::FhirValueSetCodes::getWarnings() const
{
    return mValidationWarning;
}

bool fhirtools::FhirValueSetCodes::hasErrors() const
{
    return mHasErrors;
}

const FhirValueSet& fhirtools::FhirValueSetCodes::valueSet() const
{
    return *mValueSet;
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

FhirValueSet::Builder& FhirValueSet::Builder::sourceFile(std::filesystem::path path)
{
    mFhirValueSet->mSourceFile = std::move(path);
    return *this;
}

FhirValueSet::Builder& FhirValueSet::Builder::initGroup(const FhirResourceGroupResolver& resolver)
{
    mFhirValueSet->mGroup = resolver.findGroup(*mFhirValueSet);
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
FhirValueSet::Builder& FhirValueSet::Builder::excludeFilter()
{
    mFhirValueSet->mExcludes.back().filters.emplace_back();
    return *this;
}
FhirValueSet::Builder& FhirValueSet::Builder::excludeFilterOp(const std::string& filter)
{
    if (filter == "is-a")
    {
        mFhirValueSet->mExcludes.back().filters.back().mOp = FilterOp::isA;
    }
    else if (filter == "is-not-a")
    {
        mFhirValueSet->mExcludes.back().filters.back().mOp = FilterOp::isNotA;
    }
    else if (filter == "=")
    {
        mFhirValueSet->mExcludes.back().filters.back().mOp = FilterOp::equals;
    }
    else
    {
        mFhirValueSet->addError("unsupported filter type: " + filter);
    }
    return *this;
}
FhirValueSet::Builder& FhirValueSet::Builder::excludeFilterValue(const std::string& value)
{
    mFhirValueSet->mExcludes.back().filters.back().mValue = value;
    return *this;
}
FhirValueSet::Builder& FhirValueSet::Builder::excludeFilterProperty(const std::string& value)
{
    mFhirValueSet->mExcludes.back().filters.back().mProperty = value;
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
void fhirtools::FhirValueSet::Builder::reset()
{
    mFhirValueSet = std::make_unique<FhirValueSet>();
}

FhirValueSet FhirValueSet::Builder::getAndReset()
{
    FhirValueSet prev{std::move(*mFhirValueSet)};
    reset();
    Expect(prev.mGroup, "Missing group in ValueSet:" + prev.getUrl() + '|' + to_string(prev.getVersion()));
    return prev;
}

const std::string& FhirValueSet::Builder::getName() const
{
    return mFhirValueSet->getName();
}

DefinitionKey FhirValueSet::Builder::key()
{
    return mFhirValueSet->key();
}

std::ostream& operator<<(std::ostream& ostream, const fhirtools::FhirValueSetCodes::Code& code)
{
    ostream << "[" << code.codeSystem << "]" << code.code;
    return ostream;
}
}
