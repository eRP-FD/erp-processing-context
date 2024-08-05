/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "fhirtools/repository/FhirCodeSystem.hxx"
#include "erp/util/Version.hxx"
#include "fhirtools/FPExpect.hxx"
#include "fhirtools/repository/FhirResourceGroup.hxx"

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/predicate.hpp>

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

std::shared_ptr<const FhirResourceGroup> fhirtools::FhirCodeSystem::resourceGroup() const
{
    return mGroup;
}

bool FhirCodeSystem::isCaseSensitive() const
{
    return mCaseSensitive;
}
const std::vector<FhirCodeSystem::Code>& FhirCodeSystem::getCodes() const
{
    return mCodes;
}
bool FhirCodeSystem::isEmpty() const
{
    return mCodes.empty();
}
FhirCodeSystem::ContentType FhirCodeSystem::getContentType() const
{
    return mContentType;
}
const std::string& FhirCodeSystem::getSupplements() const
{
    return mSupplements;
}
std::vector<std::string> FhirCodeSystem::resolveIsA(const std::string& value, const std::string& property) const
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
        FPFail("Unsupported property for is-a: " + property);
    }
}

std::vector<std::string> FhirCodeSystem::resolveIsAParent(const std::string& value) const
{
    std::vector<std::string> ret;
    for (const auto& item : mCodes)
    {
        if (! item.parent.empty())
        {
            if (isA(getCode(item.parent), value))
            {
                ret.push_back(item.parent);
            }
        }
    }
    return ret;
}

std::vector<std::string> FhirCodeSystem::resolveIsAConcept(const std::string& value) const
{
    std::vector<std::string> ret;
    for (const auto& item : mCodes)
    {
        bool equals =
            mCaseSensitive ? item.code == value : boost::to_lower_copy(value) == boost::to_lower_copy(item.code);
        if (equals || isA(item, value))
        {
            ret.push_back(item.code);
        }
    }
    return ret;
}

std::vector<std::string> FhirCodeSystem::resolveIsNotA(const std::string& value, const std::string& property) const
{
    std::vector<std::string> ret;
    if (property == "concept")
    {
        for (const auto& item : mCodes)
        {
            if (! isA(item, value))
            {
                ret.push_back(item.code);
            }
        }
    }
    else if (property == "parent")
    {
        for (const auto& item : mCodes)
        {
            if (! item.parent.empty())
            {
                if (! isA(getCode(item.parent), value))
                {
                    ret.push_back(item.code);
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
bool FhirCodeSystem::isA(const FhirCodeSystem::Code& code, const std::string& value) const
{
    return std::find(code.isA.begin(), code.isA.end(), value) != code.isA.end();
}
const FhirCodeSystem::Code& FhirCodeSystem::getCode(const std::string& code) const
{
    for (const auto& item : mCodes)
    {
        if (item.code == code)
        {
            return item;
        }
    }
    FPFail("Could not find code " + code);
}
std::vector<std::string> FhirCodeSystem::resolveEquals(const std::string& value, const std::string& property) const
{
    FPExpect(property == "parent", "CodeSystem: " + mUrl + ": Unsupported property for =: " + property);
    std::vector<std::string> ret;
    for (const auto& item : mCodes)
    {
        if (! item.parent.empty())
        {
            if (getCode(item.parent).code == value)
            {
                ret.push_back(item.code);
            }
        }
    }
    return ret;
}
bool FhirCodeSystem::isSynthesized() const
{
    return mSynthesized;
}
bool FhirCodeSystem::containsCode(const std::string_view& code) const
{
    if (isCaseSensitive())
    {
        return mCodes.end() != std::find_if(mCodes.begin(), mCodes.end(), [&code](const auto& c) {
                   return c.code == code;
               });
    }
    else
    {
        return mCodes.end() != std::find_if(mCodes.begin(), mCodes.end(), [&code](const auto& c) {
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

FhirCodeSystem::Builder& FhirCodeSystem::Builder::initGroup(const FhirResourceGroupResolver& resolver,
                                                            const std::filesystem::path& source)
{
    mCodeSystem->mGroup = resolver.findGroup(mCodeSystem->getUrl(), mCodeSystem->getVersion(), source);
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
                                                       .parent = mConceptStack.empty() ? "" : mConceptStack.back()});
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
FhirCodeSystem::Builder& FhirCodeSystem::Builder::supplements(const std::string& canonical)
{
    FPExpect(mCodeSystem->mContentType == ContentType::supplement,
             "CodeSystem.supplement only expected for CodeSystem.content=supplement, but is " +
                 std::string{magic_enum::enum_name(mCodeSystem->mContentType)});
    mCodeSystem->mSupplements = canonical;
    return *this;
}
FhirCodeSystem::Builder& FhirCodeSystem::Builder::synthesized()
{
    mCodeSystem->mSynthesized = true;
    return *this;
}
FhirCodeSystem FhirCodeSystem::Builder::getAndReset()
{
    FhirCodeSystem prev{std::move(*mCodeSystem)};
    mCodeSystem = std::make_unique<FhirCodeSystem>();
    mConceptStack.clear();
    FPExpect3(prev.mGroup, "group not set for: " + prev.getUrl() + '|' + to_string(prev.getVersion()),
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
