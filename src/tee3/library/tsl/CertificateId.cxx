/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/tsl/CertificateId.hxx"

#include <boost/functional/hash.hpp>
namespace epa
{

bool CertificateId::operator==(const CertificateId& id) const
{
    return (subject == id.subject && subjectKeyIdentifier == id.subjectKeyIdentifier);
}


std::string CertificateId::toString() const
{
    return "CertificateId:[subject:[" + subject + "], subjectKeyIdentifier:[" + subjectKeyIdentifier
           + "]]";
}

} // namespace epa

std::size_t std::hash<epa::CertificateId>::operator()(const epa::CertificateId& id) const
{
    std::size_t seed = 0;
    boost::hash_combine(seed, id.subject);
    boost::hash_combine(seed, id.subjectKeyIdentifier);

    return seed;
}
