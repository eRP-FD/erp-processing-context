
/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "EnrolmentApiClient.hxx"
#include "erp/common/Constants.hxx"
#include "erp/util/Base64.hxx"
#include "erp/util/Configuration.hxx"

#include <boost/format.hpp>

namespace
{
auto rawEpochSeconds(const ::std::chrono::system_clock::time_point& timestamp)
{
    return ::std::chrono::duration_cast<::std::chrono::seconds>(timestamp.time_since_epoch()).count();
}

}

EnrolmentApiClient::Header::Header(::HttpMethod method, ::BlobType type)
    : ::Header{method,
               "",
               ::Header::Version_1_1,
               {{::Header::Authorization,
                 "Basic " + Configuration::instance().getStringValue(ConfigurationKey::ENROLMENT_API_CREDENTIALS)}},
               HttpStatus::Unknown}
{
    const auto baseEndpoint = ::std::string{"/Enrolment"};

    switch (type)
    {
        case ::BlobType::EndorsementKey:
            setTarget(baseEndpoint + "/KnownEndorsementKey");
            break;
        case ::BlobType::AttestationPublicKey:
            setTarget(baseEndpoint + "/KnownAttestationKey");
            break;
        case ::BlobType::AttestationKeyPair:
            setTarget(baseEndpoint);
            break;
        case ::BlobType::Quote:
            setTarget(baseEndpoint + "/KnownQuote");
            break;
        case ::BlobType::EciesKeypair:
            setTarget(baseEndpoint + "/EciesKeypair");
            break;
        case ::BlobType::TaskKeyDerivation:
            setTarget(baseEndpoint + "/Task/DerivationKey");
            break;
        case ::BlobType::CommunicationKeyDerivation:
            setTarget(baseEndpoint + "/Communication/DerivationKey");
            break;
        case ::BlobType::AuditLogKeyDerivation:
            setTarget(baseEndpoint + "/AuditLog/DerivationKey");
            break;
        case ::BlobType::KvnrHashKey:
            setTarget(baseEndpoint + "/KvnrHashKey");
            break;
        case ::BlobType::TelematikIdHashKey:
            setTarget(baseEndpoint + "/TelematikIdHashKey");
            break;
        case ::BlobType::VauSig:
            setTarget(baseEndpoint + "/VauSig");
            break;
    }
}

void EnrolmentApiClient::deleteBlob(::BlobType type, ::std::string_view id)
{
    const auto response =
        send(::ClientRequest{::EnrolmentApiClient::Header{::HttpMethod::DELETE, type},
                             ::boost::str(::boost::format{R"({"id": "%1%"})"} % ::Base64::encode(id))});
    if (response.getHeader().status() != HttpStatus::NoContent)
    {
        Fail(::std::string{"Blob deletion failed with code "}.append(toString(response.getHeader().status())));
    }
}

void EnrolmentApiClient::storeBlob(::BlobType type, ::std::string_view id, const ::ErpBlob& blob,
                                   ValidityPeriod validity, ::std::optional<::std::string> certificate)
{
    auto request = ::boost::format(R"({
  "id": "%1%",
  "blob": {
    "data": "%2%",
    "generation": %3%
  }%4%
})") % ::Base64::encode(id) %
                   ::Base64::encode(blob.data) % blob.generation;

    if (type == ::BlobType::VauSig)
    {
        Expect(certificate.has_value(), "VauSig requires a certificate");

        auto additionalVauSigData = (::boost::format{R"(,
  "certificate": "%1%")"} % ::Base64::encode(certificate.value()))
                                        .str();

        if (validity.begin.has_value())
        {
            additionalVauSigData += R"(,
  "valid-from": )" + ::std::to_string(rawEpochSeconds(validity.begin.value()));
        }

        request % additionalVauSigData;
    }
    else
    {
        if (validity.begin.has_value() || validity.end.has_value())
        {
            auto validitySection = ::boost::format{R"(,
  "metadata": {
    %1%
  })"};

            if (validity.begin.has_value() && validity.end.has_value())
            {
                const auto startDateTime = rawEpochSeconds(validity.begin.value());
                const auto endDateTime = rawEpochSeconds(validity.end.value());
                validitySection % ::boost::str(::boost::format{R"("startDateTime": %1%,
"endDateTime": %2%)"} % startDateTime % endDateTime);
            }
            else if (validity.end.has_value())
            {
                const auto expiryDateTime = rawEpochSeconds(validity.end.value());

                validitySection % ::boost::str(::boost::format{R"("expiryDateTime": %1%)"} % expiryDateTime);
            }

            request % validitySection.str();
        }
        else
        {
            request % "";
        }
    }

    const auto response = send(ClientRequest{
        Header{(type == ::BlobType::VauSig) ? ::HttpMethod::POST : HttpMethod::PUT, type}, request.str()});
    if (response.getHeader().status() != ::HttpStatus::Created)
    {
        ::std::string details;

        if (! response.getBody().empty())
        {
            details = "\nDetails:\n" + response.getBody();
        }

        Fail(::std::string{"Blob storing failed with code "}
                 .append(toString(response.getHeader().status()))
                 .append(details));
    }
}
