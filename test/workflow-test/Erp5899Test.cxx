/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "test/workflow-test/ErpWorkflowTestFixture.hxx"

#include "erp/util/Base64.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"
#include "test/util/JwtBuilder.hxx"
#include "test/util/ResourceManager.hxx"
#include "test/util/ResourceTemplates.hxx"

#include <fstream>

class Erp5899Test : public ErpWorkflowTest
{
public:

    JWT jwtFromResource(const std::string& fileName) const
    {
        rapidjson::Document claim;
        claim.CopyFrom(resourceManager.getJsonResource(testDataPath + fileName), claim.GetAllocator());
        return JwtBuilder::testBuilder().makeValidJwt(std::move(claim));
    }

    JWT jwtArzt() const override { return jwtFromResource("claims_arzt.json"); }
    JWT jwtApotheke() const override { return jwtFromResource("claims_apotheke.json"); }
    JWT jwtVersicherter() const override { return jwtFromResource("claims_versicherter.json"); }

    std::string qesBundle(const std::string& fileName)
    {
        return Base64::encode(resourceManager.getStringResource(testDataPath + fileName));
    }

    std::string medicationDispense(const std::string& kvnr,
                                   const std::string& prescriptionIdForMedicationDispense,
                                   const std::string& /*whenHandedOver*/,
                                   model::ResourceVersion::FhirProfileBundleVersion) override
    {
        return ResourceTemplates::medicationDispenseXml(
            {.prescriptionId = model::PrescriptionId::fromString(prescriptionIdForMedicationDispense),
             .kvnr = kvnr,
             .telematikId = "3-SMC-B-Testkarte-883110000129068",
             .whenHandedOver = now});
    }

    size_t countMedicationDispenses(const std::string_view& query)
    {
        size_t count{};
        countMedicationDispensesInternal(count, query);
        return count;
    }

    const model::Timestamp now = model::Timestamp::now();

private:
    //NOLINTNEXTLINE(readability-function-cognitive-complexity)
    void countMedicationDispensesInternal(size_t& count, const std::string_view& query)
    {
        std::optional<model::Bundle> medicationDispenseBundle;
        ASSERT_NO_FATAL_FAILURE(medicationDispenseBundle = medicationDispenseGetAll(query));
        ASSERT_TRUE(medicationDispenseBundle.has_value());
        std::vector<model::MedicationDispense> medicationDispenses;
        if (medicationDispenseBundle->getResourceCount() > 0)
        {
            ASSERT_NO_THROW(medicationDispenses =
                    medicationDispenseBundle->getResourcesByType<model::MedicationDispense>("MedicationDispense"));
        }
        count = medicationDispenses.size();
    }

    ResourceManager& resourceManager = ResourceManager::instance();
    const std::string testDataPath{"test/issues/ERP-5899/"};
};


TEST_F(Erp5899Test, run)//NOLINT(readability-function-cognitive-complexity)
{
    const auto nowStr = UrlHelper::escapeUrl(now.toXsDateTimeWithoutFractionalSeconds(model::Timestamp::GermanTimezone));
    const auto originalQuery =
        std::string{"whenhandedover=gt"} + nowStr + "&performer=3-SMC-B-Testkarte-883110000129068";
    const auto geQuery =
        std::string{"whenhandedover=ge"} + nowStr + "&performer=3-SMC-B-Testkarte-883110000129068";
    const auto beforeNowStr = UrlHelper::escapeUrl(
        (now - std::chrono::seconds(1)).toXsDateTimeWithoutFractionalSeconds(model::Timestamp::GermanTimezone));
    const auto oneSecondEarlierQuery =
        std::string{"whenhandedover=gt"} + beforeNowStr + "&performer=3-SMC-B-Testkarte-883110000129068";

    size_t originalCount = 0;
    ASSERT_NO_FATAL_FAILURE(originalCount = countMedicationDispenses(originalQuery));
    size_t geCount = 0;
    ASSERT_NO_FATAL_FAILURE(geCount = countMedicationDispenses(geQuery));
    size_t oneSecondEarlierCount = 0;
    ASSERT_NO_FATAL_FAILURE(oneSecondEarlierCount = countMedicationDispenses(oneSecondEarlierQuery));

    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate());
    ASSERT_TRUE(task.has_value());
    std::string accessCode{task->accessCode()};

    std::string qesBundle;
    taskActivateWithOutcomeValidation(task->prescriptionId(), accessCode,
                 std::get<0>(makeQESBundle("X110502414", task->prescriptionId(), model::Timestamp::now())));

    std::optional<model::Bundle> bundle;
    ASSERT_NO_FATAL_FAILURE(bundle = taskAccept(task->prescriptionId(), accessCode));
    ASSERT_TRUE(bundle.has_value());

    {
        auto tasks = bundle->getResourcesByType<model::Task>("Task");
        ASSERT_EQ(tasks.size(), 1);
        task = std::move(tasks[0]);
    }
    std::string secret;
    ASSERT_NO_THROW(secret = task->secret().value());
    std::optional<model::Kvnr> kvnr;
    ASSERT_NO_THROW(kvnr = task->kvnr().value());
    ASSERT_NO_FATAL_FAILURE(taskClose(task->prescriptionId(), secret, kvnr->id()));

    ASSERT_NO_FATAL_FAILURE(EXPECT_EQ(countMedicationDispenses(originalQuery), originalCount + 0));
    ASSERT_NO_FATAL_FAILURE(EXPECT_EQ(countMedicationDispenses(geQuery), geCount + 1));
    ASSERT_NO_FATAL_FAILURE(EXPECT_EQ(countMedicationDispenses(oneSecondEarlierQuery), oneSecondEarlierCount + 1));
}
