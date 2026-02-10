// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_ADMIN_PUTRUNTIMECONFIGHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_ADMIN_PUTRUNTIMECONFIGHANDLER_HXX
#include "shared/admin/AdminRequestHandler.hxx"

namespace exporter
{
class RuntimeConfigurationSetter;

class PutRuntimeConfigHandler : public AdminRequestHandlerBase
{
public:
    explicit PutRuntimeConfigHandler(ConfigurationKey adminRcCredentialsKey);

    Operation getOperation() const override;

private:
    void doHandleRequest(BaseSessionContext& session) override;
    static void handleParameterPause(RuntimeConfigurationSetter& runtimeConfig, const std::string& param);
    static void handleParameterResume(RuntimeConfigurationSetter& runtimeConfig, const std::string& param);
    static void handleParameterThrottle(RuntimeConfigurationSetter& runtimeConfig, const std::string& param);
    static void handleParameterMetricsLogThreshold(RuntimeConfigurationSetter& runtimeConfig, const std::string& key,
                                                   const std::string& param);
};

}// namespace exporter

#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_ADMIN_PUTRUNTIMECONFIGHANDLER_HXX
