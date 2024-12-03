/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVICE_MEDICATIONDISPENSEHANDLERBASE_HXX
#define ERP_PROCESSING_CONTEXT_SERVICE_MEDICATIONDISPENSEHANDLERBASE_HXX


#include "erp/service/ErpRequestHandler.hxx"
#include "shared/service/Operation.hxx"

#include <vector>
#include <string>

class ErpElement;

namespace model
{
class GemErpPrMedication;
class MedicationDispense;
class MedicationDispenseBundle;
class MedicationsAndDispenses;
}

class MedicationDispenseHandlerBase: public ErpRequestHandler
{
public:
    MedicationDispenseHandlerBase(Operation operation, const std::initializer_list<std::string_view>& allowedProfessionOiDs);

protected:
    static model::MedicationsAndDispenses parseBody(PcSessionContext& session, Operation forOperation);

    static void checkMedicationDispenses(std::vector<model::MedicationDispense>& medicationDispenses,
                                const model::PrescriptionId& prescriptionId, const model::Kvnr& kvnr,
                                const std::string& telematikIdFromAccessToken);

    static model::MedicationDispenseBundle createBundle(const model::MedicationsAndDispenses& bodyData);

private:
    using  UnspecifiedResourceFactory = model::ResourceFactory<model::UnspecifiedResource>;
    static void checkSingleOrBundleAllowed(const model::MedicationDispense& medicationDispense);
    static model::MedicationDispense medicationDispensesSingle(UnspecifiedResourceFactory&& unspec);
    static std::vector<model::MedicationDispense> medicationDispensesFromBundle(UnspecifiedResourceFactory&& unspec);
    static model::ProfileType parameterTypeFor(Operation operation);
    static model::MedicationsAndDispenses medicationDispensesFromParameters(UnspecifiedResourceFactory&& unspec, Operation forOperation);
};

#endif
