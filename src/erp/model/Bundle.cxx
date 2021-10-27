/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/model/Bundle.hxx"

#include "erp/util/Expect.hxx"
#include "erp/util/Uuid.hxx"

#include <rapidjson/pointer.h>
#include <magic_enum.hpp>


using namespace rapidjson;

namespace
{
    constexpr std::string_view keyEntryLink ("link");
    constexpr std::string_view keyEntryResource ("resource");
    constexpr std::string_view valueLinkRelationSelf ("self");
    constexpr std::string_view valueLinkRelationPrev ("prev");
    constexpr std::string_view valueLinkRelationNext ("next");
    constexpr std::string_view valueResourceTypeBundle ("Bundle");

    Pointer idPointer ("/id");
    Pointer typePointer ("/type");
    Pointer timestampPointer("/timestamp");
    Pointer totalPointer("/total");
    Pointer linkPointer ("/link");
    Pointer fullUrlPointer ("/fullUrl");
    Pointer searchModePointer ("/search/mode");
    Pointer entryPointer ("/entry");
    Pointer relationPointer ("/relation");
    Pointer resourcePointer ("/resource");
    Pointer urlPointer ("/url");
    Pointer resourceTypePointer ("/resourceType");
    Pointer signaturePointer ("/signature");
    Pointer signatureWhenPointer ("/signature/when");

}

// Сustom definitions of names for enum.
// Specialization of `enum_name` must be injected in `namespace magic_enum::customize`.
template <>
constexpr std::string_view magic_enum::customize::enum_name<model::Bundle::Type>(model::Bundle::Type value) noexcept {
    switch (value) {
        case model::Bundle::Type::document:
        case model::Bundle::Type::message:
        case model::Bundle::Type::transaction:
        case model::Bundle::Type::batch:
        case model::Bundle::Type::history:
        case model::Bundle::Type::searchset:
        case model::Bundle::Type::collection:
            return {};
        case model::Bundle::Type::transaction_response:
            return "transaction-response";
        case model::Bundle::Type::batch_response:
            return "batch-response";
    }
    return {};
}


namespace model
{

Bundle::Bundle (const Type type, const Uuid& bundleId)
    : Resource<Bundle>()
{
    // The bundle ID:
    setId(bundleId);

    setValue(typePointer, magic_enum::enum_name(type));
    setValue(timestampPointer, Timestamp::now().toXsDateTime());

    // Set the resource type to "Bundle".
    setValue(resourceTypePointer, valueResourceTypeBundle);

    if (type == Type::searchset)
    {
        setValue(totalPointer, rapidjson::Value(gsl::narrow<uint64_t>(0u)));
    }
}


Bundle::Bundle (const Type type)
    : Bundle(type, Uuid())
{
}


Bundle::Bundle (NumberAsStringParserDocument&& document)
    : Resource<Bundle>(std::move(document))
{
    ModelExpect(getResourceType()==valueResourceTypeBundle, "resource type is not 'Bundle'");
}

void Bundle::setId(const Uuid& bundleId)
{
    setValue(idPointer, bundleId.toString());
}

void Bundle::addResource(const std::optional<std::string>& fullUrl,
                         const std::optional<std::string>& resourceLink,
                         const std::optional<SearchMode>& searchMode,
                         rapidjson::Value&& resourceObject)
{
    // Set up a new entry object.
    auto entry = createEmptyObject();
    if (resourceLink)
    {
        setKeyValue(entry, linkPointer, resourceLink.value());
    }
    if (fullUrl)
    {
        setKeyValue(entry, fullUrlPointer, fullUrl.value());
    }
    ModelExpect(resourceObject.IsObject(), "Invalid resource object type");
    setKeyValue(entry, resourcePointer, resourceObject);
    if (searchMode)
    {
        Expect3(getBundleType() == Type::searchset, "search mode shall only be set for searchsets", std::logic_error);
        switch (searchMode.value())
        {
            case SearchMode::match:
                setKeyValue(entry, searchModePointer, "match");
                increaseTotalSearchMatches();
                break;
            case SearchMode::include:
                setKeyValue(entry, searchModePointer, "include");
                break;
            case SearchMode::outcome:
                setKeyValue(entry, searchModePointer, "outcome");
                break;
        }
    }
    // Add it to the entry array;
    addToArray(entryPointer, std::move(entry));
}


void Bundle::addResource(const std::optional<std::string>& fullUrl,
                         const std::optional<std::string>& resourceLink,
                         const std::optional<SearchMode>& searchMode,
                         const rapidjson::Value& resourceObject)
{
    // Create a copy of `resourceObject` before forwarding to the non-const variant of `addResource`.
    auto value = copyValue(resourceObject);
    addResource(fullUrl, resourceLink, searchMode, std::move(value));
}


size_t Bundle::getResourceCount(void) const
{
    return valueSize(entryPointer);
}


const rapidjson::Value& Bundle::getResource (const size_t index) const
{
    const auto* arrayEntry = const_cast<Bundle*>(this)->getMemberInArray(entryPointer, index);
    ModelExpect(arrayEntry != nullptr, "resource entry does not exist");
    return getValueMember(*arrayEntry, keyEntryResource);
}


std::string_view Bundle::getResourceLink (const size_t index) const
{
    const auto* arrayEntry = const_cast<Bundle*>(this)->getMemberInArray(entryPointer, index);
    ModelExpect(arrayEntry != nullptr, "resource entry does not exist");
    const auto& value = getValueMember(*arrayEntry, keyEntryLink);
    return NumberAsStringParserDocument::getStringValueFromValue(&value);
}


void Bundle::setSignature (rapidjson::Value& signature)
{
    setValue(signaturePointer, signature);
}


void Bundle::setSignature(const model::Signature& signature)
{
    auto value = copyValue(signature.jsonDocument());
    setSignature(value);
}


std::optional<model::Signature> Bundle::getSignature() const
{
    const auto* signature = getValue(signaturePointer);
    if (signature != nullptr)
    {
        return model::Signature::fromJson(*signature);
    }
    return {};
}


Timestamp Bundle::getSignatureWhen () const
{
    return Timestamp::fromXsDateTime(std::string{getStringValue(signatureWhenPointer)});
}


void Bundle::setLink (const Link::Type linkType, const std::string_view linkText)
{
    std::string_view valueLinkRelation;
    switch(linkType)
    {
        case Link::Type::Self:
            valueLinkRelation = valueLinkRelationSelf;
            break;
        case Link::Type::Prev:
            valueLinkRelation = valueLinkRelationPrev;
            break;
        case Link::Type::Next:
            valueLinkRelation = valueLinkRelationNext;
            break;
    }
    auto* value = findMemberInArray(linkPointer, relationPointer, valueLinkRelation);

    if (value == nullptr)
    {
        // Link does not yet exist. Create a new one.
        auto link = createEmptyObject();
        setKeyValue(link, relationPointer, valueLinkRelation);
        setKeyValue(link, urlPointer, linkText);
        // Add it to the link array.
        addToArray(linkPointer, std::move(link));
    }
    else
    {
        // Link does already exist. Just change its value.
        setKeyValue(*value, urlPointer, linkText);
    }
}


std::optional<std::string_view> Bundle::getLink (const Link::Type type) const
{
    switch(type)
    {
        case Link::Type::Self: return findStringInArray(linkPointer, relationPointer, valueLinkRelationSelf, urlPointer);
        case Link::Type::Prev: return findStringInArray(linkPointer, relationPointer, valueLinkRelationPrev, urlPointer);
        case Link::Type::Next: return findStringInArray(linkPointer, relationPointer, valueLinkRelationNext, urlPointer);
        default:             return {};
    }
}


std::string_view Bundle::getResourceType (void) const
{
    return getStringValue(resourceTypePointer);
}

Uuid Bundle::getId() const
{
    return Uuid(std::string(getStringValue(idPointer)));
}

size_t Bundle::getTotalSearchMatches() const
{
    auto optionalTotal = getOptionalInt64Value(totalPointer);
    return optionalTotal.value_or(0);
}

Bundle::Type Bundle::getBundleType() const
{
    auto strVal = getStringValue(typePointer);
    auto enumVal = magic_enum::enum_cast<Type>(strVal);
    Expect(enumVal.has_value(), "Type of Bundle could not be determined");
    return *enumVal;
}

void Bundle::setTotalSearchMatches(std::size_t totalSearchMatches)
{
    Expect3(getBundleType() == Type::searchset, "total shall only be set for searchsets", std::logic_error);
    setValue(totalPointer, rapidjson::Value(totalSearchMatches));
}

void Bundle::increaseTotalSearchMatches()
{
    auto current = getTotalSearchMatches();
    setValue(totalPointer, rapidjson::Value(gsl::narrow<uint64_t>(current+1)));
}

} // end of namespace model
