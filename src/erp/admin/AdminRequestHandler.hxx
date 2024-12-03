/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_ADMIN_ADMINREQUESTHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_ADMIN_ADMINREQUESTHANDLER_HXX

#include "erp/server/RequestHandler.hxx"
#include "erp/server/ServerSession.hxx"
#include "shared/server/handler/RequestHandlerInterface.hxx"

#include <memory>

class SessionContext;

class AdminRequestHandlerBase : public RequestHandlerBasicAuthentication
{
public:
    explicit AdminRequestHandlerBase(ConfigurationKey credentialsKey);
    void handleRequest(SessionContext& session) override;

private:
    virtual void doHandleRequest(SessionContext& session) = 0;
    ConfigurationKey mCredentialsKey;
};

class PostRestartHandler : public AdminRequestHandlerBase
{
public:
    explicit PostRestartHandler();
    ~PostRestartHandler() override;
    Operation getOperation(void) const override;

private:
    void doHandleRequest(SessionContext& session) override;

    int mDefaultShutdownDelay;
    static constexpr const char* delay_parameter_name = "delay-seconds";
};

class GetConfigurationHandler : public AdminRequestHandlerBase
{
public:
    explicit GetConfigurationHandler();
    Operation getOperation() const override;

private:
    void doHandleRequest(SessionContext& session) override;
};


#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_ADMIN_ADMINREQUESTHANDLER_HXX
