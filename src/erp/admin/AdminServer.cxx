/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/admin/AdminServer.hxx"
#include "erp/admin/AdminRequestHandler.hxx"
#include "erp/admin/PutRuntimeConfigHandler.hxx"
#include "erp/pc/PcServiceContext.hxx"
#include "erp/service/HealthHandler.hxx"

namespace AdminServer
{
void addEndpoints(RequestHandlerManager& manager)
{
    manager.onPostDo("/admin/shutdown", std::make_unique<PostRestartHandler>());
    manager.onGetDo("/admin/configuration", std::make_unique<GetConfigurationHandler>());
    manager.onGetDo("/health", std::make_unique<HealthHandler>());
    manager.onPutDo("/admin/configuration", std::make_unique<PutRuntimeConfigHandler>());
}
}
