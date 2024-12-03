/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/ocsp/OcspHelper.hxx"
#include "library/util/Assert.hxx"
#include "library/util/BinaryBuffer.hxx"

#include "fhirtools/util/Gsl.hxx"

namespace epa
{

void OcspHelper::Asn1SequenceDeleter::operator()(ASN1_SEQUENCE_ANY* asn1SequenceAny)
{
    if (asn1SequenceAny)
    {
        sk_ASN1_TYPE_pop_free(asn1SequenceAny, ASN1_TYPE_free);
    }
}


const EVP_MD* OcspHelper::getDigestFromExtension(ASN1_SEQUENCE_ANY* extensionData)
{
    if (! extensionData)
    {
        return nullptr;
    }

    const ASN1_TYPE* certHash{sk_ASN1_TYPE_value(extensionData, 0)};
    if (! certHash || certHash->type != V_ASN1_SEQUENCE)
    {
        return nullptr;
    }

    const ASN1_STRING* algorithmIdentifier{certHash->value.sequence};
    if (! algorithmIdentifier || ! algorithmIdentifier->data)
    {
        return nullptr;
    }

    const unsigned char* algorithmIdentifierBuffer{ASN1_STRING_get0_data(algorithmIdentifier)};
    if (! algorithmIdentifierBuffer)
    {
        return nullptr;
    }

    const Asn1SequencePtr algorithmIdentifierSequence{d2i_ASN1_SEQUENCE_ANY(
        nullptr, &algorithmIdentifierBuffer, ASN1_STRING_length(algorithmIdentifier))};

    if (! algorithmIdentifierSequence)
    {
        return nullptr;
    }

    const ASN1_TYPE* oidPtr{sk_ASN1_TYPE_value(algorithmIdentifierSequence.get(), 0)};
    if (! oidPtr || oidPtr->type != V_ASN1_OBJECT || oidPtr->value.object == nullptr)
    {
        return nullptr;
    }

    return EVP_get_digestbyobj(oidPtr->value.object);
}


const ASN1_OCTET_STRING* OcspHelper::getCertHashValueFromExtension(ASN1_SEQUENCE_ANY* extensionData)
{
    if (! extensionData)
    {
        return nullptr;
    }

    const ASN1_TYPE* certHashValue = sk_ASN1_TYPE_value(extensionData, 1);

    if (! certHashValue || certHashValue->type != V_ASN1_OCTET_STRING)
    {
        return nullptr;
    }
    return certHashValue->value.octet_string;
}


OcspResponsePtr OcspHelper::binaryBufferToOcspResponse(const BinaryBuffer& responseBuffer)
{
    if (responseBuffer.empty())
    {
        return nullptr;
    }
    const auto* buffer = responseBuffer.data();
    return OcspResponsePtr(
        d2i_OCSP_RESPONSE(nullptr, &buffer, gsl::narrow_cast<long>(responseBuffer.size())));
}


BinaryBuffer OcspHelper::ocspRequestToBinaryBuffer(OCSP_REQUEST& ocspRequest)
{
    unsigned char* buffer = nullptr;
    const int bufferLength = i2d_OCSP_REQUEST(&ocspRequest, &buffer);
    Assert(bufferLength > 0) << "Could not create OCSP request!";

    OpenSslBufferPtr bufferPtr(buffer);
    BinaryBuffer requestBuffer(buffer, static_cast<size_t>(bufferLength));

    return requestBuffer;
}


/**
 * Creates OCSP request, never returns empty pointer.
 */
OcspRequestPtr OcspHelper::createOcspRequest(OCSP_CERTID& certId)
{
    OcspRequestPtr request{OCSP_REQUEST_new()};
    Assert(request != nullptr) << "Could not create OCSP request!";

    OcspCertidPtr id(OCSP_CERTID_dup(&certId));
    Assert(id != nullptr) << "Could not create duplicate OCSP certificate id!";

    Assert(OCSP_request_add0_id(request.get(), id.get()))
        << "Could not set certificate id to OCSP request!";
    // after this, id will be part of request, so it does no longer have to be freed
    (void) id.release();

    return request;
}


SystemTime OcspHelper::toTimePoint(const ASN1_GENERALIZEDTIME* asn1Generalizedtime)
{
    Assert(asn1Generalizedtime != nullptr) << "asn1Generalizedtime must not be null";
    struct tm tm
    {
    };
    ASN1_TIME_to_tm(asn1Generalizedtime, &tm);
    return Time::systemTimeFromUtcTm(tm);
}

} // namespace epa
