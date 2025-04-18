/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/service/CommunicationGetHandler.hxx"

#include "shared/ErpRequirements.hxx"
#include "erp/database/Database.hxx"
#include "erp/model/Identity.hxx"
#include "shared/util/TLog.hxx"
#include "erp/util/search/UrlArguments.hxx"


CommunicationGetHandlerBase::CommunicationGetHandlerBase(Operation operation,
    const std::initializer_list<std::string_view>& allowedProfessionOiDs)
    : ErpRequestHandler(operation, allowedProfessionOiDs)
{
}

void CommunicationGetHandlerBase::markCommunicationsAsRetrieved (
    Database& database,
    std::vector<model::Communication>& communications,
    const std::string& caller)
{
    A_19521.start("mark communication objects as retrieved");

    // Find the Communication objects which have not yet been retrieved and mark them with the current time
    // as retrieved.
    const auto now = model::Timestamp::now();
    std::vector<Uuid> communicationIdsToMark;
    for (auto& communication : communications)
    {
        const std::string recipientIdentiy = model::getIdentityString(communication.recipient());
        if (!communication.timeReceived().has_value() && recipientIdentiy == caller)
        {
            Expect(communication.id().has_value(), "communication id has not been initialized");
            communicationIdsToMark.emplace_back(communication.id().value());

            // This storing of the received date may need some polishing as it is disconnected from the
            // actual update in the database.
            communication.setTimeReceived(now);
        }
    }

    database.markCommunicationsAsRetrieved(
        communicationIdsToMark,
        now,
        caller);

    A_19521.finish();
}


model::Bundle CommunicationGetHandlerBase::createBundle (const std::vector<model::Communication>& communications) const
{
    const std::string linkBase = getLinkBase() + "/Communication";
    model::Bundle bundle(model::BundleType::searchset, ::model::FhirResourceBase::NoProfile);
    for (const auto& communication : communications)
    {
        Expect(communication.id().has_value(), "communication id has not been initialized");
        bundle.addResource(
            linkBase + "/" + communication.id()->toString(),
            {},
            model::Bundle::SearchMode::match,
            communication.jsonDocument());
    }
    return bundle;
}


// GEMREQ-start A_19520-01#getValidatedCaller
std::string CommunicationGetHandlerBase::getValidatedCaller (PcSessionContext& session) const
{
    A_19520_01.start("extract caller from the access token");
    const auto callerClaim = session.request.getAccessToken().stringForClaim(JWT::idNumberClaim);
    Expect(callerClaim.has_value(), "JWT does not contain a claim for the caller");
    A_19520_01.finish();
    return callerClaim.value();
}
// GEMREQ-end A_19520-01#getValidatedCaller


CommunicationGetAllHandler::CommunicationGetAllHandler (const std::initializer_list<std::string_view>& allowedProfessionOiDs)
    : CommunicationGetHandlerBase(Operation::GET_Communication, allowedProfessionOiDs)
{
}


void CommunicationGetAllHandler::handleRequest (PcSessionContext& session)
{
    TVLOG(1) << name() << ": processing request to " << session.request.header().target();

    // Set up a DB query with filters that are based on path and query arguments of the request
    auto* database = session.database();

    A_19522.start("support sent, received, recipient and sender in searching and sorting");
    A_24438.start("Set sent as default sort argument.");
    auto arguments = std::optional<UrlArguments>(
        std::in_place,
        std::vector<SearchParameter>
        {
            {"sent", "id", SearchParameter::Type::DateAsUuid},
            {"received",  SearchParameter::Type::Date},
            {"sender",    SearchParameter::Type::HashedIdentity},
            {"recipient", SearchParameter::Type::HashedIdentity},
            {"identifier", "id", SearchParameter::Type::DateAsUuid},
        }, "sent");
    A_24438.finish();
    arguments->parse(session.request, session.serviceContext.getKeyDerivation());
    A_19522.finish();

    const auto caller = getValidatedCaller(session);

    // GEMREQ-start A_19520-01#allPassCaller
    A_19520_01.start("pass caller to database filter");
    auto communications = database->retrieveCommunications(
        caller, // Used to filter by sender or recipient according to A_19520
        {},     // No filter by communication id for getAll. Also, per ERP-3862, we don't have to support the _id parameter.
        arguments);
    A_19520_01.finish();
    // GEMREQ-end A_19520-01#allPassCaller
    try
    {
        markCommunicationsAsRetrieved(*database, communications, caller);

        std::size_t totalSearchMatches =
            responseIsPartOfMultiplePages(arguments->pagingArgument(), communications.size())
                ? database->countCommunications(caller, arguments)
                : communications.size();

        auto bundle = createBundle(communications);
        bundle.setTotalSearchMatches(totalSearchMatches);
        const auto links = arguments->createBundleLinks(getLinkBase(), "/Communication", totalSearchMatches);
        for (const auto& link : links)
            bundle.setLink(link.first, link.second);

        makeResponse(session, HttpStatus::OK, &bundle);
    }
    catch (const model::ModelException& e)
    {
        TVLOG(1) << "ModelException: " << e.what();
        ErpFailWithDiagnostics(HttpStatus::BadRequest, "caught ModelException", e.what());
    }
}


CommunicationGetByIdHandler::CommunicationGetByIdHandler (const std::initializer_list<std::string_view>& allowedProfessionOiDs)
    : CommunicationGetHandlerBase(Operation::GET_Communication_id, allowedProfessionOiDs)
{
}


void CommunicationGetByIdHandler::handleRequest (PcSessionContext& session)
{
    TVLOG(1) << name() << ": processing request to " << session.request.header().target();

    // Set up a DB query with filters that are based on path and query parameters of the request
    auto* database = session.database();
    const auto caller = getValidatedCaller(session);
    const auto communicationId = getValidatedCommunicationId(session);

    // GEMREQ-start A_19520-01#byIdPassCaller
    A_19520_01.start("pass caller to database filter");
    auto communications = database->retrieveCommunications(
        caller,                 // Used to filter by sender or recipient according to A_19520
        communicationId,        // Filter by communication id, according to A_19520
        UrlArguments({}));      // No additional search parameters for getById
    A_19520_01.finish();
    // GEMREQ-end A_19520-01#byIdPassCaller

    ErpExpect(!communications.empty(), HttpStatus::NotFound, "no Communication found for id");
    ErpExpect(communications.size()==1, HttpStatus::InternalServerError, "more than one Communication found for unique id");

    markCommunicationsAsRetrieved(*database, communications, caller);

    makeResponse(session, HttpStatus::OK, &communications.front());
}


Uuid CommunicationGetByIdHandler::getValidatedCommunicationId (PcSessionContext& session) const
{
    const auto communicationIdString = session.request.getPathParameter("id");
    ErpExpect(communicationIdString.has_value(), HttpStatus::BadRequest, "no or invalid id parameter");
    Uuid uuid(communicationIdString.value());
    ErpExpect(uuid.isValidIheUuid(), HttpStatus::BadRequest, "Invalid format of communication id");
    return uuid;
}
