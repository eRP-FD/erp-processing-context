/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_CLOSETASKHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_CLOSETASKHANDLER_HXX

#include "erp/service/task/TaskHandler.hxx"

namespace fhirtools
{
class Collection;
}

class ErpElement;

class CloseTaskHandler
    : public TaskHandlerBase
{
public:
    CloseTaskHandler(const std::initializer_list<std::string_view>& allowedProfessionOiDs);

    void handleRequest(PcSessionContext& session) override;

private:
    static std::vector<model::MedicationDispense> medicationDispensesFromBody(PcSessionContext& session);
    static void validateMedications(const std::vector<model::MedicationDispense>& medicationDispenses,
                                    const PcServiceContext& session);
    template<typename ModelT>
    static void validateWithoutMedicationProfiles(model::NumberAsStringParserDocument&& medicationDispenseOrBundleDoc,
                                                  std::string_view medicationsPath,
                                                  const PcSessionContext& session);
    static void validateSameMedicationVersion(const fhirtools::Collection& medications);

};

#endif
