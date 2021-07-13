#include "erp/server/response/ServerResponse.hxx"

#include "erp/util/Expect.hxx"


ServerResponse::ServerResponse (void)
    : mHeader(),
      mBody(),
      mIsKeepAliveActive()
{
    mHeader.setContentLength(0);
}


ServerResponse::ServerResponse (Header header, std::string body)
    : mHeader(std::move(header)),
      mIsKeepAliveActive()
{
    Expect3(mHeader.method() == HttpMethod::UNKNOWN, "http method must not be set for client request",
            std::logic_error);
    setBody(std::move(body));
}


ServerResponse::ServerResponse (ServerResponse&& other) noexcept
    : mHeader(std::move(other.mHeader)),
      mBody(std::move(other.mBody)),
      mIsKeepAliveActive(other.mIsKeepAliveActive)
{
}


ServerResponse::~ServerResponse (void) = default;


void ServerResponse::setStatus (HttpStatus status)
{
    mHeader.setStatus(status);
    mHeader.setContentLength(mHeader.contentLength());
}


void ServerResponse::setHeader (const std::string& key, const std::string& value)
{
    mHeader.addHeaderField(key, value);
}


void ServerResponse::removeHeader (const std::string& key)
{
    mHeader.removeHeaderField(key);
}


std::optional<std::string> ServerResponse::getHeader (const std::string& key) const
{
    return mHeader.header(key);
}


const Header& ServerResponse::getHeader (void) const
{
    return mHeader;
}


void ServerResponse::setKeepAlive (bool keepAlive)
{
    if (mIsKeepAliveActive.has_value())
    {
        Expect(*mIsKeepAliveActive == keepAlive, "changing a previously set keep-alive value is not implemented");

        // Nothing to be done.
        return;
    }

    mIsKeepAliveActive = keepAlive;
    if ( ! mIsKeepAliveActive)
    {
        // If keep-alive is not desired, then set "Connection: close",
        // according to https://tools.ietf.org/html/rfc7230#section-6.3
        mHeader.addHeaderField(Header::Connection, Header::ConnectionClose);
    }
}


void ServerResponse::setBody (std::string&& body)
{
    mBody = std::move(body);
    mHeader.setContentLength (mBody.length());
}


void ServerResponse::setBody (const std::string& body)
{
    mBody = body;
    mHeader.setContentLength (mBody.length());
}


void ServerResponse::withoutBody (void)
{
    mBody.clear();
    mHeader.setContentLength(0);
}


const std::string& ServerResponse::getBody (void) const
{
    return mBody;
}

