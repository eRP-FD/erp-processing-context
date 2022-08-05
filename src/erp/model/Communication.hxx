/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_COMMUNICATION_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_COMMUNICATION_HXX

#include "erp/model/CommunicationPayload.hxx"
#include "erp/model/PrescriptionId.hxx"
#include "fhirtools/model/Timestamp.hxx"
#include "erp/util/Uuid.hxx"
#include "erp/validation/SchemaType.hxx"

namespace model
{
class Communication : public Resource<Communication>
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
    static const std::string_view& messageTypeToString(MessageType messageType);
    static int8_t messageTypeToInt(MessageType messageType);
    static MessageType stringToMessageType(const std::string_view& messageType);
    static const std::string_view& messageTypeToProfileUrl(MessageType messageType);
    static MessageType profileUrlToMessageType(const std::string_view& profileUrl);
    static bool messageTypeHasPrescriptionId(MessageType messageType);
    static SchemaType messageTypeToSchemaType(MessageType messageType);

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
    const std::string_view& messageTypeAsString() const;
    const std::string_view& messageTypeAsProfileUrl() const;
    int8_t messageTypeAsInt() const;

    std::optional<Uuid> id() const;
    void setId(const Uuid& id);

    std::optional<std::string_view> sender() const;
    void setSender(const std::string_view& sender);

    std::optional<std::string_view> recipient() const;
    void setRecipient(const std::string_view& recipient);

    std::optional<fhirtools::Timestamp> timeSent() const;
    void setTimeSent(const fhirtools::Timestamp& timestamp = fhirtools::Timestamp::now());

    std::optional<fhirtools::Timestamp> timeReceived() const;
    void setTimeReceived(const fhirtools::Timestamp& timestamp = fhirtools::Timestamp::now());
    void deleteTimeReceived();

    PrescriptionId prescriptionId() const;
    std::optional<std::string> accessCode() const;

    /**
     * The content string is often used in tests.
     * Therefore the following methods have been provided for convenience.
     */
    std::optional<std::string_view> contentString(uint32_t idx = 0) const;

    void verifyPayload() const;

private:
    friend Resource<Communication>;
    explicit Communication(NumberAsStringParserDocument&& document); // internal ctor

    static std::string retrievePrescriptionIdFromTaskReference(const std::string_view& taskReference);
    static std::optional<std::string> retrieveAccessCodeFromTaskReference(const std::string_view& taskReference);

    CommunicationPayload mPayload;
};

} // namespace model

#endif
