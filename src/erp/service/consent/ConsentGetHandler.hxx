/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVICE_CONSENTGETHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_SERVICE_CONSENTGETHANDLER_HXX

#include "erp/service/ErpRequestHandler.hxx"


class ConsentGetHandler: public ErpRequestHandler
{
public:
    ConsentGetHandler(const std::initializer_list<std::string_view>& allowedProfessionOiDs);

    void handleRequest(PcSessionContext& session) override;
};


#endif
