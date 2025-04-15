/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/tsl/CertificateId.hxx"


bool CertificateId::operator==(const CertificateId& id) const
{
    return (subject == id.subject && subjectKeyIdentifier == id.subjectKeyIdentifier);
}


std::string CertificateId::toString() const
{
    return "CertificateId:[subject:[" + subject + "], subjectKeyIdentifier:[" + subjectKeyIdentifier + "]]";
}


std::size_t std::hash<CertificateId>::operator()(const CertificateId& id) const
{
    return ((hash<string>()(id.subject) ^ (hash<string>()(id.subjectKeyIdentifier) << 1)) >> 1);
}
