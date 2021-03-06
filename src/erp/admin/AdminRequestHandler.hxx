/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_ADMIN_ADMINREQUESTHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_ADMIN_ADMINREQUESTHANDLER_HXX

#include "erp/server/handler/RequestHandlerInterface.hxx"

#include <memory>

class AdminRequestHandlerBase : public RequestHandlerBasicAuthentication
{
public:
    void handleRequest(SessionContext& session) override;

private:
    virtual void doHandleRequest(SessionContext& session) = 0;
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


#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_ADMIN_ADMINREQUESTHANDLER_HXX
