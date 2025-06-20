/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/model/MedicationDispenseId.hxx"
#include "shared/model/ResourceNames.hxx"
#include "test/workflow-test/ErpWorkflowTestFixture.hxx"

#include <ostream>

struct Params {
    model::PrescriptionType mPrescriptionType;
    size_t numMedicationDispenses;
    friend std::ostream& operator<<(std::ostream& os, const Params& params)
    {
        os << "Type: " << (int) params.mPrescriptionType
           << " numMedicationDispenses: " << params.numMedicationDispenses;
        return os;
    }
};


class MultipleMedicationDispensesTestP : public ErpWorkflowTestTemplate<::testing::TestWithParam<Params>>
{
public:
protected:
    void SetUp() override
    {
        ErpWorkflowTestTemplate<::testing::TestWithParam<Params>>::SetUp();
    }

public:
    model::PrescriptionId createClosedTask(const std::string& kvnr)
    {
        std::optional<model::PrescriptionId> prescriptionId;
        createClosedTaskInternal(prescriptionId, kvnr);
        return *prescriptionId;
    }
    void createClosedTaskInternal(std::optional<model::PrescriptionId>& prescriptionId, const std::string& kvnr)//NOLINT(readability-function-cognitive-complexity)
    {
        std::string accessCode;
        ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId, accessCode, GetParam().mPrescriptionType));
        ASSERT_TRUE(prescriptionId.has_value());

        std::string qesBundle;
        std::vector<model::Communication> communications;
        ASSERT_NO_FATAL_FAILURE(checkTaskActivate(qesBundle, communications, *prescriptionId, kvnr, accessCode));

        std::string secret;
        std::optional<model::Timestamp> lastModified;
        ASSERT_NO_FATAL_FAILURE(checkTaskAccept(secret, lastModified, *prescriptionId, kvnr, accessCode, qesBundle));
        ASSERT_TRUE(lastModified.has_value() || !runsInErpTest());


        ASSERT_NO_FATAL_FAILURE(checkTaskClose(*prescriptionId, kvnr, secret, lastModified, communications,
                                               GetParam().numMedicationDispenses));
    }
};

TEST_P(MultipleMedicationDispensesTestP, MultipleMedicationsOneTaskTest)//NOLINT(readability-function-cognitive-complexity)
{
    model::Timestamp startTime = model::Timestamp::now();
    auto kvnr = generateNewRandomKVNR().id();
    auto task1 = createClosedTask(kvnr);


    const auto telematicIdDoctor = jwtArzt().stringForClaim(JWT::idNumberClaim).value();
    const auto telematicIdPharmacy = jwtApotheke().stringForClaim(JWT::idNumberClaim).value();
    std::vector<std::string> actorIdentifiers = {
        telematicIdDoctor,   kvnr, telematicIdPharmacy, telematicIdPharmacy, telematicIdPharmacy,
        telematicIdPharmacy, kvnr};
    std::vector<model::AuditEvent::SubType> expectedActions = {
        model::AuditEvent::SubType::update, model::AuditEvent::SubType::read,   model::AuditEvent::SubType::update,
        model::AuditEvent::SubType::read,   model::AuditEvent::SubType::update, model::AuditEvent::SubType::read,
        model::AuditEvent::SubType::read};
    std::vector<std::optional<model::PrescriptionId>> prescriptionIds = {
        task1, task1, task1, task1, task1, task1, task1
    };

    // GET MedicationDispense/
    {
        auto meds = medicationDispenseGetAll({}, JwtBuilder::testBuilder().makeJwtVersicherter(kvnr));
        ASSERT_TRUE(meds.has_value());
        ASSERT_EQ(meds->getResourcesByType<model::MedicationDispense>().size(), GetParam().numMedicationDispenses);
        const auto& medBundle = meds->getResourcesByType<model::MedicationDispense>();
        ASSERT_EQ(medBundle.size(), GetParam().numMedicationDispenses);
        for (const auto& med : medBundle)
        {
            ASSERT_EQ(med.prescriptionId(), task1);
            ASSERT_EQ(med.kvnr(), kvnr);
        }
        actorIdentifiers.push_back(kvnr);
        expectedActions.push_back(model::AuditEvent::SubType::read);
        prescriptionIds.emplace_back(std::nullopt);
    }

    // GET MedicationDispense/?identifier=https://gematik.de/fhir/erp/NamingSystem/GEM_ERP_NS_PrescriptionId|<PrescriptionID>
    {
        auto meds = medicationDispenseGetAll(
            "identifier=" + std::string{model::resource::naming_system::prescriptionID} + "|" + task1.toString(),
            JwtBuilder::testBuilder().makeJwtVersicherter(kvnr));
        ASSERT_TRUE(meds.has_value());
        ASSERT_EQ(meds->getResourcesByType<model::MedicationDispense>().size(), GetParam().numMedicationDispenses);
        const auto& medBundle = meds->getResourcesByType<model::MedicationDispense>();
        ASSERT_EQ(medBundle.size(), GetParam().numMedicationDispenses);
        for (const auto& med : medBundle)
        {
            ASSERT_EQ(med.prescriptionId(), task1);
            ASSERT_EQ(med.kvnr(), kvnr);
        }
        actorIdentifiers.push_back(kvnr);
        expectedActions.push_back(model::AuditEvent::SubType::read);
        prescriptionIds.emplace_back(task1);
    }

    checkAuditEvents(prescriptionIds, kvnr, "de", startTime, actorIdentifiers, {0, 2, 3, 4, 5}, expectedActions);
}

TEST_P(MultipleMedicationDispensesTestP, MultipleMedicationsMultipleTaskTest)//NOLINT(readability-function-cognitive-complexity)
{
    (void) Fhir::instance();
    auto id = [](const model::PrescriptionId& prescriptionID) {
        return std::string{model::resource::naming_system::prescriptionID}.append("|").append(
            prescriptionID.toString());
    };
    auto kvnr = generateNewRandomKVNR().id();
    auto task1 = createClosedTask(kvnr);
    auto task2 = createClosedTask(kvnr);
    auto task3 = createClosedTask(kvnr);

    // GET MedicationDispense/
    {
        auto meds = medicationDispenseGetAll({}, JwtBuilder::testBuilder().makeJwtVersicherter(kvnr));
        ASSERT_TRUE(meds.has_value());
        ASSERT_EQ(meds->getResourcesByType<model::MedicationDispense>().size(), GetParam().numMedicationDispenses * 3);
        const auto& medBundle = meds->getResourcesByType<model::MedicationDispense>();
        ASSERT_EQ(medBundle.size(), GetParam().numMedicationDispenses * 3);
        for (const auto& med : medBundle)
        {
            ASSERT_TRUE(med.prescriptionId() == task1 || med.prescriptionId() == task2 ||
                        med.prescriptionId() == task3);
            ASSERT_EQ(med.kvnr(), kvnr);
        }
    }

    {
        auto meds =
            medicationDispenseGetAll("identifier=" + id(task1), JwtBuilder::testBuilder().makeJwtVersicherter(kvnr));
        ASSERT_TRUE(meds.has_value());
        ASSERT_EQ(meds->getResourcesByType<model::MedicationDispense>().size(), GetParam().numMedicationDispenses);
        const auto& medBundle = meds->getResourcesByType<model::MedicationDispense>();
        ASSERT_EQ(medBundle.size(), GetParam().numMedicationDispenses);
        for (const auto& med : medBundle)
        {
            ASSERT_EQ(med.prescriptionId(), task1);
            ASSERT_EQ(med.kvnr(), kvnr);
        }
    }
    {
        auto meds =
            medicationDispenseGetAll("identifier=" + id(task2), JwtBuilder::testBuilder().makeJwtVersicherter(kvnr));
        ASSERT_TRUE(meds.has_value());
        ASSERT_EQ(meds->getResourcesByType<model::MedicationDispense>().size(), GetParam().numMedicationDispenses);
        const auto& medBundle = meds->getResourcesByType<model::MedicationDispense>();
        ASSERT_EQ(medBundle.size(), GetParam().numMedicationDispenses);
        for (const auto& med : medBundle)
        {
            ASSERT_EQ(med.prescriptionId(), task2);
            ASSERT_EQ(med.kvnr(), kvnr);
        }
    }
    {
        auto meds =
            medicationDispenseGetAll("identifier=" + id(task3), JwtBuilder::testBuilder().makeJwtVersicherter(kvnr));
        ASSERT_TRUE(meds.has_value());
        ASSERT_EQ(meds->getResourcesByType<model::MedicationDispense>().size(), GetParam().numMedicationDispenses);
        const auto& medBundle = meds->getResourcesByType<model::MedicationDispense>();
        ASSERT_EQ(medBundle.size(), GetParam().numMedicationDispenses);
        for (const auto& med : medBundle)
        {
            ASSERT_EQ(med.prescriptionId(), task3);
            ASSERT_EQ(med.kvnr(), kvnr);
        }
    }
    {
        const auto searchArgument = "identifier=" + id(task1) + "," + id(task3);
        auto meds = medicationDispenseGetAll(searchArgument, JwtBuilder::testBuilder().makeJwtVersicherter(kvnr));
        ASSERT_TRUE(meds.has_value());
        ASSERT_EQ(meds->getResourcesByType<model::MedicationDispense>().size(), GetParam().numMedicationDispenses * 2);
        const auto& medBundle = meds->getResourcesByType<model::MedicationDispense>();
        ASSERT_EQ(medBundle.size(), GetParam().numMedicationDispenses * 2);
        for (const auto& med : medBundle)
        {
            ASSERT_TRUE(med.prescriptionId() == task3 || med.prescriptionId() == task1);
            ASSERT_EQ(med.kvnr(), kvnr);
        }
    }
    {
        const auto searchArgument = "identifier=" + id(task1) + ',' + id(task2) + ',' + id(task3);
        auto meds = medicationDispenseGetAll(searchArgument, JwtBuilder::testBuilder().makeJwtVersicherter(kvnr));
        ASSERT_TRUE(meds.has_value());
        ASSERT_EQ(meds->getResourcesByType<model::MedicationDispense>().size(), GetParam().numMedicationDispenses * 3);
        const auto& medBundle = meds->getResourcesByType<model::MedicationDispense>();
        ASSERT_EQ(medBundle.size(), GetParam().numMedicationDispenses * 3);
        for (const auto& med : medBundle)
        {
            ASSERT_TRUE(med.prescriptionId() == task3 || med.prescriptionId() == task1 || med.prescriptionId() == task2);
            ASSERT_EQ(med.kvnr(), kvnr);
        }
    }
}


INSTANTIATE_TEST_SUITE_P(MultipleMedicationDispensesTestPInst, MultipleMedicationDispensesTestP,
                         testing::Values(Params{model::PrescriptionType::apothekenpflichigeArzneimittel, 1},
                                         Params{model::PrescriptionType::apothekenpflichigeArzneimittel, 7},
                                         Params{model::PrescriptionType::digitaleGesundheitsanwendungen, 2},
                                         Params{model::PrescriptionType::digitaleGesundheitsanwendungen, 3},
                                         Params{model::PrescriptionType::direkteZuweisung, 2},
                                         Params{model::PrescriptionType::direkteZuweisung, 4},
                                         Params{model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 5},
                                         Params{model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 6},
                                         Params{model::PrescriptionType::direkteZuweisungPkv, 3},
                                         Params{model::PrescriptionType::direkteZuweisungPkv, 8}));
