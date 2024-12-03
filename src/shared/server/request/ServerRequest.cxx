/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "ServerRequest.hxx"
#include "shared/util/Expect.hxx"


ServerRequest::ServerRequest (Header header)
    : mHeader(std::move(header)),
      mBody(),
      mPathParameters(),
      mQueryParameters(),
      mFragment(),
      mAccessToken()
{
    mHeader.setContentLength(0);
}


const Header& ServerRequest::header (void) const
{
    return mHeader;
}


Header& ServerRequest::header (void)
{
    return mHeader;
}


void ServerRequest::setHeader (Header header)
{
    mHeader = std::move(header);
    mHeader.setContentLength(mBody.size());
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


const std::string& ServerRequest::getBody (void) const
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


size_t ServerRequest::getPathParameterCount (void) const
{
    return mPathParameters.size();
}


const std::unordered_map<std::string, std::string>& ServerRequest::getPathParameters (void) const
{
    return mPathParameters;
}


void ServerRequest::setQueryParameters (std::vector<std::pair<std::string,std::string>> parameters)
{
    mQueryParameters = std::move(parameters);
}


const std::vector<std::pair<std::string, std::string>>& ServerRequest::getQueryParameters (void) const
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


const std::string& ServerRequest::getFragment (void) const
{
    return mFragment;
}

void ServerRequest::setAccessToken(JWT jwt)
{
    mAccessToken = std::move(jwt);
}

const JWT& ServerRequest::getAccessToken(void) const
{
    return mAccessToken;
}
