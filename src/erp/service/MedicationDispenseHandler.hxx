/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVICE_MEDICATIONDISPENSEHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_SERVICE_MEDICATIONDISPENSEHANDLER_HXX

#include "erp/service/ErpRequestHandler.hxx"
#include "erp/service/MedicationDispenseHandlerBase.hxx"

#include <vector>
#include <string>

class GetAllMedicationDispenseHandler: public MedicationDispenseHandlerBase
{
public:
    GetAllMedicationDispenseHandler(const std::initializer_list<std::string_view>& allowedProfessionOiDs);
    void handleRequest(PcSessionContext& session) override;
};

#endif
