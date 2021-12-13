/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVICE_CONSENTDELETEHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_SERVICE_CONSENTDELETEHANDLER_HXX

#include "erp/service/ErpRequestHandler.hxx"


class ConsentDeleteHandler: public ErpRequestHandler
{
public:
    ConsentDeleteHandler(const std::initializer_list<std::string_view>& allowedProfessionOiDs);

    void handleRequest(PcSessionContext& session) override;
};


#endif
