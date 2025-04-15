/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/KbvMedicationIngredient.hxx"
#include "fhirtools/model/erp/ErpElement.hxx"
#include "fhirtools/transformer/ResourceProfileTransformer.hxx"
#include "shared/fhir/Fhir.hxx"
#include "test/util/ResourceManager.hxx"
#include "test/util/ResourceTemplates.hxx"

#include <gtest/gtest.h>

class FhirTransformatorTest : public testing::Test
{
public:
    FhirTransformatorTest()
        : mRepo(getView())
    {
    }

    gsl::not_null<std::shared_ptr<const fhirtools::FhirStructureRepository>> mRepo;
private:
    static gsl::not_null<std::shared_ptr<const fhirtools::FhirStructureRepository>> getView()
    {
        const auto& fhirInstance =Fhir::instance();
        const auto& backend = fhirInstance.backend();
        const auto& viewList = fhirInstance.structureRepository(model::Timestamp::now());
        auto kbvVer = ResourceTemplates::Versions::KBV_ERP_current();
        return viewList.match(&backend, std::string{model::resource::structure_definition::kbv_medication_pzn}, kbvVer);
    }
};

TEST_F(FhirTransformatorTest, FailForDifferentBaseProfiles)
{
    try
    {
        fhirtools::ResourceProfileTransformer transformer(
            mRepo.get().get(),
            mRepo->findStructure({"https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Medication_PZN", std::nullopt}),
            mRepo->findStructure({"https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_PracticeSupply", std::nullopt}),
            {});
        FAIL();
    }
    catch (const std::runtime_error& rt)
    {
        EXPECT_EQ(std::string{rt.what()}, "source and target profile must derive from same base profile, but are "
                                          "actually derived from Medication and SupplyRequest");
    }
}
