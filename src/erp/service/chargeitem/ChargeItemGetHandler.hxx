/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVICE_CHARGEITEMGETHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_SERVICE_CHARGEITEMGETHANDLER_HXX

#include "erp/service/chargeitem/ChargeItemHandlerBase.hxx"


class ChargeItemGetAllHandler: public ChargeItemHandlerBase
{
public:
    ChargeItemGetAllHandler(const std::initializer_list<std::string_view>& allowedProfessionOiDs);

    void handleRequest(PcSessionContext& session) override;

private:
    model::Bundle createBundle(std::vector<model::ChargeItem>& chargeItems);
};


class ChargeItemGetByIdHandler: public ChargeItemHandlerBase
{
public:
    ChargeItemGetByIdHandler (const std::initializer_list<std::string_view>& allowedProfessionOiDs);

    void handleRequest(PcSessionContext& session) override;
};



#endif
