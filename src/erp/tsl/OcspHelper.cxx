/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "OcspHelper.hxx"


namespace
{
    const EVP_MD* getDigest(const ASN1_STRING* algorithmIdentifier,
                            const unsigned char* algorithmIdentifierBuffer)
    {
        const OcspHelper::Asn1SequencePtr algorithmIdentifierSequence{
            d2i_ASN1_SEQUENCE_ANY(nullptr, &algorithmIdentifierBuffer,
                                  ASN1_STRING_length(algorithmIdentifier))};

        if (!algorithmIdentifierSequence)
        {
            return nullptr;
        }

        const ASN1_TYPE* oidPtr{sk_ASN1_TYPE_value(algorithmIdentifierSequence.get(), 0)};
        if (!oidPtr || oidPtr->type != V_ASN1_OBJECT || oidPtr->value.object == nullptr)
        {
            return nullptr;
        }

        return EVP_get_digestbyobj(oidPtr->value.object);
    }
}

void OcspHelper::Asn1SequenceDeleter::operator() (ASN1_SEQUENCE_ANY* asn1SequenceAny)
{
    if (asn1SequenceAny)
    {
        sk_ASN1_TYPE_pop_free(asn1SequenceAny, ASN1_TYPE_free);
    }
}


const EVP_MD* OcspHelper::getDigestFromExtension (ASN1_SEQUENCE_ANY* extensionData)
{
    if (!extensionData)
    {
        return nullptr;
    }

    const ASN1_TYPE* certHash{sk_ASN1_TYPE_value(extensionData, 0)};
    if (!certHash || certHash->type != V_ASN1_SEQUENCE)
    {
        return nullptr;
    }

    const ASN1_STRING* algorithmIdentifier{certHash->value.sequence};
    if (!algorithmIdentifier || !algorithmIdentifier->data)
    {
        return nullptr;
    }

    const unsigned char* algorithmIdentifierBuffer{ASN1_STRING_get0_data(algorithmIdentifier)};
    if (!algorithmIdentifierBuffer)
    {
        return nullptr;
    }

    return getDigest(algorithmIdentifier, algorithmIdentifierBuffer);
}


const ASN1_OCTET_STRING*
OcspHelper::getCertHashValueFromExtension (ASN1_SEQUENCE_ANY* extensionData)
{
    if (!extensionData)
    {
        return nullptr;
    }

    const ASN1_TYPE* certHashValue = sk_ASN1_TYPE_value(extensionData, 1);

    if (!certHashValue || certHashValue->type != V_ASN1_OCTET_STRING)
    {
        return nullptr;
    }
    return certHashValue->value.octet_string;
}
