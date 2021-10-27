/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef EPA_CLIENT_OCSP_OCSPHELPER_HXX
#define EPA_CLIENT_OCSP_OCSPHELPER_HXX

#include "erp/util/OpenSsl.hxx"

#include <memory>


class OcspHelper
{
public:
    struct Asn1SequenceDeleter
    {
        void operator() (ASN1_SEQUENCE_ANY* asn1SequenceAny);
    };

    using Asn1SequencePtr = std::unique_ptr<ASN1_SEQUENCE_ANY, Asn1SequenceDeleter>;

    static const EVP_MD* getDigestFromExtension (ASN1_SEQUENCE_ANY* extensionData);

    static const ASN1_OCTET_STRING*
    getCertHashValueFromExtension (ASN1_SEQUENCE_ANY* extensionData);
};

#endif
