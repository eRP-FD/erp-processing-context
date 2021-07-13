#include "erp/service/ErpRequestHandler.hxx"

#include "erp/ErpRequirements.hxx"
#include "erp/pc/ProfessionOid.hxx"
#include "erp/server/response/ServerResponse.hxx"
#include "erp/util/TLog.hxx"
#include "erp/util/search/PagingArgument.hxx"

#include <utility>


ErpRequestHandler::ErpRequestHandler (const Operation operation, const std::initializer_list<std::string_view>& allowedProfessionOIDs)
    : mOperation(operation),
      mAllowedProfessionOIDs(allowedProfessionOIDs)
{
}


bool ErpRequestHandler::allowedForProfessionOID (std::string_view professionOid) const
{
    return mAllowedProfessionOIDs.count(professionOid) > 0;
}


Operation ErpRequestHandler::getOperation (void) const
{
    return mOperation;
}


std::string_view ErpRequestHandler::name () const
{
    return toString(mOperation);
}

bool ErpRequestHandler::callerWantsJson (const ServerRequest& request)
{
    A_19539.start("1. priority: _format URL parameter");
    const auto formatParameter = request.getQueryParameter("_format");
    if (formatParameter)
    {
        if (formatParameter.value() == "fhir+json")
        {
            return true;
        }
        else if(formatParameter.value() == "fhir+xml")
        {
            return false;
        }
        ErpFail(HttpStatus::NotAcceptable, "Requested format '" + formatParameter.value() + "' cannot be provided");
    }
    A_19539.finish();

    A_19539.start("2. priority: Accept attribute in HTTP-Header");
    const auto acceptHeader = request.header().header(Header::Accept);
    if (acceptHeader)
    {
        const auto acceptJson = request.header().getAcceptMimeType(MimeType::fhirJson.getMimeType());
        const auto acceptXml = request.header().getAcceptMimeType(MimeType::fhirXml.getMimeType());
        if (acceptJson && acceptXml)
        {
            return acceptJson->getQFactorWeight() > acceptXml->getQFactorWeight();
        }
        else if (acceptJson)
        {
            return true;
        }
        else if (acceptXml)
        {
            return false;
        }
        ErpFail(HttpStatus::NotAcceptable, "Requested format '" + acceptHeader.value() + "' cannot be provided");
    }
    A_19539.finish();

    A_19538.start("3. priority: default mime type application/fhir+json for responses to patients");
    A_19537.start("3. priority: default mime type application/fhir+xml for responses to LEI");
    // Get caller oid and compare it against those for an insurant or representative.
    // Extract oid of caller from jwt and compare against values of insurant and representative.
    const auto professionOIDClaim = request.getAccessToken().stringForClaim(JWT::professionOIDClaim);
    return professionOIDClaim.has_value() && professionOIDClaim == profession_oid::oid_versicherter;
}

void ErpRequestHandler::makeResponse(PcSessionContext& session, HttpStatus status,
                                     const model::ResourceBase* body)
{
    session.response.setStatus(status);
    if (body != nullptr)
    {
        if (session.callerWantsJson)
        {
            session.response.setHeader(Header::ContentType, ContentMimeType::fhirJsonUtf8);
            session.response.setBody(body->serializeToJsonString());
        }
        else
        {
            session.response.setHeader(Header::ContentType, ContentMimeType::fhirXmlUtf8);
            session.response.setBody(body->serializeToXmlString());
        }
    }
}

bool ErpRequestHandler::isVerificationIdentityKvnr(const std::string_view& kvnr)
{
    A_20751.start("Recognize the verification identity");
    std::string kvnr_(kvnr);
    ErpExpect(kvnr.size() == 10, HttpStatus::InternalServerError, "The kvnr " + kvnr_ + " is invalid");
    if (String::starts_with(kvnr_, "X0000"))
    {
        kvnr_ = kvnr_.substr(5, 4);
        try
        {
            int value = std::stoi(kvnr_);
            if (value >= 1 && value <= 5000)
            {
                return true;
            }
        }
        catch (const std::invalid_argument&)
        {
        }
        catch (const std::out_of_range&)
        {
        }
    }
    A_20751.finish();
    return false;
}

std::string ErpRequestHandler::makeFullUrl(const std::string_view tail)
{
    return std::string(getLinkBase() + std::string(tail));
}

std::string ErpRequestHandler::getLinkBase ()
{
    return Configuration::instance().getStringValue(ConfigurationKey::PUBLIC_E_PRESCRIPTION_SERVICE_URL);
}

std::string ErpRequestHandler::getLanguageFromHeader(const Header& requestHeader)
{
    return requestHeader.header(Header::AcceptLanguage).value_or("de");  // Default: German
}
void ErpRequestHandler::preHandleRequestHook(SessionContext<PcServiceContext>& session)
{
    session.callerWantsJson = callerWantsJson(session.request);
}

bool ErpRequestHandler::responseIsPartOfMultiplePages(const PagingArgument& pagingArgument, std::size_t numOfResults)
{
    return pagingArgument.getOffset() > 0 || pagingArgument.getCount() == numOfResults;
}

// ---

ErpUnconstrainedRequestHandler::ErpUnconstrainedRequestHandler (const Operation operation)
    : ErpRequestHandler(operation, {})
{
}

bool ErpUnconstrainedRequestHandler::allowedForProfessionOID (std::string_view /*professionOid*/) const
{
    return true;
}
