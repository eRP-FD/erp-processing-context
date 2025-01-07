/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/admin/AdminServer.hxx"
#include "erp/admin/PutRuntimeConfigHandler.hxx"
#include "erp/pc/PcServiceContext.hxx"
#include "erp/service/HealthHandler.hxx"
#include "erp/util/ConfigurationFormatter.hxx"
#include "erp/util/RuntimeConfiguration.hxx"
#include "shared/admin/AdminRequestHandler.hxx"

namespace AdminServer
{
void addEndpoints(RequestHandlerManager& manager, std::shared_ptr<const erp::RuntimeConfiguration> runtimeConfig)
{
    manager.onPostDo("/admin/shutdown",
                     std::make_unique<PostRestartHandler>(ConfigurationKey::ADMIN_CREDENTIALS,
                                                          ConfigurationKey::ADMIN_DEFAULT_SHUTDOWN_DELAY_SECONDS));
    manager.onGetDo("/admin/configuration",
                    std::make_unique<GetConfigurationHandler>(
                        ConfigurationKey::ADMIN_CREDENTIALS,
                        std::make_unique<erp::ConfigurationFormatter>(std::move(runtimeConfig))));
    manager.onGetDo("/health", std::make_unique<HealthHandler>());
    manager.onPutDo("/admin/configuration",
                    std::make_unique<PutRuntimeConfigHandler>(ConfigurationKey::ADMIN_RC_CREDENTIALS));
}
}
