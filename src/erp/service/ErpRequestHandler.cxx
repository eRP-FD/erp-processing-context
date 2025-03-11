/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/service/ErpRequestHandler.hxx"
#include "erp/util/search/PagingArgument.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/model/ProfessionOid.hxx"
#include "shared/server/response/ServerResponse.hxx"
#include "shared/util/TLog.hxx"

#include <utility>


// GEMREQ-start allowedProfessionOIDs
ErpRequestHandler::ErpRequestHandler(const Operation operation,
                                     const std::initializer_list<std::string_view>& allowedProfessionOIDs)
    : mOperation(operation)
    , mAllowedProfessionOIDsAllWorkflows(allowedProfessionOIDs)
{
}
ErpRequestHandler::ErpRequestHandler(Operation operation, OIDsByWorkflow allowedProfessionOiDsByWorkflow)
    : mOperation(operation)
    , mAllowedProfessionOIDsByWorkflow(std::move(allowedProfessionOiDsByWorkflow))
{
}
// GEMREQ-end allowedProfessionOIDs


// GEMREQ-start allowedForProfessionOID
bool ErpRequestHandler::allowedForProfessionOID (std::string_view professionOid,
                                                 const std::optional<std::string>& optionalPathIdParameter) const
{
    if (mAllowedProfessionOIDsAllWorkflows.contains(professionOid))
    {
        return true;
    }
    if (optionalPathIdParameter.has_value())
    {
        try
        {
            auto prescriptionId = model::PrescriptionId::fromString(optionalPathIdParameter.value());
            auto workflow = prescriptionId.type();
            return mAllowedProfessionOIDsByWorkflow.contains(workflow) &&
                   mAllowedProfessionOIDsByWorkflow.at(workflow).contains(professionOid);
        }
        catch (const model::ModelException&)
        {
            return false;
        }
    }
    return false;
}
// GEMREQ-end allowedForProfessionOID


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

model::PrescriptionId ErpRequestHandler::parseId(const ServerRequest& request, AccessLog& accessLog)
{
    const auto prescriptionIdValue = request.getPathParameter("id");
    ErpExpect(prescriptionIdValue.has_value(), HttpStatus::BadRequest, "id path parameter is missing");

    try
    {
        auto prescriptionId = model::PrescriptionId::fromString(prescriptionIdValue.value());
        accessLog.prescriptionId(prescriptionId);
        return prescriptionId;
    }
    catch (const model::ModelException&)
    {
        ErpFail(HttpStatus::NotFound, "Failed to parse prescription ID from URL parameter");
    }
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
            return false;
        }
        catch (const std::out_of_range&)
        {
            return false;
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

std::optional<std::string> ErpRequestHandler::getLanguageFromHeader(const Header& requestHeader)
{
    return requestHeader.header(Header::AcceptLanguage);
}
void ErpRequestHandler::preHandleRequestHook(SessionContext& session)
{
    session.callerWantsJson = callerWantsJson(session.request);
}

bool ErpRequestHandler::responseIsPartOfMultiplePages(const PagingArgument& pagingArgument, std::size_t numOfResults)
{
    return pagingArgument.getOffset() > 0 || pagingArgument.getCount() == numOfResults;
}

void ErpRequestHandler::checkValidEncoding(const ContentMimeType& contentMimeType)
{
    const auto& encoding = contentMimeType.getEncoding();
    bool validEncoding = !encoding || encoding->empty() || encoding.value() == Encoding::utf8;
    ErpExpect(validEncoding, HttpStatus::UnsupportedMediaType,
              "Invalid content type encoding received: " + std::string(contentMimeType));
}

// ---

ErpUnconstrainedRequestHandler::ErpUnconstrainedRequestHandler (const Operation operation)
    : ErpRequestHandler(operation, {})
{
}

bool ErpUnconstrainedRequestHandler::allowedForProfessionOID(std::string_view, const std::optional<std::string>&) const
{
    return true;
}
