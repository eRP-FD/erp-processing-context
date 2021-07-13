#ifndef ERP_PROCESSING_CONTEXT_TEE_INNERTEEREQUEST_HXX
#define ERP_PROCESSING_CONTEXT_TEE_INNERTEEREQUEST_HXX

#include "erp/crypto/Jwt.hxx"
#include "erp/util/SafeString.hxx"

#include "erp/common/Header.hxx"

#include <memory>
#include <string>


class InnerTeeRequest
{
public:
    explicit InnerTeeRequest (const SafeString& p);
    InnerTeeRequest (InnerTeeRequest&& other) noexcept;
    ~InnerTeeRequest (void);

    void parseHeaderAndBody();

    const std::string& version (void) const;
    const JWT& authenticationToken (void) const;
    JWT&& releaseAuthenticationToken (void);
    const std::string& requestId (void) const;
    const SafeString& aesKey (void) const;
    const Header& header (void) const;
    Header releaseHeader (void);
    const std::string& body (void) const;
    std::string releaseBody (void);

    struct Data;

private:
    std::unique_ptr<Data> mData;
};


#endif
