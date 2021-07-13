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
