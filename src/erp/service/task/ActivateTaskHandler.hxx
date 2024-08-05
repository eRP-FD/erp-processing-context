/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_ACTIVATETASKHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_ACTIVATETASKHANDLER_HXX

#include "erp/service/task/TaskHandler.hxx"

class CadesBesSignature;
class TslManager;
namespace fhirtools
{
class ValidatorOptions;
}
namespace model
{
enum class KbvStatusKennzeichen;
class MedicationRequest;
class KbvMedicationRequest;
class KbvBundle;
class KBVMultiplePrescription;
template<typename>
class ResourceFactory;
}

class ActivateTaskHandler : public TaskHandlerBase
{
public:
    ActivateTaskHandler(const std::initializer_list<std::string_view>& allowedProfessionOiDs);
    void handleRequest(PcSessionContext& session) override;

private:
    static std::tuple<HttpStatus, model::KbvBundle> prescriptionBundleFromXml(PcServiceContext& serviceContext,
                                                                              std::string_view prescription);
    static void checkMultiplePrescription(const std::optional<model::KBVMultiplePrescription>& mPExt,
                                          const model::PrescriptionType prescriptionType,
                                          std::optional<model::KbvStatusKennzeichen> legalBasisCode,
                                          const model::Timestamp& authoredOn);
    static void checkValidCoverage(const model::KbvBundle& bundle, const model::PrescriptionType prescriptionType);
    static void checkNarcoticsMatches(const model::KbvBundle& bundle);
    static void checkAuthoredOnEqualsSigningDate(const model::KbvMedicationRequest& medicationRequest, const model::Timestamp& signingTime);
    static bool checkPractitioner(const model::KbvBundle& bundle, PcSessionContext& session);
    static HttpStatus checkExtensions(const model::ResourceFactory<model::KbvBundle>& factory,
                                      Configuration::OnUnknownExtension onUnknownExtension,
                                      const fhirtools::FhirStructureRepository& fhirStructureRepo,
                                      const fhirtools::ValidatorOptions& valOpts);
    void setMvoExpiryAcceptDates(model::Task& task, const std::optional<date::year_month_day>& mvoEndDate,
                                 const date::year_month_day& signingDay) const;
};


#endif//ERP_PROCESSING_CONTEXT_ACTIVATETASKHANDLER_HXX
