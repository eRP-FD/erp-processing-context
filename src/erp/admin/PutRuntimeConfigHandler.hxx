// (C) Copyright IBM Deutschland GmbH 2021, 2024
// (C) Copyright IBM Corp. 2021, 2024
// non-exclusively licensed to gematik GmbH

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_ADMIN_PUTRUNTIMECONFIGHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_ADMIN_PUTRUNTIMECONFIGHANDLER_HXX
#include "erp/admin/AdminRequestHandler.hxx"

class PutRuntimeConfigHandler : public AdminRequestHandlerBase
{
public:
    explicit PutRuntimeConfigHandler();

    Operation getOperation() const override;

private:
    void doHandleRequest(SessionContext& session) override;
};


#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_ADMIN_PUTRUNTIMECONFIGHANDLER_HXX
