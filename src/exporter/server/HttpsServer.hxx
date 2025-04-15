/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVER_EXPORTER_HTTPSSERVER_HXX
#define ERP_PROCESSING_CONTEXT_SERVER_EXPORTER_HTTPSSERVER_HXX

#include "shared/server/BaseHttpsServer.hxx"
#include "exporter/server/ServerSession.hxx"

#include "shared/server/handler/RequestHandlerManager.hxx"
#include "shared/util/SafeString.hxx"

#include <boost/asio/ssl/context.hpp>

class MedicationExporterServiceContext;
class RequestHandlerManager;

namespace exporter {

class HttpsServer : public BaseHttpsServer
{
public:
    // 0.0.0.0 means "all interfaces on the local machine", including local host and all ethernet drivers.
    // If a host has two IP addresses, and a server running on the host is configured to listen
    // on 0.0.0.0, it will be reachable at both of those IP addresses.
    static constexpr std::string_view defaultHost = "0.0.0.0";

    HttpsServer(const std::string_view address, uint16_t port, RequestHandlerManager&& requestHandlers,
                MedicationExporterServiceContext& serviceContext, bool enforceClientAuthentication = false,
                const SafeString& caCertificates = SafeString());

    MedicationExporterServiceContext& serviceContext();

private:
    MedicationExporterServiceContext& mServiceContext;
};

} // namespace exporter

#endif
