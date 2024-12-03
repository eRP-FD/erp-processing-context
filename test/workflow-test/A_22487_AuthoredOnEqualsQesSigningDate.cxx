/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "test/util/ResourceTemplates.hxx"
#include "test/workflow-test/ErpWorkflowTestFixture.hxx"
#include "shared/model/Timestamp.hxx"

#include <date/tz.h>
#include <boost/algorithm/string/replace.hpp>

using namespace std::literals::chrono_literals;

TEST_F(ErpWorkflowTest, AuthoredOnEqualsQesDate)
{
    std::optional<model::PrescriptionId> prescriptionId;
    std::string accessCode;
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId, accessCode));
    ASSERT_TRUE(prescriptionId.has_value());
    ASSERT_FALSE(accessCode.empty());

    using zoned_ms = date::zoned_time<std::chrono::milliseconds>;
    auto yesterdayTime = model::Timestamp::now() - 24h;
    auto yesterday = zoned_ms{model::Timestamp::GermanTimezone, yesterdayTime.localDay()};
    auto signedTime = model::Timestamp(yesterday.get_sys_time()) + 23h + 59min + 59s;
    auto authoredOn = model::Timestamp(yesterday.get_sys_time());

    auto bundle = kbvBundleXml({.prescriptionId = prescriptionId.value(), .authoredOn = authoredOn});
    taskActivateWithOutcomeValidation(prescriptionId.value(), accessCode, toCadesBesSignature(bundle, signedTime), HttpStatus::OK);
}

TEST_F(ErpWorkflowTest, AuthoredOnNotEqualsQesDate)
{
    std::optional<model::PrescriptionId> prescriptionId;
    std::string accessCode;
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId, accessCode));
    ASSERT_TRUE(prescriptionId.has_value());
    ASSERT_FALSE(accessCode.empty());

    using zoned_ms = date::zoned_time<std::chrono::milliseconds>;
    auto yesterdayTime = model::Timestamp::now() - 24h;
    auto yesterday = zoned_ms{model::Timestamp::GermanTimezone, yesterdayTime.localDay()};

    auto authoredOn = model::Timestamp(yesterday.get_sys_time());
    auto bundle = kbvBundleXml({.prescriptionId = prescriptionId.value(), .authoredOn = authoredOn});
    {
        auto signedTimeFuture = model::Timestamp::now();
        taskActivateWithOutcomeValidation(
            prescriptionId.value(), accessCode, toCadesBesSignature(bundle, signedTimeFuture), HttpStatus::BadRequest,
            model::OperationOutcome::Issue::Type::invalid,
            "Ausstellungsdatum und Signaturzeitpunkt weichen voneinander ab, müssen aber taggleich sein");
    }

    {
        auto signedTimePast = model::Timestamp(yesterday.get_sys_time()) - 1s;
        taskActivateWithOutcomeValidation(
            prescriptionId.value(), accessCode, toCadesBesSignature(bundle, signedTimePast), HttpStatus::BadRequest,
            model::OperationOutcome::Issue::Type::invalid,
            "Ausstellungsdatum und Signaturzeitpunkt weichen voneinander ab, müssen aber taggleich sein");
    }
}
