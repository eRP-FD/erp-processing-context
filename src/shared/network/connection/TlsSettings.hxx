/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVER_TLSSETTINGS_HXX
#define ERP_PROCESSING_CONTEXT_SERVER_TLSSETTINGS_HXX

#include <boost/asio/ssl/context.hpp>

#include <optional>

class TlsSettings
{
public:
    static void restrictVersions(boost::asio::ssl::context& sslContext);
    static void setAllowedCiphersAndCurves(boost::asio::ssl::context& sslContext,
                                           const std::optional<std::string>& forcedCiphers);
};


#endif
