/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "CommunicationPostHandler.hxx"
#include "erp/model/ChargeItem.hxx"
#include "erp/model/Task.hxx"
#include "erp/service/SubscriptionPostHandler.hxx"
#include "fhirtools/model/erp/ErpElement.hxx"
#include "fhirtools/parser/FhirPathParser.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/crypto/SignedPrescription.hxx"
#include "shared/model/KbvBundle.hxx"
#include "shared/model/KbvMedicationRequest.hxx"
#include "shared/model/ResourceNames.hxx"
#include "shared/model/extensions/KBVMultiplePrescription.hxx"
#include "shared/server/request/ServerRequest.hxx"
#include "shared/server/response/ServerResponse.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/Demangle.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/TLog.hxx"


using namespace model;
using namespace model::resource;

namespace
{

void checkTaskReadyNotExpiredAndMvoAlreadyStarted(const Task& task, const std::optional<Binary>& healthCareProviderPrescription)
{
    A_26320.start("task must be ready for DispReq");
    ErpExpect(task.status() == Task::Status::ready, HttpStatus::BadRequest, "Task has invalid status.");
    A_26320.finish();

    A_26321.start("Task must not have expired.");
    const auto today = Timestamp::fromGermanDate(Timestamp::now().toGermanDate());
    ErpExpect(task.expiryDate() >= today, HttpStatus::BadRequest, "Task is expired.");
    A_26321.finish();

    A_26327.start("Task MVO must have valid start date");
    if (healthCareProviderPrescription.has_value())
    {
        const auto cadesBesSignature =
            SignedPrescription::fromBinNoVerify(std::string{*healthCareProviderPrescription.value().data()});
        const auto& prescription = cadesBesSignature.payload();
        const auto prescriptionBundle = model::KbvBundle::fromXmlNoValidation(prescription);
        const auto& medicationRequests = prescriptionBundle.getResourcesByType<model::KbvMedicationRequest>();
        if (! medicationRequests.empty())
        {
            const auto& medicationRequest = medicationRequests[0];
            const auto mvoExtension = medicationRequest.getExtension<model::KBVMultiplePrescription>();

            if (medicationRequest.isMultiplePrescription())
            {
                const auto start = mvoExtension->startDate();
                ErpExpect(start.value() <= today, HttpStatus::BadRequest, "Prescription is not fillable yet.");
            }
        }
    }
    A_26327.finish();
}

}

CommunicationPostHandler::CommunicationPostHandler(const std::initializer_list<std::string_view>& allowedProfessionOiDs)
    : ErpRequestHandler(Operation::POST_Communication, allowedProfessionOiDs)
    , mMaxMessageCount(gsl::narrow<uint64_t>(
          Configuration::instance().getIntValue(ConfigurationKey::SERVICE_COMMUNICATION_MAX_MESSAGES)))
{
}


// GEMREQ-start A_19450-01#deserialize
void CommunicationPostHandler::handleRequest (PcSessionContext& session)
{
    const Header& header = session.request.header();

    TVLOG(1) << name() << ": processing request to " << header.target();

    // Deserialize the requested communication object. The format (json or xml) is defined
    // by the content type field in the header.
    // Expecting JSON format for insurants and representatives and XML from anybody else.

    try
    {
        auto communication = parseAndValidateRequestBody<model::Communication>(session);
        // GEMREQ-end A_19450-01#deserialize
        auto messageType = communication.messageType();
        PrescriptionId prescriptionId = communication.prescriptionId();


        const Identity recipient{communication.recipient()};

        A_19448_01.start("set sender and send timestamp");
        const std::optional<std::string> senderClaim = session.request.getAccessToken().stringForClaim(JWT::idNumberClaim);
        ErpExpect(senderClaim.has_value(), HttpStatus::BadRequest, "JWT does not contain a claim for the sender");
        communication.setTimeSent();

        const std::optional<std::string> senderProfessionOid =
            session.request.getAccessToken().stringForClaim(JWT::professionOIDClaim);
        ErpExpect(senderProfessionOid.has_value(), HttpStatus::BadRequest,
            "JWT does not contain a claim for the senders profession Oid");
        const Identity sender{validateSender(messageType, senderProfessionOid.value(), senderClaim.value())};
        communication.setSender(sender);
        A_19448_01.finish();

        // ERP-12846: ensure current keys are loaded before starting DB-Transaction
        auto utcToday = date::floor<date::days>(session.sessionTime().toChronoTimePoint());
        session.serviceContext.getTelematicPseudonymManager().ensureKeysUptodateForDay(utcToday);

        auto* databaseHandle = session.database();

        checkForChargeItemReference(prescriptionId, messageType, databaseHandle);

        std::optional<Task> task{};
        std::optional<model::Kvnr> taskKvnr{};
        if (messageType == Communication::MessageType::DispReq ||
            messageType == Communication::MessageType::InfoReq ||
            messageType == Communication::MessageType::Reply ||
            messageType == Communication::MessageType::Representative)
        {
            A_21371_02.start("check existence of task");
            auto [taskAndKey, healthCareProviderPrescription] = databaseHandle->retrieveTaskAndPrescription(prescriptionId);
            ErpExpect(taskAndKey.has_value(), HttpStatus::BadRequest, "Task for prescription id " + prescriptionId.toString() + " is missing");
            A_21371_02.finish();

            task = std::move(taskAndKey->task);
            taskKvnr = task->kvnr();
            if (messageType == Communication::MessageType::DispReq)
            {
                checkTaskReadyNotExpiredAndMvoAlreadyStarted(*task, healthCareProviderPrescription);
            }
            ErpExpect(taskKvnr.has_value(), HttpStatus::BadRequest, "Referenced task does not contain a KVNR");
        }

        session.accessLog.prescriptionId(prescriptionId);

        // Check for valid format of KNVR of recipient or Telematic ID
        validateRecipient(messageType, recipient);

        A_20229_01.start("limit KVNR -> KVNR messages to 10 per task");
        if (messageType == Communication::MessageType::Representative)
        {
            const auto count = databaseHandle->countRepresentativeCommunications(
                std::get<model::Kvnr>(sender), std::get<model::Kvnr>(recipient), prescriptionId);
            ErpExpect(count < mMaxMessageCount, HttpStatus::TooManyRequests,
                      "there are already " + std::to_string(mMaxMessageCount) + " KVNR to KVNR messages");
        }
        A_20229_01.finish();

        if (messageType == Communication::MessageType::DispReq ||
            messageType == Communication::MessageType::InfoReq ||
            messageType == Communication::MessageType::Representative)
        {
            A_20885_03.start("Examination of insured person and eligibility");
            if (senderProfessionOid == profession_oid::oid_versicherter)
            {
                std::optional<std::string> headerAccessCode = header.header(Header::XAccessCode);
                checkEligibilityOfInsurant(messageType, std::get<model::Kvnr>(sender), taskKvnr.value(),
                                           headerAccessCode, std::string(task->accessCode()), communication.accessCode());
            }
            A_20885_03.finish();
        }

        A_20230_01.start("restrict transfer for messages following the represantive schema to prescriptions that are ready or in-progress");
        if (messageType == Communication::MessageType::Representative)
        {
            // If there is a prescription id in the communication body there must also be a task for this (see check above).
            const Task::Status taskStatus = task->status();
            ErpExpect(taskStatus == Task::Status::ready || taskStatus == Task::Status::inprogress,
                HttpStatus::BadRequest,
                "Messages between KVNR's are restricted to prescriptions that are ready or in-progress");
        }
        A_20230_01.finish();

        A_20231.start("prevent messages to self");
        ErpExpect(recipient != sender, HttpStatus::BadRequest, "messages to self are not permitted");
        A_20231.finish();

        A_20752.start("Exclusion of representative communication from or of verification identity");
        // The service MUST refuse any access to e-prescriptions using an AccessCode (representative access)
        // if the access results in a combination of the KVNR of the eGK test card and the KVNR of the insured.
        // As only for representative messages both sender and receiver may be KVNRs this check will be limited
        // to representative messages.
        if (messageType == Communication::MessageType::Representative)
        {
            ErpExpect(std::get<model::Kvnr>(sender).verificationIdentity() ==
                          std::get<model::Kvnr>(recipient).verificationIdentity(),
                      HttpStatus::BadRequest,
                      "KVNR verification identities may not access information from insurants and vice versa");
        }
        A_20752.finish();

        if (messageType == Communication::MessageType::DispReq ||
            messageType == Communication::MessageType::InfoReq ||
            messageType == Communication::MessageType::Reply ||
            messageType == Communication::MessageType::Representative)
        {
            A_20753.start("Exclusion of representative access to or using verification identity");
            // The service MUST refuse any access to e-prescriptions using an AccessCode (representative access)
            // if the access results in a combination of KVNR of the eGK test card and KVNR of the insured.
            // Insured persons are not allowed to access test prescriptions.
            // With the eGK test card it is not allowed to access prescriptions of insured person's.
            checkVerificationIdentitiesOfKvnrs(messageType, taskKvnr.value(), sender, recipient);
            A_20753.finish();
        }

        // GEMREQ-start A_19450-01#callVerifyPayload
        A_19450_01.start("do not allow malicious code in payload");
        A_23878.start("verify json payload of DispReq");
        A_23879.start("verify json payload of Reply");
        communication.verifyPayload(session.serviceContext.getJsonValidator());
        A_23879.finish();
        A_23878.finish();
        A_19450_01.finish();
        // GEMREQ-end A_19450-01#callVerifyPayload

        communication.deleteTimeReceived();

        // Please note that "insertCommunication" adds the id to the communication.
        const std::optional<Uuid> id = databaseHandle->insertCommunication(communication);
        if (id.has_value())
        {
            session.response.setHeader(Header::Location, std::string(structure_definition::communicationLocation) + id->toString());
        }

        makeResponse(session, HttpStatus::Created, &communication);

        A_22367_02.start("Create initial subscription only for specific message types.");
        if (messageType == Communication::MessageType::InfoReq ||
            messageType == Communication::MessageType::ChargChangeReq ||
            messageType == Communication::MessageType::DispReq)
        {
            SubscriptionPostHandler::publish(session, std::get<model::TelematikId>(recipient));
        }
        A_22367_02.finish();
    // GEMREQ-start A_19450-01#catchModelException
    }
    catch (const ModelException& e)
    {
        TVLOG(1) << "ModelException: " << e.what();
        ErpFailWithDiagnostics(HttpStatus::BadRequest, "Invalid request body", e.what());
    }
    // GEMREQ-end A_19450-01#catchModelException
}

void CommunicationPostHandler::checkForChargeItemReference(const PrescriptionId& prescriptionId,
                                                           const Communication::MessageType& messageType,
                                                           const Database* databaseHandle) const
{
    if (messageType == Communication::MessageType::ChargChangeReq ||
        messageType == Communication::MessageType::ChargChangeReply)
    {
        ErpExpect(prescriptionId.isPkv(), HttpStatus::BadRequest, "Reference charge item ID is not of type PKV");
        A_22734.start("check existence of charge item");
        try
        {
            static_cast<void>(databaseHandle->retrieveChargeInformation(prescriptionId));
        }
        catch (const ErpException& ex)
        {
            if (ex.status() == HttpStatus::NotFound)
            {
                ErpFail(HttpStatus::BadRequest, "referenced charge item does not exist");
            }

            throw;
        }
        A_22734.finish();
    }
}

model::Identity CommunicationPostHandler::validateSender(
    model::Communication::MessageType messageType,
    const std::string& professionOid,
    const std::string& sender) const
{
    if (messageType == Communication::MessageType::Reply ||
        messageType == Communication::MessageType::ChargChangeReply)
    {
        ErpExpect(profession_oid::toInnerRequestRole(professionOid) == profession_oid::inner_request_role_pharmacy,
            HttpStatus::BadRequest, "Invalid sender profession oid in communication reply message");
        ErpExpect(model::TelematikId::isTelematikId(sender), HttpStatus::BadRequest,
            "A valid Telematic ID must contain at least one \"-\"");
        return model::TelematikId{sender};
    }
    else
    {
        ErpExpect(profession_oid::toInnerRequestRole(professionOid) == profession_oid::inner_request_role_patient,
            HttpStatus::BadRequest, "Invalid sender profession oid in communication message");
        ErpExpect(Kvnr::isKvnr(sender), HttpStatus::BadRequest, "Invalid Kvnr");
        auto kvnrType = messageType == Communication::MessageType::ChargChangeReq ? model::Kvnr::Type::pkv
                                                                                  : model::Kvnr::Type::unspecified;
        return model::Kvnr{sender, kvnrType};
    }
}

void CommunicationPostHandler::validateRecipient(
    model::Communication::MessageType messageType,
    const model::Identity& recipient) const
{
    if (messageType == Communication::MessageType::InfoReq ||
        messageType == Communication::MessageType::ChargChangeReq ||
        messageType == Communication::MessageType::DispReq)
    {
        ErpExpect(std::holds_alternative<model::TelematikId>(recipient), HttpStatus::BadRequest,
                  "Expected TelematikId, got KVNR");
        ErpExpect(std::get<model::TelematikId>(recipient).validFormat(), HttpStatus::BadRequest,
            "A valid Telematic ID must contain at least one \"-\"");
    }
    else
    {
        ErpExpect(std::holds_alternative<model::Kvnr>(recipient), HttpStatus::BadRequest,
                  "Expected KVNR, got TelematikId");
        // Please note that the format of KVNRs is already checked when validating against the schema
        // but Telematic IDs are not validated against the schema.
        // Because of this discrepancy the code below has been kept for the sake of clarification.
        ErpExpect(std::get<model::Kvnr>(recipient).validFormat(), HttpStatus::BadRequest, "Invalid Kvnr");
    }
}

void CommunicationPostHandler::checkEligibilityOfInsurant(
    model::Communication::MessageType messageType,
    const model::Kvnr& senderKvnr,
    const model::Kvnr& taskKvnr,
    const std::optional<std::string>& headerAccessCode,
    const std::string& taskAccessCode,
    const std::optional<std::string>& communicationAccessCode) const
{
    switch (messageType)
    {
    case Communication::MessageType::InfoReq:
        // The AccessCode for the authorization of the representative cannot be checked, as it must not be sent
        // with the availability request - otherwise the e - prescription is considered assigned.
        ErpExpect(!communicationAccessCode.has_value(), HttpStatus::BadRequest,
                "Info request must not contain an access code in payload");
        break;
    case Communication::MessageType::ChargChangeReq:
        ErpExpect(!communicationAccessCode.has_value(), HttpStatus::BadRequest,
                  "Charge item change request must not contain an access code in payload");
        break;
    case Communication::MessageType::Reply:
        // As the AccessCode is not contained in the info request it cannot also not be sent with the reply.
        ErpExpect(!communicationAccessCode.has_value(), HttpStatus::BadRequest,
                "Reply must not contain an access code in payload");
        break;
    case Communication::MessageType::ChargChangeReply:
        ErpExpect(!communicationAccessCode.has_value(), HttpStatus::BadRequest,
                  "Charge item change reply must not contain an access code in payload");
        break;
    case Communication::MessageType::DispReq:
        ErpExpect(communicationAccessCode.has_value(), HttpStatus::BadRequest,
                "Dispense request must contain an access code in payload");
        break;
    case Communication::MessageType::Representative:
        // If sent by insurant ..
        if (senderKvnr == taskKvnr)
            ErpExpect(communicationAccessCode.has_value(), HttpStatus::BadRequest,
                    "Representative message sent by insurant must contain an access code in payload");
        break;
    }

    // If sent by representative ..
    if (senderKvnr != taskKvnr)
    {
        // .. the header must contain an access code.
        ErpExpect(headerAccessCode.has_value(), HttpStatus::BadRequest, "Header must contain an access code");

        // If the headers access code is not equal to the access code of the referenced task
        // the setting of the message must be canceled in order to prevent misleading test messages.
        ErpExpect(headerAccessCode.value() == taskAccessCode, HttpStatus::BadRequest,
                "Access code of header not equal to access code of referenced task");
    }
}

void CommunicationPostHandler::checkVerificationIdentitiesOfKvnrs(
    Communication::MessageType messageType,
    const model::Kvnr& taskKvnr,
    const model::Identity& sender,
    const model::Identity& recipient) const
{
    if (messageType == Communication::MessageType::InfoReq || messageType == Communication::MessageType::DispReq)
    {
        ErpExpect(std::get<model::Kvnr>(sender).verificationIdentity() == taskKvnr.verificationIdentity(),
                  HttpStatus::BadRequest,
                  "KVNR verification identities may not access information from insurants and vice versa");
    }
    else if (messageType == Communication::MessageType::Reply)
    {
        ErpExpect(std::get<model::Kvnr>(recipient).verificationIdentity() == taskKvnr.verificationIdentity(),
                  HttpStatus::BadRequest,
                  "KVNR verification identities may not access information from insurants and vice versa");
    }
    else if (messageType == Communication::MessageType::Representative)
    {
        ErpExpect(std::get<model::Kvnr>(sender).verificationIdentity() == taskKvnr.verificationIdentity(),
                  HttpStatus::BadRequest,
                  "KVNR verification identities may not access information from insurants and vice versa");
        ErpExpect(std::get<model::Kvnr>(recipient).verificationIdentity() == taskKvnr.verificationIdentity(),
                  HttpStatus::BadRequest,
                  "KVNR verification identities may not access information from insurants and vice versa");
    }
}
