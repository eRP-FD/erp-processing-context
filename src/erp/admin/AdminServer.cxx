/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#include "erp/admin/AdminServer.hxx"
#include "erp/admin/AdminRequestHandler.hxx"
#include "erp/pc/PcServiceContext.hxx"
#include "erp/service/HealthHandler.hxx"

namespace AdminServer
{
void addEndpoints(RequestHandlerManager& manager)
{
    manager.onPostDo("/admin/shutdown", std::make_unique<PostRestartHandler>());
    manager.onGetDo("/health", std::make_unique<HealthHandler>());
}
}
