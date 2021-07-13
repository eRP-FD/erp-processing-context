#ifndef ERP_PROCESSING_CONTEXT_SERVICE_MEDICATIONDISPENSEHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_SERVICE_MEDICATIONDISPENSEHANDLER_HXX

#include "erp/service/ErpRequestHandler.hxx"
#include "erp/database/Data.hxx"

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
