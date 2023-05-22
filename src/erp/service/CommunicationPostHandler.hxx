/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_COMMUNICATIONPOSTHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_COMMUNICATIONPOSTHANDLER_HXX

#include "erp/service/ErpRequestHandler.hxx"
#include "erp/model/Communication.hxx"
#include "erp/model/Identity.hxx"

#include <variant>

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
    void validateInfoRequest(const model::Communication& communication, const XmlValidator& xmlValidator,
                             const InCodeValidator& inCodeValidator) const;

    model::Identity validateSender(
        model::Communication::MessageType messageType,
        const std::string& professionOid,
        const std::string& sender) const;
    void validateRecipient(
        model::Communication::MessageType messageType,
        const model::Identity& recipient) const;
    void checkEligibilityOfInsurant(
        model::Communication::MessageType messageType,
        const model::Kvnr& sender,
        const model::Kvnr& taskKvnr,
        const std::optional<std::string>& headerAccessCode,
        const std::string& taskAccessCode,
        const std::optional<std::string>& communicationAccessCode) const;
    void checkVerificationIdentitiesOfKvnrs(
        model::Communication::MessageType messageType,
        const model::Kvnr& taskKvnr,
        const model::Identity& sender,
        const model::Identity& recipient) const;

    uint64_t mMaxMessageCount;
    void checkForChargeItemReference(const model::PrescriptionId& prescriptionId,
                                     const model::Communication::MessageType& messageType,
                                     const Database* databaseHandle) const;
};


#endif //ERP_PROCESSING_CONTEXT_COMMUNICATIONPOSTHANDLER_HXX
