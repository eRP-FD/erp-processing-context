/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "EnrolmentApiClient.hxx"
#include "fhirtools/model/NumberAsStringParserDocument.hxx"
#include "shared/common/Constants.hxx"
#include "shared/network/message/HttpMethod.hxx"
#include "shared/tpm/PcrSet.hxx"
#include "shared/util/Base64.hxx"
#include "shared/util/Configuration.hxx"
#include "src/shared/enrolment/EnrolmentRequestHandler.hxx"

#include <boost/format.hpp>

namespace
{
auto rawEpochSeconds(const ::std::chrono::system_clock::time_point& timestamp)
{
    return ::std::chrono::duration_cast<::std::chrono::seconds>(timestamp.time_since_epoch()).count();
}

}

EnrolmentApiClient::Header::Header(::HttpMethod method, ::BlobType type)
    : ::EnrolmentApiClient::Header::Header{method, [type]() {
                                               switch (type)
                                               {
                                                   case ::BlobType::EndorsementKey:
                                                       return "/Enrolment/KnownEndorsementKey";
                                                       break;
                                                   case ::BlobType::AttestationPublicKey:
                                                       return "/Enrolment/KnownAttestationKey";
                                                       break;
                                                   case ::BlobType::AttestationKeyPair:
                                                       return "/Enrolment";
                                                       break;
                                                   case ::BlobType::Quote:
                                                       return "/Enrolment/KnownQuote";
                                                       break;
                                                   case ::BlobType::EciesKeypair:
                                                       return "/Enrolment/EciesKeypair";
                                                       break;
                                                   case ::BlobType::TaskKeyDerivation:
                                                       return "/Enrolment/Task/DerivationKey";
                                                       break;
                                                   case ::BlobType::CommunicationKeyDerivation:
                                                       return "/Enrolment/Communication/DerivationKey";
                                                       break;
                                                   case ::BlobType::AuditLogKeyDerivation:
                                                       return "/Enrolment/AuditLog/DerivationKey";
                                                       break;
                                                   case ::BlobType::ChargeItemKeyDerivation:
                                                       return "/Enrolment/ChargeItem/DerivationKey";
                                                       break;
                                                   case ::BlobType::KvnrHashKey:
                                                       return "/Enrolment/KvnrHashKey";
                                                       break;
                                                   case ::BlobType::TelematikIdHashKey:
                                                       return "/Enrolment/TelematikIdHashKey";
                                                       break;
                                                   case ::BlobType::VauSig:
                                                       return "/Enrolment/VauSig";
                                                   case ::BlobType::PseudonameKey:
                                                       // Not allowed for enrolment.
                                                       break;
                                                   case BlobType::VauAut:
                                                       return "/Enrolment/VauAut";
                                               }
                                               Fail("encountered unhandled blob type");
                                           }()}
{
}

EnrolmentApiClient::Header::Header(::HttpMethod method, ::std::string_view endpoint)
    : ::Header{method,
               "",
               ::Header::Version_1_1,
               {{::Header::Authorization,
                 "Basic " + ::Configuration::instance().getStringValue(ConfigurationKey::ENROLMENT_API_CREDENTIALS)}},
               HttpStatus::Unknown}
{
    Expect(! ::Configuration::instance().getStringValue(ConfigurationKey::ENROLMENT_API_CREDENTIALS).empty(),
           "enrolment API credentials not set");

    setTarget(::std::string{endpoint});
    setKeepAlive(false);
}

EnrolmentApiClient::EnrolmentApiClient()
    : HttpsClient{ConnectionParameters{
          .hostname = Configuration::instance().serverHost(),
          .port = Configuration::instance().getStringValue(ConfigurationKey::ENROLMENT_SERVER_PORT),
          .connectionTimeoutSeconds = Constants::httpTimeoutInSeconds,
          .resolveTimeout = std::chrono::milliseconds{Configuration::instance().getIntValue(
              ConfigurationKey::HTTPCLIENT_RESOLVE_TIMEOUT_MILLISECONDS)},
          .tlsParameters = TlsConnectionParameters{.certificateVerifier =
                                                       TlsCertificateVerifier::withVerificationDisabledForTesting()}}}
{
}

::EnrolmentModel EnrolmentApiClient::get(::std::string_view endpoint)
{
    return request(HttpMethod::GET, endpoint, "");
}

::EnrolmentModel EnrolmentApiClient::getQuote(const ::std::string& nonceBase64, const ::PcrSet& pcrSet)
{
    ::model::NumberAsStringParserDocument input;
    input.setValue(::rapidjson::Pointer{::std::string{::EnrolmentRequestHandlerBase::requestNonce}}, nonceBase64);
    for (const auto& entry : pcrSet)
    {
        input.addToArray(::rapidjson::Pointer{::std::string{::EnrolmentRequestHandlerBase::requestPcrSet}},
                         rapidjson::Value{entry});
    }

    input.setValue(::rapidjson::Pointer{::std::string{::EnrolmentRequestHandlerBase::requestHashAlgorithm}},
                   ::EnrolmentRequestHandlerBase::hashAlgorithm);

    return request(::HttpMethod::POST, "/Enrolment/GetQuote", input);
}

::std::string EnrolmentApiClient::authenticateCredential(::std::string_view secretBase64,
                                                         ::std::string_view credentialBase64, ::std::string_view akName)
{
    ::model::NumberAsStringParserDocument input;
    input.setValue(::rapidjson::Pointer{::std::string{::EnrolmentRequestHandlerBase::requestSecret}}, secretBase64);
    input.setValue(::rapidjson::Pointer{::std::string{::EnrolmentRequestHandlerBase::requestCredential}},
                   credentialBase64);
    input.setValue(::rapidjson::Pointer{::std::string{::EnrolmentRequestHandlerBase::requestAkName}}, akName);

    const auto response =
        send(::ClientRequest{::EnrolmentApiClient::Header{HttpMethod::POST, "/Enrolment/AuthenticateCredential"},
                             input.serializeToJsonString()});
    Expect(response.getHeader().status() == HttpStatus::OK, "POST /Enrolment/AuthenticateCredential failed");

    return ::EnrolmentModel{response.getBody()}.getString("/plainTextCredential");
}

void EnrolmentApiClient::deleteBlob(::BlobType type, ::std::string_view id)
{
    const auto response =
        send(::ClientRequest{::EnrolmentApiClient::Header{::HttpMethod::DELETE, type},
                             ::boost::str(::boost::format{R"({"id": "%1%"})"} % ::Base64::encode(id))});
    Expect(response.getHeader().status() == HttpStatus::NoContent,
           ::std::string{"Blob deletion failed with code "}.append(toString(response.getHeader().status())));
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

    if (type == ::BlobType::VauSig || type == BlobType::VauAut)
    {
        Expect(certificate.has_value(), "VauSig/VauAut require a certificate");

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
        Header{(type == BlobType::VauSig || type == BlobType::VauAut) ? ::HttpMethod::POST : ::HttpMethod::PUT, type},
        request.str()});
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

void EnrolmentApiClient::storeVsdmKey(const ErpBlob& vsdmKeyBlob)
{
    std::ostringstream os;
    os << R"({"blob":{"generation":)" << vsdmKeyBlob.generation << R"(,"data":")" << Base64::encode(vsdmKeyBlob.data)
       << "\"}}";
    const auto response = request(HttpMethod::POST, "/Enrolment/VsdmHmacKey", os.str(), HttpStatus::Created);
}

void EnrolmentApiClient::deleteVsdmKey(char operatorId, char version)
{
    std::ostringstream os;
    os << R"({"vsdm":{"betreiberkennung":")" << operatorId << R"(","version":")" << version << R"("}})";
    const auto response = send(ClientRequest{EnrolmentApiClient::Header{HttpMethod::DELETE, "/Enrolment/VsdmHmacKey"}, os.str()});
    const auto status = response.getHeader().status();
    Expect(status == HttpStatus::NoContent || status == HttpStatus::NotFound,
           std::string{"VSDM Key Blob deletion failed with code "}.append(toString(response.getHeader().status())));
}

EnrolmentModel EnrolmentApiClient::request(HttpMethod method, std::string_view endpoint, const std::string& body,
                                           HttpStatus expectedResponseStatus)
{
    const auto response = send(ClientRequest{EnrolmentApiClient::Header{method, endpoint}, body});
    Expect(response.getHeader().status() == expectedResponseStatus,
           (boost::format{"Request to '%1%' failed with code %2%\nInput: %3%"} % endpoint %
            toString(response.getHeader().status()) % body)
               .str());

    return response.getBody().empty() ? EnrolmentModel() : EnrolmentModel(response.getBody());
}


EnrolmentModel EnrolmentApiClient::request(::HttpMethod method, ::std::string_view endpoint,
                                             const ::model::NumberAsStringParserDocument& input)
{
    return request(method, endpoint, input.IsNull() ? std::string{} : input.serializeToJsonString());
}
