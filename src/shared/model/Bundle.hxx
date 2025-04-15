/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_BUNDLE_HXX
#define ERP_PROCESSING_CONTEXT_BUNDLE_HXX

#include "shared/model/Link.hxx"
#include "shared/model/Signature.hxx"
#include "shared/model/Timestamp.hxx"
#include "shared/server/response/ServerResponse.hxx"
#include "shared/util/Expect.hxx"
#include "shared/model/PrescriptionId.hxx"
#include "shared/model/Resource.hxx"

#include <rapidjson/document.h>
#include <rapidjson/pointer.h>
#include <vector>


class Uuid;

namespace model
{

// https://simplifier.net/packages/hl7.fhir.r4.core/4.0.1/files/80374
enum class BundleType
{
    document,            // The bundle is a document. The first resource is a Composition.
    message,             // The bundle is a message. The first resource is a MessageHeader.
    transaction,         // The bundle is a transaction - intended to be processed by a server as an atomic commit.
    transaction_response,// The bundle is a transaction response. Because the response is a transaction response,
                         //  the transaction has succeeded, and all responses are error free.
    batch,               // The bundle is a set of actions - intended to be processed by a server as a group of
                         //  independent actions
    batch_response,      // The bundle is a batch response. Note that as a batch, some responses may indicate
                         //  failure and others success.
    history,             // The bundle is a list of resources from a history interaction on a server.
    searchset,           // The bundle is a list of resources returned as a result of a search/query interaction,
                         //  operation, or message.
    collection           // The bundle is a set of resources collected into a single package for ease of
                         //  distribution that imposes no processing obligations or behavioral rules beyond
                         //  persistence.
};

enum class BundleSearchMode
{
    match,
    include,
    outcome
};

template<class DerivedBundle>
// NOLINTNEXTLINE(bugprone-exception-escape)
class BundleBase : public Resource<DerivedBundle>
{
public:
    static constexpr auto resourceTypeName = "Bundle";

    using SearchMode = BundleSearchMode;

    using Resource<DerivedBundle>::Resource;
    using Resource<DerivedBundle>::setValue;
    using Resource<DerivedBundle>::setKeyValue;
    using Resource<DerivedBundle>::addToArray;
    using Resource<DerivedBundle>::getValueMember;
    using Resource<DerivedBundle>::getValue;
    using Resource<DerivedBundle>::getStringValue;
    using Resource<DerivedBundle>::findStringInArray;

    BundleBase(BundleType type, FhirResourceBase::Profile profile);
    BundleBase(BundleType type, FhirResourceBase::Profile profile, const Uuid& bundleId); //NOLINT(performance-unnecessary-value-param)

    void setId(const Uuid& bundleId);

    /*
     * Add the given `resource` object to the /entry array.
     * Will create a {link:<resourceLnk>,resource:<resourceOject>} object.
     * Note that ownership of `resourceObject` is taken over because of rapidjson's own move semantics.
     */
    void addResource(const std::optional<std::string>& fullUrl, const std::optional<std::string>& resourceLink,
                     const std::optional<SearchMode>& searchMode, rapidjson::Value&& resourceObject);
    /**
     * Variant of addResource where a deep copy of the given resource object is created prior to
     * adding it to the /entry array.
     */
    void addResource(const std::optional<std::string>& fullUrl, const std::optional<std::string>& resourceLink,
                     const std::optional<SearchMode>& searchMode, const rapidjson::Value& resourceObject);

    [[nodiscard]] rapidjson::Value::ConstArray getEntries() const;
    [[nodiscard]] size_t getResourceCount(void) const;
    const rapidjson::Value& getResource(const size_t index) const;
    [[nodiscard]] std::string_view getResourceLink(const size_t index) const;

    template<typename TResource>
    [[nodiscard]] std::vector<TResource> getResourcesByType(std::string_view type = TResource::resourceTypeName) const;
    template<typename TResource>
    [[nodiscard]] TResource getUniqueResourceByType(std::string_view type = TResource::resourceTypeName) const;

    void setSignature(rapidjson::Value& signature);
    void setSignature(const model::Signature& signature);
    void removeSignature();

    std::optional<model::Signature> getSignature() const;

    // returns Bundle.signature.when
    [[nodiscard]] model::Timestamp getSignatureWhen() const;

    void setLink(Link::Type type, std::string_view link);
    [[nodiscard]] std::optional<std::string_view> getLink(Link::Type type) const;

    [[nodiscard]] Uuid getId() const;
    [[nodiscard]] size_t getTotalSearchMatches() const;
    [[nodiscard]] BundleType getBundleType() const;
    [[nodiscard]] PrescriptionId getIdentifier() const;
    void setIdentifier(const PrescriptionId& prescriptionId);

    void setTotalSearchMatches(std::size_t totalSearchMatches);

private:
    friend Resource<class Bundle>;
};


template<class DerivedBundle>
template<typename TResource>
// NOLINTNEXTLINE(bugprone-exception-escape)
std::vector<TResource> BundleBase<DerivedBundle>::getResourcesByType(const std::string_view type) const
{
    std::vector<TResource> resources;

    const auto* entries = this->getValue(rapidjson::Pointer("/entry"));
    ModelExpect(entries && entries->IsArray(), "entry array not present in Bundle");
    for (auto entry = entries->Begin(), end = entries->End(); entry != end; ++entry)
    {
        const auto* resourceType = rapidjson::Pointer("/resource/resourceType").Get(*entry);
        ModelExpect(resourceType, "Missing resourceType in Bundle entry");
        if (NumberAsStringParserDocument::getStringValueFromValue(resourceType) == type)
        {
            const auto* resource = rapidjson::Pointer("/resource").Get(*entry);
            NumberAsStringParserDocument doc;
            doc.CopyFrom(*resource, doc.GetAllocator());
            resources.emplace_back(std::move(TResource::fromJson(doc)));
        }
    }

    return resources;
}

template<class DerivedBundle>
template<typename TResource>
TResource BundleBase<DerivedBundle>::getUniqueResourceByType(std::string_view type) const
{
    auto resources = getResourcesByType<TResource>(type);
    ModelExpect(resources.size() == 1, "Expected exactly one resource of type " + std::string{type} +
                                           ", but the bundle contains " + std::to_string(resources.size()));
    return std::move(resources.front());
}

// NOLINTNEXTLINE(bugprone-exception-escape)
class Bundle final : public BundleBase<Bundle>
{
public:
    static constexpr auto profileType = ProfileType::fhir;

    using BundleBase::BundleBase;
    using BundleBase::fromXml;
    using BundleBase::fromJson;

    std::optional<Timestamp> getValidationReferenceTimestamp() const override;
};

// NOLINTBEGIN(bugprone-exception-escape)
extern template class BundleBase<Bundle>;
extern template class Resource<Bundle>;
// NOLINTEND(bugprone-exception-escape)

}// end of namespace model

#endif
