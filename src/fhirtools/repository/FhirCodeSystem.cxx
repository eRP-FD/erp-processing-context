/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "fhirtools/FPExpect.hxx"
#include "fhirtools/repository/DefinitionKey.hxx"
#include "fhirtools/repository/FhirCodeSystem.hxx"
#include "fhirtools/repository/groups/FhirResourceGroup.hxx"
#include "fhirtools/repository/views/FhirStructureRepositoryView.hxx"
#include "shared/util/Version.hxx"

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <ranges>

namespace fhirtools
{

const std::string& FhirCodeSystem::getUrl() const
{
    return mUrl;
}
const std::string& FhirCodeSystem::getName() const
{
    return mName;
}
const FhirVersion& FhirCodeSystem::getVersion() const
{
    return mVersion;
}

DefinitionKey FhirCodeSystem::key() const
{
    return {mUrl, mVersion};
}

const std::filesystem::path& fhirtools::FhirCodeSystem::sourceFile() const
{
    return mSourceFile;
}

std::shared_ptr<const FhirResourceGroup> fhirtools::FhirCodeSystem::resourceGroup() const
{
    return mGroup;
}

bool FhirCodeSystem::isCaseSensitive() const
{
    return mCaseSensitive;
}

// NOLINTNEXTLINE(misc-no-recursion)
FhirCodeSystemCodes fhirtools::FhirCodeSystem::getCodes(const FhirStructureRepositoryView& view) const
{
    const auto& supplementers = view.findSupplementers(mUrl, mVersion);
    std::map<std::string, Code> codes;
    std::ranges::transform(mCodes, inserter(codes, codes.begin()),
                           [](const Code& code) -> std::pair<std::string, Code> {
                               return {code.code, code};
                           });
    for (const FhirCodeSystem* supplementer: supplementers)
    {
        for (const auto& supplemantalCode: supplementer->getCodes(view))
        {
            const auto [code, inserted] = codes.try_emplace(supplemantalCode.code, supplemantalCode);
            if (!inserted)
            {
                std::ranges::copy(supplemantalCode.isA, back_inserter(code->second.isA));
                std::ranges::copy(supplemantalCode.properties, back_inserter(code->second.properties));
                Expect(supplemantalCode.parent.empty() || supplemantalCode.parent == code->second.parent,
                       supplementer->sourceFile().native() + ": supplemantal code has different code hierarchy: [" +
                           to_string(supplementer->key()) + "]" + supplemantalCode.code);
            }
        }
    }
    std::vector<FhirCodeSystem::Code> resultCodes;
    std::ranges::move(codes | std::views::values, std::back_inserter(resultCodes));
    return {{mUrl, mVersion}, std::move(resultCodes), mCaseSensitive, mSynthesized};
}

bool FhirCodeSystem::isEmpty() const
{
    return mCodes.empty();
}
FhirCodeSystem::ContentType FhirCodeSystem::getContentType() const
{
    return mContentType;
}
const std::optional<DefinitionKey>& FhirCodeSystem::getSupplements() const
{
    return mSupplements;
}
FhirCodeSystemCodes FhirCodeSystemCodes::resolveIsA(const std::string& value, const std::string& property) const
{
    if (property == "concept")
    {
        return resolveIsAConcept(value);
    }
    else if (property == "parent")
    {
        return resolveIsAParent(value);
    }
    else
    {
        FPFail("Unsupported property for is-a: " + property + " codeSystem: " + to_string(mKey));
    }
}

FhirCodeSystemCodes FhirCodeSystemCodes::resolveIsAParent(const std::string& value) const
{
    FhirCodeSystemCodes ret{mKey, {}, mCaseSensitive, mSynthesized};
    for (const auto& item : (*this))
    {
        if (! item.parent.empty())
        {
            if (isA(getCode(item.parent), value))
            {
                ret.push_back(item);
            }
        }
    }
    return ret;
}

FhirCodeSystemCodes FhirCodeSystemCodes::resolveIsAConcept(const std::string& value) const
{
    FhirCodeSystemCodes ret{mKey, {}, mCaseSensitive, mSynthesized};
    for (const auto& item : *this)
    {
        bool equals =
            mCaseSensitive ? item.code == value : boost::to_lower_copy(value) == boost::to_lower_copy(item.code);
        if (equals || isA(item, value))
        {
            ret.push_back(item);
        }
    }
    return ret;
}

FhirCodeSystemCodes FhirCodeSystemCodes::resolveIsNotA(const std::string& value, const std::string& property) const
{
    FhirCodeSystemCodes ret{mKey, {}, mCaseSensitive, mSynthesized};
    if (property == "concept")
    {
        for (const auto& item : *this)
        {
            if (! isA(item, value))
            {
                ret.push_back(item);
            }
        }
    }
    else if (property == "parent")
    {
        for (const auto& item : *this)
        {
            if (! item.parent.empty())
            {
                if (! isA(getCode(item.parent), value))
                {
                    ret.push_back(item);
                }
            }
        }
    }
    else
    {
        FPFail("Unsupported property for is-not-a: " + property);
    }
    return ret;
}

bool FhirCodeSystemCodes::isA(const FhirCodeSystem::Code& code, const std::string& value)
{
    return std::ranges::find(code.isA, value) != code.isA.end();
}

const FhirCodeSystem::Code& FhirCodeSystemCodes::getCode(const std::string& code) const
{
    auto result = std::ranges::find(*this, code, &FhirCodeSystem::Code::code);
    FPExpect(result != end(), "Could not find code " + code);
    return *result;
}

FhirCodeSystemCodes FhirCodeSystemCodes::resolveEquals(const std::string& value, const std::string& property) const
{
    const auto isParentProperty = property == "parent";
    FhirCodeSystemCodes ret{mKey, {}, mCaseSensitive, mSynthesized};
    for (const auto& code : *this)
    {
        if (isParentProperty && ! code.parent.empty())
        {
            if (getCode(code.parent).code == value)
            {
                ret.push_back(code);
            }
        }
        if (! isParentProperty)
        {
            for (const auto& codeProperty : code.properties)
            {
                if (property == codeProperty.code && value == codeProperty.value)
                {
                    ret.emplace_back(code);
                }
            }
        }
    }
    return ret;
}
bool FhirCodeSystem::isSynthesized() const
{
    return mSynthesized;
}

FhirCodeSystemCodes::FhirCodeSystemCodes(DefinitionKey key, std::vector<FhirCodeSystem::Code> codes, bool caseSensitive,
                                         bool synthesized)
    : vector{std::move(codes)}
    , mKey{std::move(key)}
    , mCaseSensitive{caseSensitive}
    , mSynthesized{synthesized}
{
}

const DefinitionKey& FhirCodeSystemCodes::key() const
{
    return mKey;
}

bool FhirCodeSystemCodes::caseSensitive() const
{
    return mCaseSensitive;
}

bool FhirCodeSystemCodes::synthesized() const
{
    return mSynthesized;
}

bool FhirCodeSystemCodes::containsCode(std::string_view code) const
{
    if (mCaseSensitive)
    {
        return end() != std::ranges::find_if(*this, [&code](const auto& c) {
                   return c.code == code;
               });
    }
    else
    {
        return end() != std::ranges::find_if(*this, [&code](const auto& c) {
                   return boost::algorithm::iequals(code, c.code);
               });
    }
    return false;
}


FhirCodeSystem::Builder::Builder()
    : mCodeSystem(std::make_unique<FhirCodeSystem>())
{
}
FhirCodeSystem::Builder::Builder(const FhirCodeSystem& codeSystem)
    : mCodeSystem(std::make_unique<FhirCodeSystem>(codeSystem))
{
}
FhirCodeSystem::Builder& FhirCodeSystem::Builder::url(const std::string& url)
{
    mCodeSystem->mUrl = url;
    return *this;
}
FhirCodeSystem::Builder& FhirCodeSystem::Builder::name(const std::string& name)
{
    mCodeSystem->mName = name;
    return *this;
}
FhirCodeSystem::Builder& FhirCodeSystem::Builder::version(FhirVersion version)
{
    mCodeSystem->mVersion = std::move(version);
    return *this;
}

FhirCodeSystem::Builder& FhirCodeSystem::Builder::sourceFile(std::filesystem::path path)
{
    mCodeSystem->mSourceFile = std::move(path);
    return *this;
}

FhirCodeSystem::Builder& FhirCodeSystem::Builder::initGroup(const FhirResourceGroupResolver& resolver)
{
    mCodeSystem->mGroup = resolver.findGroup(*mCodeSystem);
    return *this;
}

FhirCodeSystem::Builder& FhirCodeSystem::Builder::caseSensitive(const std::string& caseSensitive)
{
    if (caseSensitive == "true")
    {
        mCodeSystem->mCaseSensitive = true;
    }
    else if (caseSensitive == "false")
    {
        mCodeSystem->mCaseSensitive = false;
    }
    else
    {
        FPFail("invalid string value for caseSensitive: " + caseSensitive);
    }
    return *this;
}
FhirCodeSystem::Builder& FhirCodeSystem::Builder::code(const std::string& code)
{
    mCodeSystem->mCodes.push_back(FhirCodeSystem::Code{.code = code,
                                                       .isA = mConceptStack,
                                                       .parent = mConceptStack.empty() ? "" : mConceptStack.back(),
                                                       .properties = {}});
    mConceptStack.push_back(code);
    return *this;
}
FhirCodeSystem::Builder& FhirCodeSystem::Builder::contentType(const std::string& content)
{
    if (content == "not-present")
    {
        mCodeSystem->mContentType = ContentType::not_present;
    }
    else
    {
        auto contentType = magic_enum::enum_cast<FhirCodeSystem::ContentType>(content);
        FPExpect(contentType.has_value(), "invalid CodeSystem.content: " + content);
        mCodeSystem->mContentType = *contentType;
    }
    return *this;
}
FhirCodeSystem::Builder& FhirCodeSystem::Builder::supplements(const DefinitionKey& key)
{
    FPExpect(mCodeSystem->mContentType == ContentType::supplement,
             "CodeSystem.supplement only expected for CodeSystem.content=supplement, but is " +
                 std::string{magic_enum::enum_name(mCodeSystem->mContentType)});
    mCodeSystem->mSupplements = key;
    return *this;
}
FhirCodeSystem::Builder& FhirCodeSystem::Builder::synthesized()
{
    mCodeSystem->mSynthesized = true;
    return *this;
}
FhirCodeSystem::Builder& FhirCodeSystem::Builder::addProperty()
{
    mCodeSystem->mCodes.back().properties.emplace_back();
    return *this;
}
FhirCodeSystem::Builder& FhirCodeSystem::Builder::propertyCode(const std::string& code)
{
    mCodeSystem->mCodes.back().properties.back().code = code;
    return *this;
}
FhirCodeSystem::Builder& FhirCodeSystem::Builder::propertyValue(const std::string& value)
{
    mCodeSystem->mCodes.back().properties.back().value = value;
    return *this;
}
void fhirtools::FhirCodeSystem::Builder::reset()
{
    mCodeSystem = std::make_unique<FhirCodeSystem>();
    mConceptStack.clear();
}
FhirCodeSystem FhirCodeSystem::Builder::getAndReset()
{
    FhirCodeSystem prev{std::move(*mCodeSystem)};
    reset();
    FPExpect3(prev.mGroup, "Missing group in CodeSystem: " + prev.getUrl() + '|' + to_string(prev.getVersion()),
              std::logic_error);
    return prev;
}
FhirCodeSystem::Builder& FhirCodeSystem::Builder::popConcept()
{
    FPExpect(! mConceptStack.empty(), "concept stack is empty");
    mConceptStack.pop_back();
    return *this;
}

}
