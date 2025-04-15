// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#ifndef ERP_PROCESSING_CONTEXT_SRC_EXPORTER_MODEL_EPAMEDICATIONPZNINGREDIENT_HXX
#define ERP_PROCESSING_CONTEXT_SRC_EXPORTER_MODEL_EPAMEDICATIONPZNINGREDIENT_HXX

#include "shared/model/MedicationBase.hxx"

namespace model {
class Pzn;

class EPAMedicationPZNIngredient : public MedicationBase<EPAMedicationPZNIngredient>{
public:
  static constexpr auto profileType = ProfileType::EPAMedicationPZNIngredient;

  explicit EPAMedicationPZNIngredient(const Pzn& pzn, std::string_view display);

private:
  using MedicationBase::MedicationBase;
  friend class Resource<EPAMedicationPZNIngredient>;
};

extern template class Resource<EPAMedicationPZNIngredient>;
extern template class MedicationBase<EPAMedicationPZNIngredient>;

}

#endif
