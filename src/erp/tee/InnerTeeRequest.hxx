/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_TEE_INNERTEEREQUEST_HXX
#define ERP_PROCESSING_CONTEXT_TEE_INNERTEEREQUEST_HXX

#include "shared/crypto/Jwt.hxx"
#include "shared/util/SafeString.hxx"

#include "shared/network/message/Header.hxx"

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
