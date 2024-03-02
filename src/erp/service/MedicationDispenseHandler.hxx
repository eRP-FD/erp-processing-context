/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVICE_MEDICATIONDISPENSEHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_SERVICE_MEDICATIONDISPENSEHANDLER_HXX

#include "erp/service/ErpRequestHandler.hxx"

#include <vector>
#include <string>

class GetAllMedicationDispenseHandler: public ErpRequestHandler
{
public:
    GetAllMedicationDispenseHandler(const std::initializer_list<std::string_view>& allowedProfessionOiDs);
    void handleRequest(PcSessionContext& session) override;
private:
    static model::Bundle createBundle(
        const std::vector<model::MedicationDispense>& medicationDispenses);
};

class GetMedicationDispenseHandler: public ErpRequestHandler
{
public:
    GetMedicationDispenseHandler(const std::initializer_list<std::string_view>& allowedProfessionOiDs);
    void handleRequest(PcSessionContext& session) override;
};

#endif
