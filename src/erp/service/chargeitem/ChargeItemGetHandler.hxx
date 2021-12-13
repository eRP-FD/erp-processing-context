/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVICE_CHARGEITEMGETHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_SERVICE_CHARGEITEMGETHANDLER_HXX

#include "erp/service/ErpRequestHandler.hxx"


class ChargeItemGetAllHandler: public ErpRequestHandler
{
public:
    ChargeItemGetAllHandler(const std::initializer_list<std::string_view>& allowedProfessionOiDs);

    void handleRequest(PcSessionContext& session) override;
};


class ChargeItemGetByIdHandler: public ErpRequestHandler
{
public:
    ChargeItemGetByIdHandler (const std::initializer_list<std::string_view>& allowedProfessionOiDs);

    void handleRequest(PcSessionContext& session) override;
};



#endif
