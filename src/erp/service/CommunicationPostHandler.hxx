/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_COMMUNICATIONPOSTHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_COMMUNICATIONPOSTHANDLER_HXX

#include "erp/service/ErpRequestHandler.hxx"
#include "erp/model/Communication.hxx"

namespace model
{
class Communication;
}

class CommunicationPostHandler : public ErpRequestHandler
{
public:
    CommunicationPostHandler(const std::initializer_list<std::string_view>& allowedProfessionOiDs);
    void handleRequest(PcSessionContext& session) override;
private:
    void validateAgainstFhirProfile(
        model::Communication::MessageType messageType,
        const model::Communication& communication,
        const XmlValidator& xmlValidator,
        const InCodeValidator& inCodeValidator) const;
    void validateSender(
        model::Communication::MessageType messageType,
        const std::string& professionOid,
        const std::string& sender) const;
    void validateRecipient(
        model::Communication::MessageType messageType,
        const std::string& recipient) const;
    void checkEligibilityOfInsurant(
        model::Communication::MessageType messageType,
        const std::string_view& sender,
        const std::string_view& taskKvnr,
        const std::optional<std::string>& headerAccessCode,
        const std::string& taskAccessCode,
        const std::optional<std::string>& communicationAccessCode) const;
    void checkVerificationIdentitiesOfKvnrs(
        model::Communication::MessageType messageType,
        const std::string_view& taskKvnr,
        const std::optional<std::string_view>& sender,
        const std::string_view& recipient) const;

    uint64_t mMaxMessageCount;
};


#endif //ERP_PROCESSING_CONTEXT_COMMUNICATIONPOSTHANDLER_HXX
