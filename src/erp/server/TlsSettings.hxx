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
