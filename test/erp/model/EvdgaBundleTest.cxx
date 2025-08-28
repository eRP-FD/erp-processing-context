#include "erp/model/EvdgaBundle.hxx"
#include "shared/fhir/Fhir.hxx"
#include "shared/model/ResourceFactory.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test/util/StaticData.hxx"

#include <gtest/gtest.h>

TEST(EvdgaBundleTest, validated)
{
    auto prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4711);
    auto evdgaBundleXml = ResourceTemplates::evdgaBundleXml({
        .prescriptionId = prescriptionId.toString(),
        .authoredOn = model::Timestamp::now().toGermanDate(),
    });
    using EvdgaBundleFactory = model::ResourceFactory<model::EvdgaBundle>;
    std::optional<EvdgaBundleFactory> evdgaBundleFactory;
    ASSERT_NO_THROW(
        evdgaBundleFactory.emplace(EvdgaBundleFactory::fromXml(evdgaBundleXml, *StaticData::getXmlValidator())));
    std::optional<model::EvdgaBundle> evdgaBundle;
    EXPECT_NO_THROW(evdgaBundle.emplace(model::EvdgaBundle::fromXml(evdgaBundleXml, *StaticData::getXmlValidator())));
}
