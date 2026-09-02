/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "ServerRequest.hxx"
#include "shared/model/PrescriptionId.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/UrlHelper.hxx"

namespace
{
ServerRequest::Type determineType(const std::string& path)
{
    if (path.starts_with("/pushers/v1") || path.starts_with("/channels/v1") || path.starts_with("/history/v1"))
    {
        return ServerRequest::Type::Push;
    }
    return ServerRequest::Type::Fhir;
}
}

ServerRequest::ServerRequest (Header header)
    : mHeader(std::move(header)),
      mBody(),
      mPathParameters(),
      mQueryParameters(),
      mFragment(),
      mAccessToken()
    , mType(determineType(mHeader.path()))
{
    mHeader.setContentLength(0);
}


const Header& ServerRequest::header () const
{
    return mHeader;
}


Header& ServerRequest::header ()
{
    return mHeader;
}


void ServerRequest::setHeader (Header header)
{
    mHeader = std::move(header);
    mHeader.setContentLength(mBody.size());
    mType = determineType(mHeader.path());
}


void ServerRequest::setMethod(HttpMethod method)
{
    mHeader.setMethod(method);
    mHeader.setContentLength(mBody.size());
}


void ServerRequest::setBody (std::string body)
{
    mBody = std::move(body);
    mHeader.setContentLength(mBody.size());
}


const std::string& ServerRequest::getBody () const
{
    return mBody;
}


void ServerRequest::setPathParameters (const std::vector<std::string>& keys, const std::vector<std::string>& values)
{
    if (keys.size() != values.size())
    {
        Fail2("ServerRequest::setPathParameters expects the same number of keys and values", std::runtime_error);
    }

    for (size_t index=0; index<keys.size(); ++index)
        mPathParameters[keys[index]] = values[index];
}


std::optional<std::string> ServerRequest::getPathParameter (const std::string& parameterName) const
{
    auto candidate = mPathParameters.find(parameterName);
    if (candidate != mPathParameters.end())
        return candidate->second;
    else
        return {};
}


size_t ServerRequest::getPathParameterCount () const
{
    return mPathParameters.size();
}


const std::unordered_map<std::string, std::string>& ServerRequest::getPathParameters () const
{
    return mPathParameters;
}


void ServerRequest::setQueryParameters (std::vector<std::pair<std::string,std::string>> parameters)
{
    mQueryParameters = std::move(parameters);
}


const std::vector<std::pair<std::string, std::string>>& ServerRequest::getQueryParameters () const
{
    return mQueryParameters;
}


std::optional<std::string> ServerRequest::getQueryParameter (const std::string& key) const
{
    std::optional<std::string> result;
    for (const auto& candidate : mQueryParameters)
    {
        if (candidate.first == key)
        {
            if (result.has_value())
                ErpFail(HttpStatus::BadRequest, "query parameter occurs more than once");
            else
                result = candidate.second;
        }
    }
    return result;
}


void ServerRequest::setFragment (std::string fragment)
{
    mFragment = std::move(fragment);
}


const std::string& ServerRequest::getFragment () const
{
    return mFragment;
}

void ServerRequest::setAccessToken(JWT jwt)
{
    mAccessToken = std::move(jwt);
}

const JWT& ServerRequest::getAccessToken() const
{
    return mAccessToken;
}

ServerRequest::Type ServerRequest::getType() const
{
    return mType;
}

std::optional<model::PrescriptionId> ServerRequest::tryParsePrescriptionIdFromPathId() const
{
    if (const auto idParam = getPathParameter("id"))
    {
        try
        {
            return model::PrescriptionId::fromString(idParam.value());
        }
        catch (const std::exception&)
        {
            return std::nullopt;
        }
    }
    return std::nullopt;
}
