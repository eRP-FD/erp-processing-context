/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_TOOLS_ENROLMENT_API_CLIENT_HXX
#define ERP_PROCESSING_CONTEXT_TOOLS_ENROLMENT_API_CLIENT_HXX

#include "shared/common/Constants.hxx"
#include "shared/enrolment/EnrolmentModel.hxx"
#include "shared/hsm/ErpTypes.hxx"
#include "shared/network/client/HttpClient.hxx"
#include "shared/network/client/HttpsClient.hxx"
#include "shared/network/connection/SslStream.hxx"
#include "shared/network/message/Header.hxx"
#include "shared/network/message/HttpMethod.hxx"
#include "shared/util/Base64.hxx"
#include "shared/util/Configuration.hxx"
#include "src/shared/enrolment/VsdmHmacKey.hxx"

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
