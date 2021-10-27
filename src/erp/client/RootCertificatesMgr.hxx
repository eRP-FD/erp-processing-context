/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_CLIENT_ROOTCERTIFICATESMGR_HXX
#define ERP_PROCESSING_CONTEXT_CLIENT_ROOTCERTIFICATESMGR_HXX

#include <string>
#include "erp/util/SafeString.hxx"

namespace boost
{
    namespace asio::ssl
    {
        class context;
    }

    namespace system
    {
        class error_code;
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
