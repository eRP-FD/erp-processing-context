/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVICE_CHARGEITEM_CHARGEITEMHANDLERBASE_HXX
#define ERP_PROCESSING_CONTEXT_SERVICE_CHARGEITEM_CHARGEITEMHANDLERBASE_HXX


#include "erp/service/ErpRequestHandler.hxx"


namespace model
{
class ChargeItem;
}


class ChargeItemHandlerBase : public ErpRequestHandler
{
public:
    ChargeItemHandlerBase(Operation operation, const std::initializer_list<std::string_view>& allowedProfessionOIDs);

protected:
    static model::PrescriptionId parseIdFromPath(
        const ServerRequest& request,
        AccessLog& accessLog,
        const std::string& paramName = "id");

    static model::PrescriptionId parseIdFromQuery(
        const ServerRequest& request,
        AccessLog& accessLog,
        const std::string& paramName = "id");

    static void verifyPharmacyAccessCode(
        const ServerRequest& request,
        const model::ChargeItem& chargeItem,
        bool tryHttpHeader = false);

    static std::string createPharmacyAccessCode();
};


#endif