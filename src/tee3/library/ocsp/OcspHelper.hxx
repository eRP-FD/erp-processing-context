/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_OCSP_OCSPHELPER_HXX
#define EPA_LIBRARY_OCSP_OCSPHELPER_HXX

#include "library/util/BinaryBuffer.hxx"
#include "library/util/Time.hxx"

#include <memory>

#include "shared/crypto/OpenSslHelper.hxx"

namespace epa
{

class OcspHelper
{
public:
    struct Asn1SequenceDeleter
    {
        void operator()(ASN1_SEQUENCE_ANY* asn1SequenceAny);
    };

    using Asn1SequencePtr = std::unique_ptr<ASN1_SEQUENCE_ANY, Asn1SequenceDeleter>;

    static const EVP_MD* getDigestFromExtension(ASN1_SEQUENCE_ANY* extensionData);

    static const ASN1_OCTET_STRING* getCertHashValueFromExtension(ASN1_SEQUENCE_ANY* extensionData);

    static OcspResponsePtr binaryBufferToOcspResponse(const BinaryBuffer& responseBuffer);

    static BinaryBuffer ocspRequestToBinaryBuffer(OCSP_REQUEST& ocspRequest);

    /**
     * Creates OCSP request, never returns empty pointer.
     *
     * @throws std::runtime_error in case of problems during creation
     */
    static OcspRequestPtr createOcspRequest(OCSP_CERTID& certId);

    static SystemTime toTimePoint(const ASN1_GENERALIZEDTIME* asn1Generalizedtime);
};
} // namespace epa
#endif
