/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/model/Bundle.hxx"

#include "erp/model/ErxReceipt.hxx"
#include "erp/model/KbvBundle.hxx"
#include "erp/model/MedicationDispenseBundle.hxx"
#include "erp/model/ResourceVersion.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/Uuid.hxx"

#include <magic_enum.hpp>
#include <rapidjson/pointer.h>


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
    Pointer prescriptionIdPointer ("/identifier/value");

}

// Сustom definitions of names for enum.
// Specialization of `enum_name` must be injected in `namespace magic_enum::customize`.
template <>
constexpr std::string_view magic_enum::customize::enum_name<model::BundleType>(typename model::BundleType value) noexcept {
    switch (value) {
        case model::BundleType::document:
        case model::BundleType::message:
        case model::BundleType::transaction:
        case model::BundleType::batch:
        case model::BundleType::history:
        case model::BundleType::searchset:
        case model::BundleType::collection:
            return {};
        case model::BundleType::transaction_response:
            return "transaction-response";
        case model::BundleType::batch_response:
            return "batch-response";
    }
    return {};
}


namespace model
{

template<class DerivedBundle, typename SchemaVersionType>
BundleBase<DerivedBundle, SchemaVersionType>::BundleBase(const BundleType type, ResourceBase::Profile profile,
                                                         const Uuid& bundleId)
    : Resource<DerivedBundle, SchemaVersionType>(profile)
{
    // The bundle ID:
    setId(bundleId);

    setValue(typePointer, magic_enum::enum_name(type));
    setValue(timestampPointer, Timestamp::now().toXsDateTime());

    // Set the resource type to "Bundle".
    setValue(resourceTypePointer, valueResourceTypeBundle);

    if (type == BundleType::searchset)
    {
        setValue(totalPointer, rapidjson::Value(gsl::narrow<uint64_t>(0u)));
    }
}


template<class DerivedBundle, typename SchemaVersionType>
BundleBase<DerivedBundle, SchemaVersionType>::BundleBase(const BundleType type, ResourceBase::Profile profile)
    : BundleBase<DerivedBundle, SchemaVersionType>(type, profile, Uuid())
{
}


template <class DerivedBundle, typename SchemaVersionType>
void BundleBase<DerivedBundle, SchemaVersionType>::setId(const Uuid& bundleId)
{
    setValue(idPointer, bundleId.toString());
}

template <class DerivedBundle, typename SchemaVersionType>
void BundleBase<DerivedBundle, SchemaVersionType>::addResource(const std::optional<std::string>& fullUrl,
                         const std::optional<std::string>& resourceLink,
                         const std::optional<SearchMode>& searchMode,
                         rapidjson::Value&& resourceObject)
{
    // Set up a new entry object.
    auto entry = this->createEmptyObject();
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
        Expect3(getBundleType() == BundleType::searchset, "search mode shall only be set for searchsets", std::logic_error);
        switch (searchMode.value())
        {
            case SearchMode::match:
                setKeyValue(entry, searchModePointer, "match");
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


template <class DerivedBundle, typename SchemaVersionType>
void BundleBase<DerivedBundle, SchemaVersionType>::addResource(const std::optional<std::string>& fullUrl,
                         const std::optional<std::string>& resourceLink,
                         const std::optional<SearchMode>& searchMode,
                         const rapidjson::Value& resourceObject)
{
    // Create a copy of `resourceObject` before forwarding to the non-const variant of `addResource`.
    auto value = this->copyValue(resourceObject);
    addResource(fullUrl, resourceLink, searchMode, std::move(value));
}


template <class DerivedBundle, typename SchemaVersionType>
size_t BundleBase<DerivedBundle, SchemaVersionType>::getResourceCount(void) const
{
    return this->valueSize(entryPointer);
}


template <class DerivedBundle, typename SchemaVersionType>
const rapidjson::Value& BundleBase<DerivedBundle, SchemaVersionType>::getResource (const size_t index) const
{
    const auto* arrayEntry = this->getMemberInArray(entryPointer, index);
    ModelExpect(arrayEntry != nullptr, "resource entry does not exist");
    return getValueMember(*arrayEntry, keyEntryResource);
}


template <class DerivedBundle, typename SchemaVersionType>
std::string_view BundleBase<DerivedBundle, SchemaVersionType>::getResourceLink (const size_t index) const
{
    const auto* arrayEntry = this->getMemberInArray(entryPointer, index);
    ModelExpect(arrayEntry != nullptr, "resource entry does not exist");
    const auto& value = getValueMember(*arrayEntry, keyEntryLink);
    return NumberAsStringParserDocument::getStringValueFromValue(&value);
}


template <class DerivedBundle, typename SchemaVersionType>
void BundleBase<DerivedBundle, SchemaVersionType>::setSignature (rapidjson::Value& signature)
{
    setValue(signaturePointer, signature);
}


template <class DerivedBundle, typename SchemaVersionType>
void BundleBase<DerivedBundle, SchemaVersionType>::setSignature(const model::Signature& signature)
{
    auto value = this->copyValue(signature.jsonDocument());
    setSignature(value);
}


template <class DerivedBundle, typename SchemaVersionType>
std::optional<model::Signature> BundleBase<DerivedBundle, SchemaVersionType>::getSignature() const
{
    const auto* signature = getValue(signaturePointer);
    if (signature != nullptr)
    {
        return model::Signature::fromJson(*signature);
    }
    return {};
}


template <class DerivedBundle, typename SchemaVersionType>
Timestamp BundleBase<DerivedBundle, SchemaVersionType>::getSignatureWhen () const
{
    return Timestamp::fromXsDateTime(std::string{getStringValue(signatureWhenPointer)});
}


template <class DerivedBundle, typename SchemaVersionType>
void BundleBase<DerivedBundle, SchemaVersionType>::setLink (const Link::Type linkType, const std::string_view linkText)
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
    auto* value = this->findMemberInArray(linkPointer, relationPointer, valueLinkRelation);

    if (value == nullptr)
    {
        // Link does not yet exist. Create a new one.
        auto link = this->createEmptyObject();
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


template <class DerivedBundle, typename SchemaVersionType>
std::optional<std::string_view> BundleBase<DerivedBundle, SchemaVersionType>::getLink (const Link::Type type) const
{
    switch(type)
    {
        case Link::Type::Self: return findStringInArray(linkPointer, relationPointer, valueLinkRelationSelf, urlPointer);
        case Link::Type::Prev: return findStringInArray(linkPointer, relationPointer, valueLinkRelationPrev, urlPointer);
        case Link::Type::Next: return findStringInArray(linkPointer, relationPointer, valueLinkRelationNext, urlPointer);
        default:             return {};
    }
}


template <class DerivedBundle, typename SchemaVersionType>
std::string_view BundleBase<DerivedBundle, SchemaVersionType>::getResourceType (void) const
{
    return getStringValue(resourceTypePointer);
}

template <class DerivedBundle, typename SchemaVersionType>
Uuid BundleBase<DerivedBundle, SchemaVersionType>::getId() const
{
    return Uuid(std::string(getStringValue(idPointer)));
}

template <class DerivedBundle, typename SchemaVersionType>
size_t BundleBase<DerivedBundle, SchemaVersionType>::getTotalSearchMatches() const
{
    auto optionalTotal = this->getOptionalInt64Value(totalPointer);
    return optionalTotal.value_or(0);
}

template <class DerivedBundle, typename SchemaVersionType>
BundleType BundleBase<DerivedBundle, SchemaVersionType>::getBundleType() const
{
    auto strVal = getStringValue(typePointer);
    auto enumVal = magic_enum::enum_cast<BundleType>(strVal);
    Expect(enumVal.has_value(), "Type of Bundle could not be determined");
    return *enumVal;
}

template <class DerivedBundle, typename SchemaVersionType>
PrescriptionId BundleBase<DerivedBundle, SchemaVersionType>::getIdentifier() const
{
    const auto prescriptionIdStr = getStringValue(prescriptionIdPointer);
    return PrescriptionId::fromString(prescriptionIdStr);
}

template <class DerivedBundle, typename SchemaVersionType>
void BundleBase<DerivedBundle, SchemaVersionType>::setTotalSearchMatches(std::size_t totalSearchMatches)
{
    Expect3(getBundleType() == BundleType::searchset, "total shall only be set for searchsets", std::logic_error);
    setValue(totalPointer, rapidjson::Value(totalSearchMatches));
}


template class BundleBase<ErxReceipt>;
template class BundleBase<KbvBundle, ResourceVersion::KbvItaErp>;
template class BundleBase<Bundle>;
template class BundleBase<MedicationDispenseBundle, ResourceVersion::NotProfiled>;

} // end of namespace model
