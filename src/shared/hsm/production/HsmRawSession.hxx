/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_HSMRAWSESSION_HXX
#define ERP_PROCESSING_CONTEXT_HSMRAWSESSION_HXX

#include "shared/hsm/HsmIdentity.hxx"

#ifdef __APPLE__
    #error "HSM support is only available for Linux and Windows"
#else
    namespace hsmclient {
    #include <hsmclient/ERP_Client.h>
    }
#endif


class HsmRawSession
{
public:
    hsmclient::HSMSession rawSession;
    HsmIdentity::Identity identity{HsmIdentity::Identity::Work};

    HsmRawSession ();
    HsmRawSession (const HsmRawSession& other) = delete;
    HsmRawSession (HsmRawSession&& other) noexcept;
    ~HsmRawSession (void);
    HsmRawSession& operator= (const HsmRawSession& other) = delete;
    HsmRawSession& operator= (HsmRawSession&& other) noexcept;
};


#endif
