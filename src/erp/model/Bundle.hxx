/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_BUNDLE_HXX
#define ERP_PROCESSING_CONTEXT_BUNDLE_HXX

//#include "erp/model/NumberAsStringParserDocument.hxx"
#include "erp/model/Link.hxx"
#include "erp/model/Resource.hxx"
#include "erp/model/Timestamp.hxx"
#include "erp/server/response/ServerResponse.hxx"
#include "erp/model/Signature.hxx"
#include "erp/util/Expect.hxx"

#include <rapidjson/document.h>
#include <rapidjson/pointer.h>
#include <vector>

class Uuid;

namespace model
{

class Bundle : public Resource<Bundle>
{
public:
    enum class SearchMode
    {
        match,
        include,
        outcome
    };

    // https://simplifier.net/packages/hl7.fhir.r4.core/4.0.1/files/80374
    enum class Type
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

    explicit Bundle (Type type);
    explicit Bundle (Type type, const Uuid& bundleId);

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
    [[nodiscard]] std::vector<TResource> getResourcesByType(std::string_view type) const;

    void setSignature (rapidjson::Value& signature);
    void setSignature(const model::Signature& signature);

    std::optional<model::Signature> getSignature() const;

    // returns Bundle.signature.when
    [[nodiscard]] Timestamp getSignatureWhen() const;

    void setLink (Link::Type type, std::string_view link);
    [[nodiscard]] std::optional<std::string_view> getLink (Link::Type type) const;

    [[nodiscard]] std::string_view getResourceType (void) const;
    [[nodiscard]] Uuid getId () const;
    [[nodiscard]] size_t getTotalSearchMatches() const;
    [[nodiscard]] Type getBundleType() const;

    void setTotalSearchMatches(std::size_t totalSearchMatches);

protected:
    explicit Bundle (NumberAsStringParserDocument&& document);

private:
    void increaseTotalSearchMatches();

    friend Resource<Bundle>;

};


template<typename TResource>
std::vector<TResource> Bundle::getResourcesByType (const std::string_view type) const
{
    std::vector<TResource> resources;

    const auto* entries = getValue(rapidjson::Pointer("/entry"));
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

} // end of namespace model

#endif
