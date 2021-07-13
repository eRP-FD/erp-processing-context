#include "erp/common/BoostBeastHttpStatus.hxx"


boost::beast::http::status toBoostBeastStatus (HttpStatus status)
{
#define STATUS(boostStatus, eStatus, code) case HttpStatus::eStatus: return boost::beast::http::status::boostStatus;
    switch(status)
    {
#include "HttpStatus.inc.hxx"
        default: return boost::beast::http::status::unknown;
    }
#undef STATUS
}


HttpStatus fromBoostBeastStatus (boost::beast::http::status status)
{
#define STATUS(boostStatus, eStatus, code) case boost::beast::http::status::boostStatus: return HttpStatus::eStatus;
    switch(status)
    {
#include "HttpStatus.inc.hxx"
        default: return HttpStatus::Unknown;
    }
#undef STATUS
}
