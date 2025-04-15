/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_TSL_CERTIFICATEID_HXX
#define EPA_LIBRARY_TSL_CERTIFICATEID_HXX

#include <string>

namespace epa
{

/**
 * Represents unambiguous ID of a certificate.
 */
class CertificateId
{
public:
    /**
     * Certificate subject, it is an informative part of the ID,
     * because it is a human readable string.
     */
    std::string subject;

    /**
     * Certificate subject key identifier, an unambiguous certificate identifier.
     */
    std::string subjectKeyIdentifier;

    bool operator==(const CertificateId& id) const;
    std::string toString() const;
};

} // namespace epa

template<>
struct std::hash<epa::CertificateId>
{
    std::size_t operator()(const epa::CertificateId& id) const;
};

#endif
