/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_TOOLS_ENROLMENT_API_CLIENT_HXX
#define ERP_PROCESSING_CONTEXT_TOOLS_ENROLMENT_API_CLIENT_HXX

#include "erp/client/HttpClient.hxx"
#include "erp/client/HttpsClient.hxx"
#include "erp/common/Constants.hxx"
#include "erp/common/Header.hxx"
#include "erp/common/HttpMethod.hxx"
#include "erp/enrolment/EnrolmentModel.hxx"
#include "erp/enrolment/VsdmHmacKey.hxx"
#include "erp/hsm/ErpTypes.hxx"
#include "erp/server/SslStream.hxx"
#include "erp/util/Base64.hxx"
#include "erp/util/Configuration.hxx"

#include <chrono>
#include <optional>
#include <string_view>

class PcrSet;

namespace model
{
class NumberAsStringParserDocument;
}

class EnrolmentApiClient : public ::HttpsClient
{
public:
    struct ValidityPeriod {
        ::std::optional<::std::chrono::system_clock::time_point> begin;
        ::std::optional<::std::chrono::system_clock::time_point> end;
    };

    EnrolmentApiClient();
    using ::HttpsClient::HttpsClient;

    ::EnrolmentModel get(::std::string_view endpoint);
    ::EnrolmentModel getQuote(const ::std::string& nonceBase64, const ::PcrSet& pcrSet);

    ::std::string authenticateCredential(::std::string_view secretBase64, ::std::string_view credentialBase64,
                                         ::std::string_view akName);
    void deleteBlob(::BlobType type, ::std::string_view id);
    void storeBlob(::BlobType type, ::std::string_view id, const ::ErpBlob& blob, ValidityPeriod validity = {},
                   ::std::optional<::std::string> certificate = {});

    void storeVsdmKey(const ErpBlob& blob);
    void deleteVsdmKey(char operatorId, char version);

private:
    EnrolmentModel request(HttpMethod method, std::string_view endpoint, const std::string& body,
                           HttpStatus expectedResponseStatus = HttpStatus::OK);
    ::EnrolmentModel request(::HttpMethod method, ::std::string_view endpoint,
                             const ::model::NumberAsStringParserDocument& input);

    class Header : public ::Header
    {
    public:
        Header(::HttpMethod method, ::BlobType type);
        Header(::HttpMethod method, ::std::string_view endpoint);
    };
};

#endif// ERP_PROCESSING_CONTEXT_TOOLS_ENROLMENT_API_CLIENT_HXX
