/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_SHARED_ADMIN_ADMINREQUESTHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_SRC_SHARED_ADMIN_ADMINREQUESTHANDLER_HXX

#include "shared/server/RequestHandler.hxx"
#include "shared/server/handler/RequestHandlerInterface.hxx"
#include "shared/util/ConfigurationFormatter.hxx"

#include <memory>

class SessionContext;

class AdminRequestHandlerBase : public RequestHandlerBasicAuthentication
{
public:
    explicit AdminRequestHandlerBase(ConfigurationKey credentialsKey);
    void handleRequest(BaseSessionContext& session) override;

private:
    virtual void doHandleRequest(BaseSessionContext& session) = 0;
    ConfigurationKey mCredentialsKey;
};

class PostRestartHandler : public AdminRequestHandlerBase
{
public:
    explicit PostRestartHandler(ConfigurationKey adminCredentialsKey, ConfigurationKey adminDefaultShutdownDelayKey);
    ~PostRestartHandler() override;
    Operation getOperation(void) const override;

private:
    void doHandleRequest(BaseSessionContext& session) override;

    int mDefaultShutdownDelay;
    static constexpr const char* delay_parameter_name = "delay-seconds";
};

class GetConfigurationHandler : public AdminRequestHandlerBase
{
public:
    explicit GetConfigurationHandler(ConfigurationKey adminCredentialsKey,
                                     gsl::not_null<std::unique_ptr<ConfigurationFormatter>> formatter);
    Operation getOperation() const override;

private:
    void doHandleRequest(BaseSessionContext& session) override;
    std::unique_ptr<ConfigurationFormatter> mFormatter;
};


#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_ADMIN_ADMINREQUESTHANDLER_HXX
