#include "erp/model/EvdgaBundle.hxx"
#include "shared/model/ResourceFactory.hxx"
#include "shared/util/Configuration.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test/util/StaticData.hxx"

#include <gtest/gtest.h>

TEST(EvdgaBundleTest, validated)
{
    const auto& config = Configuration::instance();
    auto prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4711);
    auto evdgaBundleXml = ResourceTemplates::evdgaBundleXml({
        .prescriptionId = prescriptionId.toString(),
    });
    using EvdgaBundleFactory = model::ResourceFactory<model::EvdgaBundle>;
    EvdgaBundleFactory::Options valOpt{
        .validatorOptions = config.defaultValidatorOptions(model::ProfileType::KBV_PR_EVDGA_Bundle),
    };

    std::optional<EvdgaBundleFactory> evdgaBundleFactory;
    ASSERT_NO_THROW(evdgaBundleFactory.emplace(EvdgaBundleFactory::fromXml(evdgaBundleXml, *StaticData::getXmlValidator(),valOpt)));
    std::optional<model::EvdgaBundle> evdgaBundle;
    EXPECT_NO_THROW(evdgaBundle.emplace(model::EvdgaBundle::fromXml(evdgaBundleXml, *StaticData::getXmlValidator())));
}
