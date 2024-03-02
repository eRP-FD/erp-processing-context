/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_BUNDLE_HXX
#define ERP_PROCESSING_CONTEXT_BUNDLE_HXX

#include "erp/model/Link.hxx"
#include "erp/model/PrescriptionId.hxx"
#include "erp/model/Resource.hxx"
#include "erp/model/Signature.hxx"
#include "erp/model/Timestamp.hxx"
#include "erp/server/response/ServerResponse.hxx"
#include "erp/util/Expect.hxx"

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

template <class DerivedBundle, typename SchemaVersionType = ResourceVersion::DeGematikErezeptWorkflowR4>
// NOLINTNEXTLINE(bugprone-exception-escape)
class BundleBase : public Resource<DerivedBundle, SchemaVersionType>
{
public:
    static constexpr auto resourceTypeName = "Bundle";

    enum class SearchMode
    {
        match,
        include,
        outcome
    };

    using Resource<DerivedBundle, SchemaVersionType>::Resource;
    using Resource<DerivedBundle, SchemaVersionType>::setValue;
    using Resource<DerivedBundle, SchemaVersionType>::setKeyValue;
    using Resource<DerivedBundle, SchemaVersionType>::addToArray;
    using Resource<DerivedBundle, SchemaVersionType>::getValueMember;
    using Resource<DerivedBundle, SchemaVersionType>::getValue;
    using Resource<DerivedBundle, SchemaVersionType>::getStringValue;
    using Resource<DerivedBundle, SchemaVersionType>::findStringInArray;

    BundleBase(BundleType type, ResourceBase::Profile profile);
    BundleBase(BundleType type, ResourceBase::Profile profile, const Uuid& bundleId);

    void setId(const Uuid& bundleId);

    /*
     * Add the given `resource` object to the /entry array.
     * Will create a {link:<resourceLnk>,resource:<resourceOject>} object.
     * Note that ownership of `resourceObject` is taken over because of rapidjson's own move semantics.
     */
    void addResource(const std::optional<std::string>& fullUrl,
                     const std::optional<std::string>& resourceLink,
                     const std::optional<SearchMode>& searchMode,
                     rapidjson::Value&& resourceObject);
    /**
     * Variant of addResource where a deep copy of the given resource object is created prior to
     * adding it to the /entry array.
     */
    void addResource(const std::optional<std::string>& fullUrl,
                     const std::optional<std::string>& resourceLink,
                     const std::optional<SearchMode>& searchMode,
                     const rapidjson::Value& resourceObject);

    [[nodiscard]] size_t getResourceCount(void) const;
    const rapidjson::Value& getResource (const size_t index) const;
    [[nodiscard]] std::string_view getResourceLink (const size_t index) const;

    template<typename TResource>
    [[nodiscard]] std::vector<TResource> getResourcesByType(std::string_view type = TResource::resourceTypeName) const;

    void setSignature (rapidjson::Value& signature);
    void setSignature(const model::Signature& signature);
    void removeSignature();

    std::optional<model::Signature> getSignature() const;

    // returns Bundle.signature.when
    [[nodiscard]] model::Timestamp getSignatureWhen() const;

    void setLink (Link::Type type, std::string_view link);
    [[nodiscard]] std::optional<std::string_view> getLink (Link::Type type) const;

    [[nodiscard]] Uuid getId () const;
    [[nodiscard]] size_t getTotalSearchMatches() const;
    [[nodiscard]] BundleType getBundleType() const;
    [[nodiscard]] PrescriptionId getIdentifier() const;
    void setIdentifier(const PrescriptionId& prescriptionId);

    void setTotalSearchMatches(std::size_t totalSearchMatches);

private:
    friend Resource<Bundle>;
};


template <class DerivedBundle, typename SchemaVersionType>
template<typename TResource>
// NOLINTNEXTLINE(bugprone-exception-escape)
std::vector<TResource> BundleBase<DerivedBundle, SchemaVersionType>::getResourcesByType (const std::string_view type) const
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

// NOLINTNEXTLINE(bugprone-exception-escape)
class Bundle final : public BundleBase<Bundle, ResourceVersion::NotProfiled>
{
public:
    static constexpr auto resourceTypeName = "Bundle";

    using BundleBase::BundleBase;
    using BundleBase::fromXml;
    using BundleBase::fromJson;

};

// NOLINTBEGIN(bugprone-exception-escape)
extern template class BundleBase<ErxReceipt>;
extern template class BundleBase<KbvBundle, ResourceVersion::KbvItaErp>;
extern template class BundleBase<Bundle, ResourceVersion::NotProfiled>;
extern template class BundleBase<MedicationDispenseBundle>;
extern template class BundleBase<Bundle, ResourceVersion::AbgabedatenPkv>;
// NOLINTEND(bugprone-exception-escape)

} // end of namespace model

#endif
