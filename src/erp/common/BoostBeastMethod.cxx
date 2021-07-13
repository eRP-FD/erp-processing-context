#include "erp/common/BoostBeastMethod.hxx"

boost::beast::http::verb toBoostBeastVerb (const HttpMethod method)
{
    switch(method)
    {
        case HttpMethod::HEAD: return boost::beast::http::verb::head;
        case HttpMethod::DELETE: return boost::beast::http::verb::delete_;
        case HttpMethod::GET: return boost::beast::http::verb::get;
        case HttpMethod::PATCH: return boost::beast::http::verb::patch;
        case HttpMethod::POST: return boost::beast::http::verb::post;
        case HttpMethod::PUT: return boost::beast::http::verb::put;
        default:
        case HttpMethod::UNKNOWN: return boost::beast::http::verb::unknown;
    }
}


HttpMethod fromBoostBeastVerb (const boost::beast::http::verb verb)
{
    switch(verb)
    {
        case boost::beast::http::verb::head: return HttpMethod::HEAD;
        case boost::beast::http::verb::delete_: return HttpMethod::DELETE;
        case boost::beast::http::verb::get: return HttpMethod::GET;
        case boost::beast::http::verb::patch: return HttpMethod::PATCH;
        case boost::beast::http::verb::post: return HttpMethod::POST;
        case boost::beast::http::verb::put: return HttpMethod::PUT;
        default:
        case boost::beast::http::verb::unknown: return HttpMethod::UNKNOWN;
    }
}
