/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/service/AuditEventHandler.hxx"

#include "erp/ErpRequirements.hxx"
#include "erp/model/Bundle.hxx"
#include "erp/service/AuditEventCreator.hxx"
#include "erp/service/AuditEventTextTemplates.hxx"
#include "erp/server/context/SessionContext.hxx"
#include "erp/util/search/UrlArguments.hxx"
#include "erp/util/TLog.hxx"


namespace
{

model::Kvnr getKvnrFromAccessToken(const JWT& accessToken)
{
    A_19396.start("Read Kvnr from access token");
    const auto kvnrClaim = accessToken.stringForClaim(JWT::idNumberClaim);
    Expect(kvnrClaim.has_value(), "JWT does not contain Kvnr");
    A_19396.finish();
    return model::Kvnr{kvnrClaim.value()};
}

} // anonymous namespace

GetAllAuditEventsHandler::GetAllAuditEventsHandler (const std::initializer_list<std::string_view>& allowedProfessionOiDs)
    : ErpRequestHandler(Operation::GET_AuditEvent, allowedProfessionOiDs)
{
}

model::Bundle GetAllAuditEventsHandler::createBundle (
    const PcServiceContext& serviceContext,
    const ServerRequest& request,
    const std::vector<model::AuditData>& auditData) const
{
    const std::string linkBase = getLinkBase() + "/AuditEvent";
    const auto language = getLanguageFromHeader(request.header()).value_or(std::string(AuditEventTextTemplates::defaultLanguage));
    model::Bundle bundle(model::BundleType::searchset, ::model::ResourceBase::NoProfile);
    for (const auto& data : auditData)
    {
        if(data.isValidEventId())
        {
            const model::AuditEvent auditEvent = AuditEventCreator::fromAuditData(
                data, language, serviceContext.auditEventTextTemplates(), request.getAccessToken(),
                model::ResourceVersion::current<model::ResourceVersion::DeGematikErezeptWorkflowR4>());
            bundle.addResource(
                linkBase + "/" + std::string(auditEvent.id()),
                {},
                model::Bundle::SearchMode::match,
                auditEvent.jsonDocument());
        }
        else
        {
            TLOG(WARNING) << "Unknown audit event id " << magic_enum::enum_integer(data.eventId())
                          << " in record read from database, ignoring this audit event.";
        }
    }
    return bundle;
}

void GetAllAuditEventsHandler::handleRequest (PcSessionContext& session)
{
    TVLOG(1) << "GetAllAuditEventHandler: processing request to " << session.request.header().target();

    // Set up a DB query with filters that are based on path and query parameters of the request
    auto* database = session.database();
    A_19399.start("Support parameters date and subType for audit event search"); // Parameter "agent" not supported, see ERP-4978
    SearchParameter::SearchToDbValue searchToDbValue = [](const std::string_view& val)
        {
            try
            {
                return model::AuditEvent::SubTypeNameToActionName.at(val);
            }
            catch(...)
            {
                return val;
            }
        };
    auto arguments = std::optional<UrlArguments>(
        std::in_place,
        std::vector<SearchParameter>
        {
            { "date", "id", SearchParameter::Type::DateAsUuid },
            { "subtype", "action", SearchParameter::Type::String, std::move(searchToDbValue) }
        });
    arguments->parse(session.request, session.serviceContext.getKeyDerivation());
    A_19399.finish();

    const auto kvnr = getKvnrFromAccessToken(session.request.getAccessToken());

    A_19396.start("Filter audit events using Kvnr from access token");
    auto auditEvents = database->retrieveAuditEventData(
        kvnr,
        { }, // id
        { }, // prescriptionId
        arguments);
    A_19396.finish();

    std::size_t totalSearchMatches =
        responseIsPartOfMultiplePages(arguments->pagingArgument(), auditEvents.size()) ?
        database->countAuditEventData(kvnr, arguments) : auditEvents.size();

    A_19397.start("Return audit events as bundle");
    auto bundle = createBundle(session.serviceContext, session.request, auditEvents);
    bundle.setTotalSearchMatches(totalSearchMatches);
    const auto links = arguments->getBundleLinks(getLinkBase(), "/AuditEvent", totalSearchMatches);
    for (const auto& link : links)
    {
        bundle.setLink(link.first, link.second);
    }
    A_19397.finish();

    makeResponse(session, HttpStatus::OK, &bundle);
}

//--------------------------------------------------------------------------------------------------------------------

GetAuditEventHandler::GetAuditEventHandler (const std::initializer_list<std::string_view>& allowedProfessionOiDs)
        : ErpRequestHandler(Operation::GET_AuditEvent_id, allowedProfessionOiDs)
{
}


void GetAuditEventHandler::handleRequest (PcSessionContext& session)
{
    TVLOG(1) << "GetAuditEventHandler: processing request to " << session.request.header().target();

    const auto idString = session.request.getPathParameter("id");
    ErpExpect(idString.has_value(), HttpStatus::BadRequest, "No or invalid id parameter");
    Uuid uuid(idString.value());
    ErpExpect(uuid.isValidIheUuid(), HttpStatus::BadRequest, "Invalid format of AuditEvent id");

    // Set up a DB query with filters that are based on path and query parameters of the request
    auto* database = session.database();

    A_19396.start("Filter audit events using Kvnr from access token");
    auto auditEvents = database->retrieveAuditEventData(
        getKvnrFromAccessToken(session.request.getAccessToken()),
        uuid,
        {},
        UrlArguments({}));      // No additional search parameters for getById
    A_19396.finish();

    ErpExpect(!auditEvents.empty(), HttpStatus::NotFound, "No AuditEvent found for id");
    ErpExpect(auditEvents.size() == 1, HttpStatus::InternalServerError, "More than one AuditEvent found for unique id");

    const auto& auditData = auditEvents.front();
    if(!auditData.isValidEventId())
    {
        TLOG(WARNING) << "Unknown audit event id " << magic_enum::enum_integer(auditData.eventId())
                      << " in record read from database.";
        ErpFail(HttpStatus::InternalServerError, "Found AuditEvent contains invalid event id");
    }

    model::AuditEvent auditEvent = AuditEventCreator::fromAuditData(
        auditData, getLanguageFromHeader(session.request.header()).value_or(std::string(AuditEventTextTemplates::defaultLanguage)),
        session.serviceContext.auditEventTextTemplates(), session.request.getAccessToken(),
        model::ResourceVersion::current<model::ResourceVersion::DeGematikErezeptWorkflowR4>());

    makeResponse(session, HttpStatus::OK, &auditEvent);
}
