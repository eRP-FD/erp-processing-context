#include "shared/model/AuditData.hxx"
#include "shared/model/AuditEvent.hxx"
#include "shared/model/Binary.hxx"
#include "shared/model/Bundle.hxx"
#include "shared/model/Bundle.txx"
#include "shared/model/Composition.hxx"
#include "shared/model/Device.hxx"
#include "shared/model/GemErpPrMedication.hxx"
#include "shared/model/KbvBundle.hxx"
#include "shared/model/KbvCoverage.hxx"
#include "shared/model/KbvComposition.hxx"
#include "shared/model/KbvMedicationBase.hxx"
#include "shared/model/KbvMedicationCompounding.hxx"
#include "shared/model/KbvMedicationPzn.hxx"
#include "shared/model/KbvMedicationRequest.hxx"
#include "shared/model/KbvOrganization.hxx"
#include "shared/model/MedicationDispense.hxx"
#include "shared/model/MedicationDispenseBundle.hxx"
#include "shared/model/MedicationDispenseOperationParameters.hxx"
#include "shared/model/Reference.hxx"
#include "shared/model/Resource.hxx"
#include "shared/model/Resource.txx"
#include "shared/model/Patient.hxx"
#include "shared/model/Signature.hxx"
#include "shared/model/extensions/KBVMedicationCategory.hxx"
#include "shared/model/extensions/KBVMultiplePrescription.hxx"

namespace model
{

template class Resource<AuditEvent>;
template class Resource<AuditMetaData>;
template class Resource<Binary>;
template class Resource<Bundle>;
template class Resource<Composition>;
template class Resource<Device>;
template class Resource<Dosage>;
template class Resource<GemErpPrMedication>;
template class Resource<KbvBundle >;
template class Resource<KbvComposition>;
template class Resource<KbvCoverage>;
template class Resource<KBVMedicationCategory>;
template class Resource<KbvMedicationGeneric>;
template class Resource<KbvMedicationCompounding>;
template class Resource<KbvMedicationPzn>;
template class Resource<KbvMedicationRequest>;
template class Resource<KBVMultiplePrescription>;
template class Resource<KBVMultiplePrescription::Kennzeichen>;
template class Resource<KBVMultiplePrescription::Nummerierung>;
template class Resource<KBVMultiplePrescription::Zeitraum>;
template class Resource<KBVMultiplePrescription::ID>;
template class Resource<KbvOrganization>;
template class Resource<MedicationDispense>;
template class Resource<MedicationDispenseBundle>;
template class Resource<MedicationDispenseOperationParameters>;
template class Resource<Patient>;
template class Resource<Reference>;
template class Resource<Signature>;


template class BundleBase<Bundle>;
template class BundleBase<KbvBundle>;
template class BundleBase<MedicationDispenseBundle>;

}
