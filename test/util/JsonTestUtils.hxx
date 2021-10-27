/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef EPR_PROCESSINIG_CONTEXT_JSONTESTUTILS_HXX
#define EPR_PROCESSINIG_CONTEXT_JSONTESTUTILS_HXX

#include "erp/model/Communication.hxx"

#include <rapidjson/document.h>
#include <string>

std::string canonicalJson(const std::string& json);
std::string jsonToString(const rapidjson::Document& json);

enum class ActorRole // see  TAB_SYSLERP_048 Fachliche Rollen in gemSysL_eRp_V1.1.0
{
    Insurant,       // Versicherter
    Representative, // Vertreter
    Doctor,         // Verordnende Akteure - Arzt, Zahnarzt (here also the staff of the medical institution)
    Pharmacists     // Abgebende Akteure - Apotheker und pharmazeutisches Personal
};
const std::string_view& actorRoleToResourceId(ActorRole actorRole);

/**
 * Named arguments following the C++ idiom "Named Parameter".
 */
class CommunicationJsonStringBuilder
{
public:
    explicit CommunicationJsonStringBuilder(model::Communication::MessageType messageType);
    std::string createJsonString() const;
    std::string createXmlString() const;
    CommunicationJsonStringBuilder& setPrescriptionId(const std::string& prescriptionId);
    CommunicationJsonStringBuilder& setAccessCode(const std::string& accessCode);
    CommunicationJsonStringBuilder& setSender(ActorRole actorRole, const std::string& sender);
    CommunicationJsonStringBuilder& setRecipient(ActorRole actorRole, const std::string& recipient);
    CommunicationJsonStringBuilder& setTimeSent(const std::string& timeSent);
    CommunicationJsonStringBuilder& setTimeReceived(const std::string& timeReceived);
    CommunicationJsonStringBuilder& setPayload(const std::string& contentString);
    CommunicationJsonStringBuilder& setAbout(const std::string& about);
    model::Communication::MessageType messageType() const { return mMessageType; }
    std::optional<std::string> prescriptionId() const { return mPrescriptionId; }
    std::optional<std::string> accessCode() const { return mAccessCode; }
    std::optional<std::tuple<ActorRole, std::string>> recipient() const { return mRecipient; }
    std::optional<std::tuple<ActorRole, std::string>> sender() const { return mSender; }
    std::optional<std::string> timeSent() const { return mTimeSent; }
    std::optional<std::string> timeReceived() const { return mTimeReceived; }
    std::optional<std::string> payload() const { return mPayload; }
    std::optional<std::string> about() const { return mAbout; }

private:
    model::Communication::MessageType                 mMessageType;       // ["InfoReq", "Reply", "DispReq", "Representative"]
    std::optional<std::string>                        mPrescriptionId;    // e.g. "160.123.456.789.123.58"
    std::optional<std::string>                        mAccessCode;        // e.g. "777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea"
    std::optional<std::tuple<ActorRole, std::string>> mSender;            // e.g. make_tuple(Actor::Pharmacists, "A87654321")
    std::optional<std::tuple<ActorRole, std::string>> mRecipient;         // e.g. make_tuple(Actor::Pharmacists, "A87654321")
    std::optional<std::string>                        mTimeSent;          // e.g. "2020-01-23T12:34"
    std::optional<std::string>                        mTimeReceived;      // e.g. "2020-01-23T12:34"
    std::optional<std::string>                        mPayload;           // e.g. "Hello, do you ..."
    std::optional<std::string>                        mAbout;
};

#endif // EPR_PROCESSINIG_CONTEXT_JSONTESTUTILS_HXX
