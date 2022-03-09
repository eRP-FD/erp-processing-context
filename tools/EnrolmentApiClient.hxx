/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_TOOLS_ENROLMENT_API_CLIENT_HXX
#define ERP_PROCESSING_CONTEXT_TOOLS_ENROLMENT_API_CLIENT_HXX

#include "erp/client/HttpClient.hxx"
#include "erp/client/HttpsClient.hxx"
#include "erp/common/Constants.hxx"
#include "erp/common/Header.hxx"
#include "erp/hsm/ErpTypes.hxx"
#include "erp/server/SslStream.hxx"
#include "erp/util/Base64.hxx"
#include "erp/util/Configuration.hxx"

#include <boost/format.hpp>
#include <chrono>
#include <optional>
#include <string_view>
#include <variant>

class EnrolmentApiClient : public ::HttpsClient
{
public:
    struct ValidityPeriod {
        ::std::optional<::std::chrono::system_clock::time_point> begin;
        ::std::optional<::std::chrono::system_clock::time_point> end;
    };

    using ::HttpsClient::HttpsClient;

    void deleteBlob(::BlobType type, ::std::string_view id);
    void storeBlob(::BlobType type, ::std::string_view id, const ::ErpBlob& blob, ValidityPeriod validity = {},
                   ::std::optional<::std::string> certificate = {});

private:
    class Header : public ::Header
    {
    public:
        Header(::HttpMethod method, ::BlobType type);
    };
};

#endif// ERP_PROCESSING_CONTEXT_TOOLS_ENROLMENT_API_CLIENT_HXX
