/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "CommunicationPostHandler.hxx"

#include "erp/database/Database.hxx"
#include "erp/model/CommunicationPayload.hxx"
#include "erp/model/ResourceNames.hxx"
#include "erp/model/Task.hxx"
#include "erp/server/context/ServiceContext.hxx"
#include "erp/server/request/ServerRequest.hxx"
#include "erp/server/response/ServerResponse.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/TLog.hxx"
#include "erp/ErpRequirements.hxx"

#include "erp/common/MimeType.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/util/FileHelper.hxx"

using namespace model;
using namespace model::resource;

CommunicationPostHandler::CommunicationPostHandler(const std::initializer_list<std::string_view>& allowedProfessionOiDs)
    : ErpRequestHandler(Operation::POST_Communication, allowedProfessionOiDs),
      mMaxMessageCount(Configuration::instance().getIntValue(ConfigurationKey::SERVICE_COMMUNICATION_MAX_MESSAGES))
{
}

void CommunicationPostHandler::handleRequest (PcSessionContext& session)
{
    const Header& header = session.request.header();

    TVLOG(1) << name() << ": processing request to " << header.target();

    // Deserialize the requested communication object. The format (json or xml) is defined
    // by the content type field in the header.
    // Expecting JSON format for insurants and representatives and XML from anybody else.

    try
    {
        // as a first step validate using the FHIR schema, which is good enough for the XML->JSON converter
        auto communication = parseAndValidateRequestBody<Communication>(session, SchemaType::fhir);
        Communication::MessageType messageType = communication.messageType();

        std::string recipient = std::string(communication.recipient().value());

        A_19448.start("set sender and send timestamp");
        const std::optional<std::string> sender = session.request.getAccessToken().stringForClaim(JWT::idNumberClaim);
        ErpExpect(sender.has_value(), HttpStatus::BadRequest, "JWT does not contain a claim for the sender");
        communication.setSender(sender.value());
        communication.setTimeSent();
        A_19448.finish();

        const std::optional<std::string> senderProfessionOid =
            session.request.getAccessToken().stringForClaim(JWT::professionOIDClaim);
        ErpExpect(senderProfessionOid.has_value(), HttpStatus::BadRequest,
            "JWT does not contain a claim for the senders profession Oid");
        validateSender(messageType, senderProfessionOid.value(), sender.value());

        // secondly validate against the communication profile, which can only be determined
        // by looking into the resource, which must therefore already be parsed
        A_19447.start("validate against the fhir profile schema");
        validateAgainstFhirProfile(messageType, communication, session.serviceContext.getJsonValidator());
        A_19447.finish();

        auto* databaseHandle = session.database();

        PrescriptionId prescriptionId = communication.prescriptionId();
        std::optional<Task> task = databaseHandle->retrieveTaskForUpdate(prescriptionId);
        ErpExpect(task.has_value(), HttpStatus::BadRequest, "Task for prescription id " + prescriptionId.toString() + " is missing");
        std::optional<std::string_view> taskKvnr = task->kvnr();
        ErpExpect(taskKvnr.has_value(), HttpStatus::BadRequest, "Referenced task does not contain a KVNR");

        session.accessLog.prescriptionId(prescriptionId.toString());

        // Check for valid format of KNVR of recipient or Telematic ID
        validateRecipient(messageType, recipient);

        A_20229.start("limit KVNR -> KVNR messages to 10 per task");
        if (messageType == Communication::MessageType::Representative)
        {
            const auto count = databaseHandle->countRepresentativeCommunications(sender.value(), recipient, prescriptionId);
            ErpExpect(count < mMaxMessageCount, HttpStatus::TooManyRequests, "there are already " + std::to_string(mMaxMessageCount) + " KVNR to KVNR messages");
        }
        A_20229.finish();

        A_20885_01.start("Examination of insured person and eligibility");
        if (senderProfessionOid == profession_oid::oid_versicherter)
        {
            std::optional<std::string> headerAccessCode = header.header(Header::XAccessCode);
            checkEligibilityOfInsurant(messageType, sender.value(), taskKvnr.value(),
                             headerAccessCode, std::string(task->accessCode()), communication.accessCode());
        }
        A_20885_01.finish();

        A_20230.start("restrict transfer for messages following the represantive schema to prescriptions that are ready or in-progress");
        if (messageType == Communication::MessageType::Representative)
        {
            // If there is a prescription id in the communication body there must also be a task for this (see check above).
            const Task::Status taskStatus = task->status();
            ErpExpect(taskStatus == Task::Status::ready || taskStatus == Task::Status::inprogress,
                HttpStatus::BadRequest,
                "Messages between KVNR's are restricted to prescriptions that are ready or in-progress");
        }
        A_20230.finish();

        A_20231.start("prevent messages to self");
        ErpExpect(recipient != sender.value(), HttpStatus::BadRequest, "messages to self are not permitted");
        A_20231.finish();

        A_20752.start("Exclusion of representative communication from or of verification identity");
        // The service MUST refuse any access to e-prescriptions using an AccessCode (representative access)
        // if the access results in a combination of the KVNR of the eGK test card and the KVNR of the insured.
        // As only for representative messages both sender and receiver may be KVNRs this check will be limited
        // to representative messages.
        if (messageType == Communication::MessageType::Representative)
        {
            ErpExpect(isVerificationIdentityKvnr(sender.value()) == isVerificationIdentityKvnr(recipient),
                HttpStatus::BadRequest,
                "KVNR verification identities may not access information from insurants and vice versa");
        }
        A_20752.finish();

        A_20753.start("Exclusion of representative access to or using verification identity");
        // The service MUST refuse any access to e-prescriptions using an AccessCode (representative access)
        // if the access results in a combination of KVNR of the eGK test card and KVNR of the insured.
        // Insured persons are not allowed to access test prescriptions.
        // With the eGK test card it is not allowed to access prescriptions of insured person's.
        checkVerificationIdentitiesOfKvnrs(messageType, taskKvnr.value(), sender, recipient);
        A_20753.finish();

        A_19450.start("do not allow malicious code in payload");
        communication.verifyPayload();
        A_19450.finish();

        // Please note that "insertCommunication" adds the id to the communication.
        const std::optional<Uuid> id = databaseHandle->insertCommunication(communication);
        if (id.has_value())
        {
            session.response.setHeader(Header::Location, std::string(structure_definition::communicationLocation) + id->toString());
        }

        makeResponse(session, HttpStatus::Created, &communication);
    }
    catch (const ModelException& e)
    {
        TVLOG(1) << "ModelException: " << e.what();
        ErpFail(HttpStatus::BadRequest, "caught ModelException");
    }
}

void CommunicationPostHandler::validateAgainstFhirProfile(
    model::Communication::MessageType messageType,
    const model::Communication& communication,
    const JsonValidator& jsonValidator) const
{
    jsonValidator.validate(
        NumberAsStringParserDocumentConverter::copyToOriginalFormat(communication.jsonDocument()),
        Communication::messageTypeToSchemaType(messageType));
}

void CommunicationPostHandler::validateSender(
    model::Communication::MessageType messageType,
    const std::string& professionOid,
    const std::string& sender) const
{
    if (messageType == Communication::MessageType::Reply)
    {
        ErpExpect(profession_oid::toInnerRequestRole(professionOid) == profession_oid::inner_request_role_pharmacy,
            HttpStatus::BadRequest, "Invalid sender profession oid in communication reply message");
        ErpExpect(sender.find_first_of('-') != std::string_view::npos, HttpStatus::BadRequest,
            "A valid Telematic ID must contain at least one \"-\"");
    }
    else
    {
        ErpExpect(profession_oid::toInnerRequestRole(professionOid) == profession_oid::inner_request_role_patient,
            HttpStatus::BadRequest, "Invalid sender profession oid in communication message");
        ErpExpect(sender.find('\0') == std::string::npos, HttpStatus::BadRequest,
            "Null character in KVNR of sender");
        ErpExpect(sender.size() == 10, HttpStatus::BadRequest,
            "KVNR of sender must have 10 characters");
    }
}

void CommunicationPostHandler::validateRecipient(
    model::Communication::MessageType messageType,
    const std::string& recipient) const
{
    if (messageType == Communication::MessageType::InfoReq || messageType == Communication::MessageType::DispReq)
    {
        ErpExpect(recipient.find_first_of('-') != std::string_view::npos, HttpStatus::BadRequest,
            "A valid Telematic ID must contain at least one \"-\"");
    }
    else
    {
        // Please note that the format of KVNRs is already checked when validating against the schema
        // but Telematic IDs are not validated against the schema.
        // Because of this discrepancy the code below has been kept for the sake of clarification.
        ErpExpect(recipient.find('\0') == std::string::npos, HttpStatus::BadRequest,
            "Null character in KVNR of recipient");
        ErpExpect(recipient.size() == 10, HttpStatus::BadRequest,
            "KVNR of recipient must have 10 characters");
    }
}

void CommunicationPostHandler::checkEligibilityOfInsurant(
    model::Communication::MessageType messageType,
    const std::string_view& senderKvnr,
    const std::string_view& taskKvnr,
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
    case Communication::MessageType::Reply:
        // As the AccessCode is not contained in the info request it cannot also not be sent with the reply.
        ErpExpect(!communicationAccessCode.has_value(), HttpStatus::BadRequest,
                "Reply must not contain an access code in payload");
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
                "Access code of header not equal to cccess code of referenced task");
    }
}

void CommunicationPostHandler::checkVerificationIdentitiesOfKvnrs(
    Communication::MessageType messageType,
    const std::string_view& taskKvnr,
    const std::optional<std::string_view>& sender,
    const std::string_view& recipient) const
{
    if (messageType == Communication::MessageType::InfoReq || messageType == Communication::MessageType::DispReq)
    {
        ErpExpect(isVerificationIdentityKvnr(sender.value()) == isVerificationIdentityKvnr(taskKvnr),
            HttpStatus::BadRequest,
            "KVNR verification identities may not access information from insurants and vice versa");
    }
    else if (messageType == Communication::MessageType::Reply)
    {
        ErpExpect(isVerificationIdentityKvnr(recipient) == isVerificationIdentityKvnr(taskKvnr),
            HttpStatus::BadRequest,
            "KVNR verification identities may not access information from insurants and vice versa");
    }
    else if (messageType == Communication::MessageType::Representative)
    {
        ErpExpect(isVerificationIdentityKvnr(sender.value()) == isVerificationIdentityKvnr(taskKvnr),
            HttpStatus::BadRequest,
            "KVNR verification identities may not access information from insurants and vice versa");
        ErpExpect(isVerificationIdentityKvnr(recipient) == isVerificationIdentityKvnr(taskKvnr),
            HttpStatus::BadRequest,
            "KVNR verification identities may not access information from insurants and vice versa");
    }
}
