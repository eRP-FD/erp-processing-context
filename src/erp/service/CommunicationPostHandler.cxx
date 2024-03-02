/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "CommunicationPostHandler.hxx"
#include "erp/ErpRequirements.hxx"
#include "erp/common/MimeType.hxx"
#include "erp/model/ChargeItem.hxx"
#include "erp/model/CommunicationPayload.hxx"
#include "erp/model/KbvMedicationBase.hxx"
#include "erp/model/ResourceNames.hxx"
#include "erp/model/Task.hxx"
#include "erp/server/request/ServerRequest.hxx"
#include "erp/server/response/ServerResponse.hxx"
#include "erp/service/SubscriptionPostHandler.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/util/Demangle.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/FileHelper.hxx"
#include "erp/util/TLog.hxx"
#include "fhirtools/model/erp/ErpElement.hxx"
#include "fhirtools/parser/FhirPathParser.hxx"



using namespace model;
using namespace model::resource;

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
        // as a first step validate using the FHIR schema, which is good enough for the XML->JSON converter
        auto communication = parseAndValidateRequestBody<Communication>(session, SchemaType::fhir, false);
// GEMREQ-end A_19450-01#deserialize
        PrescriptionId prescriptionId = communication.prescriptionId();

        Communication::MessageType messageType = communication.messageType();

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

        // secondly validate against the communication profile, which can only be determined
        // by looking into the resource, which must therefore already be parsed
        A_19447_04.start("validate against the fhir profile schema");
        validateAgainstFhirProfile(messageType, communication, session.serviceContext.getXmlValidator(),
                                   session.serviceContext.getInCodeValidator());
        A_19447_04.finish();
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
            auto taskAndKey = databaseHandle->retrieveTaskForUpdate(prescriptionId);
            ErpExpect(taskAndKey.has_value(), HttpStatus::BadRequest, "Task for prescription id " + prescriptionId.toString() + " is missing");
            A_21371_02.finish();

            task = std::move(taskAndKey->task);
            taskKvnr = task->kvnr();
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

void CommunicationPostHandler::validateAgainstFhirProfile(
    model::Communication::MessageType messageType,
    const model::Communication& communication,
    const XmlValidator& xmlValidator,
    const InCodeValidator& inCodeValidator) const
{
    switch (messageType)
    {
        using enum model::Communication::MessageType;
        case InfoReq:
            validateInfoRequest(communication, xmlValidator, inCodeValidator);
            return;
        case Reply:
        case DispReq:
        case Representative:
        case ChargChangeReq:
        case ChargChangeReply:
            (void) Communication::fromXml(communication.serializeToXmlString(), xmlValidator, inCodeValidator,
                                          Communication::messageTypeToSchemaType(messageType),
                                          model::ResourceVersion::supportedBundles(),
                                          communication.canValidateGeneric());
            return;
    }
    Fail2("Invalid Communication::MessageType: " + std::to_string(static_cast<uintmax_t>(mMaxMessageCount)),
          std::logic_error);
}

void CommunicationPostHandler::validateInfoRequest(const model::Communication& communication,
                                                   const XmlValidator& xmlValidator,
                                                   const InCodeValidator& inCodeValidator) const
{
    using namespace std::string_literals;
    namespace rv = model::ResourceVersion;
    static rapidjson::Pointer idPtr{resource::ElementName::path(elements::id)};
    static rapidjson::Pointer resourceTypePtr{resource::ElementName::path(elements::resourceType)};
    const auto* repo = std::addressof(Fhir::instance().structureRepository());
    // ensure that all past bundles are included
    auto supportedBundles = rv::supportedBundles();
    if (supportedBundles.contains(rv::FhirProfileBundleVersion::v_2023_07_01))
    {
        supportedBundles.insert(rv::FhirProfileBundleVersion::v_2022_01_01);
    }
    const auto extractMedication = fhirtools::FhirPathParser::parse(repo, "about.resolve()");
    Expect3(extractMedication, "Failed to parse extractMedication expression.", std::logic_error);
    model::NumberAsStringParserDocument doc;
    doc.CopyFrom(communication.jsonDocument(), doc.GetAllocator());
    auto communicationElement =
        std::make_shared<ErpElement>(repo, std::weak_ptr<const ErpElement>{}, "Communication", std::addressof(doc));
    auto medications = extractMedication->eval({communicationElement});
    for (const auto& medicationElement : medications)
    {
        const auto& medicationErpElement = dynamic_cast<const ErpElement*>(medicationElement.get());
        Expect3(medicationErpElement != nullptr,
                "unexpected type for medicationElement: "s.append(util::demangle(typeid(medicationErpElement).name())),
                std::logic_error);
        //NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
        auto* medicationValue = const_cast<rapidjson::Value*>(medicationErpElement->erpValue());
        model::KbvMedicationGeneric::validateMedication(*medicationErpElement, xmlValidator, inCodeValidator,
                                                        supportedBundles, false);
        const std::string idValue = idPtr.Get(*medicationValue)->GetString();
        const std::string resourceTypeValue = resourceTypePtr.Get(*medicationValue)->GetString();
        medicationValue->SetObject();
        idPtr.Set(*medicationValue, idValue, doc.GetAllocator());
        resourceTypePtr.Set(*medicationValue, resourceTypeValue, doc.GetAllocator());
    }
    using CommunicationFactory = model::ResourceFactory<Communication>;
    CommunicationFactory::Options options;
    if (! communication.canValidateGeneric())
    {
        options.genericValidationMode = Configuration::GenericValidationMode::disable;
    };
    (void) model::ResourceFactory<Communication>::fromJson(std::move(doc), options)
        .getValidated(SchemaType::Gem_erxCommunicationInfoReq, xmlValidator, inCodeValidator);
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
