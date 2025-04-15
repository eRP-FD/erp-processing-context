/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVER_SESSION_SERVERSESSION_HXX
#define ERP_PROCESSING_CONTEXT_SERVER_SESSION_SERVERSESSION_HXX

#include "exporter/pc/MedicationExporterServiceContext.hxx"
#include "shared/network/connection/SslStream.hxx"
#include "shared/server/BaseServerSession.hxx"
#include "shared/server/request/ServerRequest.hxx"
#include "shared/server/response/ServerResponse.hxx"
#include "shared/server/PartialRequestHandler.hxx"


class RequestHandlerManager;
class AccessLog;

namespace exporter {

class ExporterRequestHandler : public PartialRequestHandler
{
public:
    ExporterRequestHandler(const RequestHandlerManager& requestHandlers, MedicationExporterServiceContext& serviceContext)
        : PartialRequestHandler(requestHandlers), mServiceContext(serviceContext) {}

	MedicationExporterServiceContext& mServiceContext;

	std::tuple<bool, std::optional<RequestHandlerManager::MatchingHandler>, ServerResponse> handleRequest(ServerRequest& request, AccessLog& accessLog) override;
};


/**
 * Each ServerSession instance processes requests on a single socket connection.
 * The ServerSession supports keep alive, reading requests and writing responses is done asynchronously.
 */
class ServerSession : public BaseServerSession
{
public:
    static std::shared_ptr<ServerSession> createShared(boost::asio::ip::tcp::socket&& socket,
                                                       boost::asio::ssl::context& context,
                                                       const RequestHandlerManager& requestHandlers,
                                                       MedicationExporterServiceContext& serviceContext);

    explicit ServerSession(boost::asio::ip::tcp::socket&& socket,
						   boost::asio::ssl::context& context,
                           std::unique_ptr<ExporterRequestHandler>&& requestHandler);

    ~ServerSession() = default;
};

} // namespace exporter

#endif
