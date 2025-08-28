/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/model/MedicationDispenseOperationParameters.hxx"
#include "test/util/ResourceManager.hxx"
#include "test/util/StaticData.hxx"
#include "test/util/TestUtils.hxx"

#include <gtest/gtest.h>

TEST(ERP_30139_dataAbsentUri, run)
{
    testutils::ShiftFhirResourceViewsGuard guard{"GEM_WF_1_5_0", fhirtools::Date("2025-08-19")};
    auto& resourceManager = ResourceManager::instance();
    const auto& input = resourceManager.getStringResource("test/issues/ERP-30139-data-absent-uri/input.xml");
    ASSERT_NO_THROW(
        (void) model::MedicationDispenseOperationParameters::fromXml(input, *StaticData::getXmlValidator()));
}
