/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVICE_MEDICATIONDISPENSEHANDLERBASE_HXX
#define ERP_PROCESSING_CONTEXT_SERVICE_MEDICATIONDISPENSEHANDLERBASE_HXX

#include "erp/model/MedicationDispense.hxx"
#include "erp/service/ErpRequestHandler.hxx"

#include <vector>
#include <string>

namespace fhirtools
{
class Collection;
}
class ErpElement;


class MedicationDispenseHandlerBase: public ErpRequestHandler
{
public:
    MedicationDispenseHandlerBase(Operation operation, const std::initializer_list<std::string_view>& allowedProfessionOiDs);

protected:
    static std::vector<model::MedicationDispense> medicationDispensesFromBody(PcSessionContext& session);
    static void checkMedicationDispenses(std::vector<model::MedicationDispense>& medicationDispenses,
                                const model::PrescriptionId& prescriptionId, const model::Kvnr& kvnr,
                                const std::string& telematikIdFromAccessToken);

    static model::Bundle createBundle(
        const std::vector<model::MedicationDispense>& medicationDispenses);
};

#endif
