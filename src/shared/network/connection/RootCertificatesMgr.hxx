/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_CLIENT_ROOTCERTIFICATESMGR_HXX
#define ERP_PROCESSING_CONTEXT_CLIENT_ROOTCERTIFICATESMGR_HXX

#include <string>
#include "shared/util/SafeString.hxx"

namespace boost
{
    namespace asio::ssl
    {
        class context;
    }
}

/**
 * Static class in charge of loading the set of trusted
 * root certificates that will be used for the validation of the
 * chain-of-certificates presented by a remote peer when establishing a secure connection.
 */
class RootCertificatesMgr
{
public:
    /**
     * Loads a set of provided as argument ( if it is empty set of default hardcoded-into-source-code )
     * root certificates into the given SSL context.
     */
    static void loadCaCertificates (boost::asio::ssl::context& context,
                                    const SafeString& caCertificates);
};

#endif
