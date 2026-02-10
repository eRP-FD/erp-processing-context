/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
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
class KbvCoverage;
class EvdgaBundle;
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
    static void handlePrescriptionRequest(PcSessionContext& session, Database::TaskAndKey& taskAndKey,
                                          const class SignedPrescription& cadesBesSignature);
    template<typename KbvOrEvdgaBundle>
    static void handleGeneric(PcSessionContext& session, Database::TaskAndKey& taskAndKey,
                              const SignedPrescription& cadesBesSignature, const KbvOrEvdgaBundle& kbvOrEvdgaBundle,
                              bool isMvo,
                              std::optional<model::KbvStatusKennzeichen> legalBasisCode);
    static void handleDigaRequest(PcSessionContext& session, Database::TaskAndKey& taskAndKey,
                                  const SignedPrescription& cadesBesSignature);

    template<typename KbvOrEvdgaBundle>
    static KbvOrEvdgaBundle prescriptionBundleFromXml(SessionContext& sessionContext, std::string_view prescription);
    static void checkMultiplePrescription(const std::optional<model::KBVMultiplePrescription>& mPExt,
                                          const model::PrescriptionType prescriptionType,
                                          std::optional<model::KbvStatusKennzeichen> legalBasisCode,
                                          const model::Timestamp& authoredOn);
    static void checkValidCoverage(const model::KbvCoverage& coverage, model::PrescriptionType prescriptionType);
    static bool isPkvCoverage(const model::KbvCoverage& coverage);
    static void checkMedicationCategoryCode(const model::KbvBundle& bundle, model::PrescriptionType workflow);
    template<typename KbvOrEvdgaBundle>
    static void checkAuthoredOnEqualsSigningDate(const KbvOrEvdgaBundle& kbvOrEvdgaBundle,
                                                 const model::Timestamp& signingTime);
    template<typename KbvOrEvdgaBundle>
    static bool checkPractitioner(const KbvOrEvdgaBundle& bundle, PcSessionContext& session);
    static void setMvoExpiryAcceptDates(model::Task& task, const std::optional<date::year_month_day>& mvoEndDate,
                                        const date::year_month_day& signingDay);

    template<typename KbvOrEvdgaBundle>
    static model::Kvnr getKvnrFromPatientResource(const KbvOrEvdgaBundle& bundle);
    template<typename KbvOrEvdgaBundle>
    static void checkBundlePrescriptionId(const model::Task& task, const KbvOrEvdgaBundle& bundle);
    static std::vector<std::string_view>
    allowedProfessionOidsForQesSignature(model::PrescriptionType prescriptionType);
    template<typename KbvOrEvdgaBundle>
    static void setProfileVersionHeader(const KbvOrEvdgaBundle& bundle, SessionContext& session);

    static bool isPrescriptionEuRedeemable(const model::Task& task, const model::KbvBundle& bundle);
    static bool isPrescriptionEuRedeemable(const model::Task& task, const model::EvdgaBundle& bundle);
};


#endif//ERP_PROCESSING_CONTEXT_ACTIVATETASKHANDLER_HXX
