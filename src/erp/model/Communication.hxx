/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_COMMUNICATION_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_COMMUNICATION_HXX

#include "erp/model/Resource.hxx"
#include "erp/model/CommunicationPayload.hxx"
#include "erp/model/Identity.hxx"
#include "erp/model/PrescriptionId.hxx"
#include "erp/model/Timestamp.hxx"
#include "erp/util/Uuid.hxx"
#include "erp/validation/SchemaType.hxx"

class JsonValidator;

namespace model
{
// NOLINTNEXTLINE(bugprone-exception-escape)
class Communication : public Resource<Communication, ResourceVersion::WorkflowOrPatientenRechnungProfile>
{
public:
    static constexpr auto resourceTypeName = "Communication";

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
    static SchemaType messageTypeToSchemaType(MessageType messageType);
    static MessageType schemaTypeToMessageType(SchemaType schemaType);
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

    bool canValidateGeneric() const;
    static bool canValidateGeneric(MessageType messageType, ResourceVersion::WorkflowOrPatientenRechnungProfile profile);

private:

    std::string_view messageTypeToProfileUrl(MessageType messageType) const;
    MessageType profileUrlToMessageType(std::string_view profileUrl) const;

    bool isDeprecatedProfile() const;
    friend Resource<Communication, ResourceVersion::WorkflowOrPatientenRechnungProfile>;
    explicit Communication(NumberAsStringParserDocument&& document); // internal ctor

    void setSender(std::string_view sender, std::string_view namingSystem);
    void setRecipient(std::string_view recipient, std::string_view namingSystem);
    std::optional<SchemaType> payloadSchema() const;

    CommunicationPayload mPayload;
};

} // namespace model

#endif
