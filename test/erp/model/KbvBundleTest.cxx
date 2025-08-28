/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/ErpRequirements.hxx"
#include "shared/model/KbvBundle.hxx"
#include "shared/model/ResourceNames.hxx"
#include "test/util/ErpMacros.hxx"
#include "test/util/ResourceTemplates.hxx"

#include <gtest/gtest.h>


TEST(KbvBundleTest, PatientIdentifierSystem)
{
    A_23936.test("A_23936 / ANFERP-3322 Anpassung Regel für kbv 1.3.0 Profile");
    using namespace std::string_literals;
    auto kbvBundle = model::KbvBundle::fromXmlNoValidation(ResourceTemplates::kbvBundleXml({
        .kbvVersion = ResourceTemplates::Versions::KBV_ERP_1_3_2,
        .forceKvid10Ns = model::resource::naming_system::pkvKvid10,
    }));
    auto message = "Patient.identifier.system must be "s.append(model::resource::naming_system::gkvKvid10);
    EXPECT_ERP_EXCEPTION_WITH_MESSAGE(kbvBundle.additionalValidation(), HttpStatus::BadRequest, message.c_str());
}
