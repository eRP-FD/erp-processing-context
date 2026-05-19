#include "erp/model/AbgabedatenPkvBundle.hxx"
#include "erp/model/ChargeItem.hxx"
#include "erp/model/Communication.hxx"
#include "erp/model/Consent.hxx"
#include "erp/model/ErxReceipt.hxx"
#include "erp/model/EvdgaBundle.hxx"
#include "erp/model/EvdgaHealthAppRequest.hxx"
#include "erp/model/GemErpChrgPrParPatchChargeItemInput.hxx"
#include "erp/model/KbvMedicationFreeText.hxx"
#include "erp/model/KbvMedicationIngredient.hxx"
#include "erp/model/KbvPracticeSupply.hxx"
#include "erp/model/KbvPractitioner.hxx"
#include "erp/model/MetaData.hxx"
#include "erp/model/Subscription.hxx"
#include "erp/model/Task.hxx"
#include "erp/model/WorkflowParameters.hxx"
#include "erp/model/eu/GemErpEuPrMedication.hxx"
#include "erp/model/eu/GemErpEuPrParAccessAuthorizationRequest.hxx"
#include "erp/model/eu/GemErpEuPrParAccessAuthorizationResponse.hxx"
#include "erp/model/eu/GemErpEuPrParCloseOperationInput.hxx"
#include "erp/model/eu/GemErpEuPrParGetPrescriptionInput.hxx"
#include "erp/model/eu/GemErpEuPrTaskInput.hxx"
#include "erp/model/extensions/ChargeItemMarkingFlags.hxx"
#include "erp/model/extensions/GemErpExEuIsRedeemableByProperties.hxx"
#include "erp/model/extensions/GemErpRedeemableByPatient.hxx"
#include "erp/model/extensions/KBVEXERPAccident.hxx"
#include "erp/model/extensions/KBVEXFORLegalBasis.hxx"
#include "shared/model/Bundle.txx"
#include "shared/model/GemErpEuPrOrganization.hxx"
#include "shared/model/GemErpEuPrPractitioner.hxx"
#include "shared/model/GemErpEuPrPractitionerRole.hxx"
#include "shared/model/Resource.txx"

namespace model
{
// NOLINTBEGIN(bugprone-exception-escape, bugprone-forward-declaration-namespace)
template class Resource<AbgabedatenPkvBundle>;
template class Resource<ChargeItem>;
template class Resource<ChargeItemMarkingFlags>;
template class Resource<ChargeItemMarkingFlag>;
template class Resource<Communication>;
template class Resource<Consent>;
template class Resource<EvdgaBundle>;
template class Resource<EvdgaHealthAppRequest>;
template class Resource<ErxReceipt>;
template class Resource<KBVEXERPAccident>;
template class Resource<KBVEXFORLegalBasis>;
template class Resource<GemErpChrgPrParPatchChargeItemInput>;

template class Resource<KbvMedicationFreeText>;
template class Resource<KbvMedicationIngredient>;
template class Resource<KbvPracticeSupply>;
template class Resource<KbvPractitioner>;
template class Resource<MetaData>;
template class Resource<CreateTaskParameters>;
template class Resource<ActivateTaskParameters>;
template class Resource<PatchChargeItemParameters>;
template class Resource<Subscription>;
template class Resource<Task>;
template class Resource<GemErpEuPrTaskInput>;

template class BundleBase<AbgabedatenPkvBundle>;
template class BundleBase<ErxReceipt>;
template class BundleBase<EvdgaBundle>;

template class Resource<GemErpEuPrParAccessAuthorizationRequest>;
template class Resource<GemErpEuPrParAccessAuthorizationResponse>;
template class Resource<GemErpEuPrParGetPrescriptionInput>;
template class Resource<GemErpEuPrParCloseOperationInput>;

template class Resource<GemErpExEuIsRedeemableByProperties>;
template class Resource<GemErpRedeemableByPatient>;
template class Resource<GemErpEuPrMedication>;

template class Resource<GemErpEuPrPractitioner>;
template class Resource<GemErpEuPrPractitionerRole>;
template class Resource<GemErpEuPrOrganization>;

// NOLINTEND(bugprone-exception-escape, bugprone-forward-declaration-namespace)
}
