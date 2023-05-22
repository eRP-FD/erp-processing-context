/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_TSL_CERTIFICATEID_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_TSL_CERTIFICATEID_HXX

#include <string>

class CertificateId
{
public:
    std::string subject;
    std::string subjectKeyIdentifier;

    bool operator==(const CertificateId& id) const;
    std::string toString() const;
};

template <>
struct std::hash<CertificateId>
{
    std::size_t operator()(const CertificateId& id) const;
};

#endif
