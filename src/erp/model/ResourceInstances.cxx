#include "erp/model/Binary.hxx"
#include "erp/model/ChargeItem.hxx"
#include "erp/model/Communication.hxx"
#include "erp/model/Composition.hxx"
#include "erp/model/Consent.hxx"
#include "erp/model/Device.hxx"
#include "erp/model/ErxReceipt.hxx"
#include "erp/model/GemErpPrMedication.hxx"
#include "erp/model/KbvBundle.hxx"
#include "erp/model/KbvComposition.hxx"
#include "erp/model/KbvCoverage.hxx"
#include "erp/model/KbvMedicationBase.hxx"
#include "erp/model/KbvMedicationCompounding.hxx"
#include "erp/model/KbvMedicationFreeText.hxx"
#include "erp/model/KbvMedicationIngredient.hxx"
#include "erp/model/KbvMedicationPzn.hxx"
#include "erp/model/KbvMedicationRequest.hxx"
#include "erp/model/KbvOrganization.hxx"
#include "erp/model/KbvPracticeSupply.hxx"
#include "erp/model/KbvPractitioner.hxx"
#include "erp/model/MedicationDispense.hxx"
#include "erp/model/MedicationDispenseBundle.hxx"
#include "erp/model/MetaData.hxx"
#include "erp/model/Patient.hxx"
#include "erp/model/Reference.hxx"
#include "erp/model/Signature.hxx"
#include "erp/model/Subscription.hxx"
#include "erp/model/Task.hxx"
#include "erp/model/WorkflowParameters.hxx"
#include "erp/model/extensions/ChargeItemMarkingFlags.hxx"
#include "erp/model/extensions/KBVEXERPAccident.hxx"
#include "erp/model/extensions/KBVEXFORLegalBasis.hxx"
#include "erp/model/extensions/KBVMedicationCategory.hxx"
#include "erp/model/extensions/KBVMultiplePrescription.hxx"
#include "fhirtools/model/erp/ErpElement.hxx"
#include "fhirtools/validator/FhirPathValidator.hxx"
#include "fhirtools/validator/ValidationResult.hxx"
#include "shared/model/AuditData.hxx"
#include "shared/model/AuditEvent.hxx"
#include "shared/model/Bundle.hxx"
#include "shared/model/Extension.hxx"
#include "shared/model/Resource.hxx"
#include "shared/model/Resource.txx"
#include "shared/util/DurationConsumer.hxx"
#include "shared/util/String.hxx"
namespace model
{
// NOLINTBEGIN(bugprone-exception-escape, bugprone-forward-declaration-namespace)
template class Resource<AbgabedatenPkvBundle>;
template class Resource<AuditEvent>;
template class Resource<AuditMetaData>;
template class Resource<Binary>;
template class Resource<Bundle>;
template class Resource<ChargeItem>;
template class Resource<ChargeItemMarkingFlags>;
template class Resource<ChargeItemMarkingFlag>;
template class Resource<Composition>;
template class Resource<Communication>;
template class Resource<Consent>;
template class Resource<Device>;
template class Resource<Dosage>;
template class Resource<GemErpPrMedication>;
template class Resource<ErxReceipt>;
template class Resource<KbvBundle>;
template class Resource<KbvComposition>;
template class Resource<KbvCoverage>;
template class Resource<KBVEXERPAccident>;
template class Resource<KBVEXFORLegalBasis>;
template class Resource<KBVMedicationCategory>;
template class Resource<KbvMedicationGeneric>;
template class Resource<KbvMedicationCompounding>;
template class Resource<KbvMedicationFreeText>;
template class Resource<KbvMedicationIngredient>;
template class Resource<KbvMedicationPzn>;
template class Resource<KbvMedicationRequest>;
template class Resource<KBVMultiplePrescription>;
template class Resource<KBVMultiplePrescription::Kennzeichen>;
template class Resource<KBVMultiplePrescription::Nummerierung>;
template class Resource<KBVMultiplePrescription::Zeitraum>;
template class Resource<KBVMultiplePrescription::ID>;
template class Resource<KbvOrganization>;
template class Resource<KbvPracticeSupply>;
template class Resource<KbvPractitioner>;
template class Resource<MedicationDispense>;
template class Resource<MedicationDispenseBundle>;
template class Resource<MetaData>;
template class Resource<CreateTaskParameters>;
template class Resource<ActivateTaskParameters>;
template class Resource<Patient>;
template class Resource<PatchChargeItemParameters>;
template class Resource<Reference>;
template class Resource<Signature>;
template class Resource<Subscription>;
template class Resource<Task>;

// NOLINTEND(bugprone-exception-escape, bugprone-forward-declaration-namespace)
}
