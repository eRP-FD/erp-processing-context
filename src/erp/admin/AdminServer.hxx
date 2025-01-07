/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_ADMIN_ADMINSERVER_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_ADMIN_ADMINSERVER_HXX
#include <memory>

class RequestHandlerManager;
namespace erp
{
class RuntimeConfiguration;
}
namespace AdminServer
{
void addEndpoints(RequestHandlerManager& manager, std::shared_ptr<const erp::RuntimeConfiguration> runtimeConfig);
}

#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_ADMIN_ADMINSERVER_HXX
