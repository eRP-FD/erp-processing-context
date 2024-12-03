/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_COMMUNICATION_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_COMMUNICATION_HXX

#include "shared/model/Resource.hxx"
#include "erp/model/CommunicationPayload.hxx"
#include "erp/model/Identity.hxx"
#include "shared/model/PrescriptionId.hxx"
#include "shared/model/Timestamp.hxx"
#include "shared/util/Uuid.hxx"
#include "shared/validation/SchemaType.hxx"

class JsonValidator;

namespace model
{
// NOLINTNEXTLINE(bugprone-exception-escape)
class Communication : public Resource<Communication>
{
public:
    static constexpr auto resourceTypeName = "Communication";
    // GEMREQ-start A_19450-01#acceptedCommunications
    static constexpr std::initializer_list<ProfileType> acceptedCommunications{
        // see ERP-21119 - CommunicationInfoReq currently disabled:
        //    ProfileType::Gem_erxCommunicationInfoReq,
        ProfileType::Gem_erxCommunicationReply,
        ProfileType::Gem_erxCommunicationDispReq,
        ProfileType::Gem_erxCommunicationRepresentative,
        ProfileType::Gem_erxCommunicationChargChangeReq,
        ProfileType::Gem_erxCommunicationChargChangeReply,
    };
    // GEMREQ-end A_19450-01#acceptedCommunications
    enum class MessageType : int8_t
    {
        InfoReq = 0,
        Reply = 1,
        DispReq = 2,
        Representative = 3,
        ChargChangeReq = 4,
        ChargChangeReply = 5
    };
    static std::string_view messageTypeToString(MessageType messageType);
    static int8_t messageTypeToInt(MessageType messageType);
    static ProfileType messageTypeToProfileType(MessageType messageType);
    static MessageType profileTypeToMessageType(ProfileType schemaType);

    static const rapidjson::Pointer resourceTypePointer;
    static const rapidjson::Pointer idPointer;
    static const rapidjson::Pointer metaProfile0Pointer;
    static const rapidjson::Pointer basedOn0ReferencePointer;
    static const rapidjson::Pointer sentPointer;
    static const rapidjson::Pointer receivedPointer;
    static const rapidjson::Pointer senderIdentifierSystemPointer;
    static const rapidjson::Pointer senderIdentifierValuePointer;
    static const rapidjson::Pointer recipientPointer;
    static const rapidjson::Pointer recipient0IdentifierSystemPointer;
    static const rapidjson::Pointer recipient0IdentifierValuePointer;
    static const rapidjson::Pointer payloadPointer;

    static const rapidjson::Pointer identifierSystemPointer;
    static const rapidjson::Pointer identifierValuePointer;

    MessageType messageType() const;
    bool isReply() const;
    std::string_view messageTypeAsString() const;
    std::string_view messageTypeAsProfileUrl() const;
    int8_t messageTypeAsInt() const;

    std::optional<Uuid> id() const;
    void setId(const Uuid& id);

    std::optional<Identity> sender() const;
    void setSender(const Identity& sender);
    void setSender(const Kvnr& sender);
    void setSender(const TelematikId& sender);

    Identity recipient() const;
    void setRecipient(const Identity& recipient);

    std::optional<model::Timestamp> timeSent() const;
    void setTimeSent(const model::Timestamp& timestamp = model::Timestamp::now());

    std::optional<model::Timestamp> timeReceived() const;
    void setTimeReceived(const model::Timestamp& timestamp = model::Timestamp::now());
    void deleteTimeReceived();

    PrescriptionId prescriptionId() const;
    std::optional<std::string> accessCode() const;

    /**
     * The content string is often used in tests.
     * Therefore the following methods have been provided for convenience.
     */
    std::optional<std::string_view> contentString() const;

    void verifyPayload(const JsonValidator& validator) const;


    std::optional<Timestamp> getValidationReferenceTimestamp() const override;
    ProfileType profileType() const;

private:
    static MessageType profileUrlToMessageType(std::string_view profileUrl);
    std::string_view messageTypeToProfileUrl(MessageType messageType) const;

    friend Resource<Communication>;
    explicit Communication(NumberAsStringParserDocument&& document); // internal ctor

    void setSender(std::string_view sender, std::string_view namingSystem);
    void setRecipient(std::string_view recipient, std::string_view namingSystem);
    std::optional<SchemaType> payloadSchema() const;
};

// NOLINTNEXTLINE(bugprone-exception-escape)
extern template class Resource<class Communication>;

} // namespace model

#endif
