/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/model/Bundle.hxx"
#include "erp/model/ChargeItem.hxx"
#include "test/erp/database/DatabaseTestFixture.hxx"
#include "test/util/CryptoHelper.hxx"
#include "test/util/ResourceManager.hxx"

using namespace std::string_view_literals;

class DatabaseTest : public DatabaseTestFixture
{
public:
    static constexpr auto pharmacyIds = {"X-0815-4711"sv, "X-0815-4712"sv, "X-0815-4713"sv};
    static constexpr auto insurantIds = {"X123456789"sv, "X123456790"sv, "X123456791"sv};


    void SetUp() override
    {
        DatabaseTestFixture::SetUp();
        auto&& db = database();
        for (const auto& insurant: insurantIds)
        {
            db.clearAllChargeInformation(insurant);
        }
        db.commitTransaction();
    }

    //NOLINTNEXTLINE(readability-function-cognitive-complexity)
    void createChargeItem(std::string_view insurant, std::string_view pharmacy,
                          const fhirtools::Timestamp& enteredDate = fhirtools::Timestamp::now())
    {
        ResourceManager& resourceManager = ResourceManager::instance();

        auto&& db = database();
        model::Task task(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, "09409348029834029384023984209");
        task.setKvnr(insurant);
        std::optional<model::PrescriptionId> id;
        ASSERT_NO_THROW(id.emplace(db.storeTask(task)));
        task.setPrescriptionId(*id);
        task.setExpiryDate(fhirtools::Timestamp::now());
        task.setAcceptDate(fhirtools::Timestamp::now());

        std::string chargeItemXML = resourceManager.getStringResource("test/EndpointHandlerTest/charge_item_input.xml");
        chargeItemXML = String::replaceAll(chargeItemXML, "72bd741c-7ad8-41d8-97c3-9aabbdd0f5b4",
                                           "fe4a04af-0828-4977-a5ce-bfeed16ebf10");
        auto chargeItem = model::ChargeItem::fromXmlNoValidation(chargeItemXML);
        chargeItem.setId(*id);
        chargeItem.setPrescriptionId(*id);
        chargeItem.setSubjectKvnr(insurant);
        chargeItem.setEntererTelematikId(pharmacy);
        chargeItem.setEnteredDate(enteredDate);
        chargeItem.setAccessCode(::MockDatabase::mockAccessCode);
        chargeItem.deleteContainedBinary();

        const auto& dispenseItemXML = resourceManager.getStringResource("test/EndpointHandlerTest/dispense_item.xml");
        auto dispenseItem = model::Bundle::fromXmlNoValidation(dispenseItemXML);
        auto signedDispenseItem =
            ::model::Binary{dispenseItem.getIdentifier().toString(),
                            ::CryptoHelper::toCadesBesSignature(dispenseItem.serializeToJsonString())};

        auto prescriptionXML =
            resourceManager.getStringResource("test/EndpointHandlerTest/kbv_pkv_bundle_template.xml");
        prescriptionXML = String::replaceAll(prescriptionXML, "##PRESCRIPTION_ID##", id->toString());
        prescriptionXML = String::replaceAll(prescriptionXML, "##KVNR##", ::std::string{insurant});
        auto prescription = ::model::Bundle::fromXmlNoValidation(prescriptionXML);
        auto signedPrescription = ::model::Binary{
            prescription.getId().toString(), ::CryptoHelper::toCadesBesSignature(prescription.serializeToJsonString())};

        auto receiptJson = resourceManager.getStringResource("test/EndpointHandlerTest/receipt_template.json");
        receiptJson = String::replaceAll(receiptJson, "##PRESCRIPTION_ID##", id->toString());
        auto receipt = ::model::Bundle::fromJsonNoValidation(receiptJson);

        ASSERT_NO_THROW(db.activateTask(task, signedPrescription));
        ASSERT_NO_THROW(db.storeChargeInformation(::model::ChargeInformation{
            ::std::move(chargeItem), ::std::move(signedPrescription), ::std::move(prescription),
            ::std::move(signedDispenseItem), ::std::move(dispenseItem), ::std::move(receipt)}));

        db.commitTransaction();
    }
    UrlArguments urlArguments(const std::vector<std::pair<std::string, std::string>>& arguments)
    {
        UrlArguments urlArgs{{
            {"entered-date", "entered_date", SearchParameter::Type::Date},
            {"patient", "kvnr_hashed", SearchParameter::Type::HashedIdentity},
        }};
        urlArgs.parse(arguments, keyDerivation());
        return urlArgs;
    }
};

TEST_F(DatabaseTest, chargeItem_basic)//NOLINT(readability-function-cognitive-complexity)
{
    // create tasks with chargeitems:
    for (const auto& insurant : insurantIds)
    {
        for (const auto& pharmacy: pharmacyIds)
        {
            ASSERT_NO_FATAL_FAILURE(createChargeItem(insurant, pharmacy));
        }
    }
    // check retrieval of items for the insuratns
    for (const auto& insurant : insurantIds)
    {
        auto&& db = database();
        std::vector<model::ChargeItem> chargeItems;
        ASSERT_NO_THROW(chargeItems = db.retrieveAllChargeItemsForInsurant(insurant, std::nullopt));
        ASSERT_EQ(chargeItems.size(), pharmacyIds.size());
        for (const auto& item: chargeItems)
        {
            EXPECT_EQ(item.subjectKvnr(), insurant);
            EXPECT_NE(std::find(pharmacyIds.begin(), pharmacyIds.end(), item.entererTelematikId()), pharmacyIds.end());
            std::optional<::model::ChargeInformation> chargeInformation;
            ASSERT_NO_THROW(chargeInformation.emplace(db.retrieveChargeInformation(item.id().value())));
            ASSERT_EQ(chargeInformation->chargeItem.id(), item.id());
        }
        db.commitTransaction();
    }
    // clear charge information for insurant[0] as done when consent is revoked
    {
        auto&& db = database();
        db.clearAllChargeInformation(data(insurantIds)[0]);
        db.commitTransaction();
    }
    // check that there are no more ChargeInformation entries for the insurant[0]
    {
        auto&& db = database();
        std::vector<model::ChargeItem> chargeItems;
        ASSERT_NO_THROW(chargeItems = db.retrieveAllChargeItemsForInsurant(data(insurantIds)[0], std::nullopt));
        EXPECT_EQ(chargeItems.size(), 0);
        db.commitTransaction();
    }
    // check that ChargeInformation is untouched for the insurant[1]
    {
        auto&& db = database();
        std::vector<model::ChargeItem> chargeItems;
        ASSERT_NO_THROW(chargeItems = db.retrieveAllChargeItemsForInsurant(data(insurantIds)[1], std::nullopt));
        EXPECT_EQ(chargeItems.size(), pharmacyIds.size());
        db.commitTransaction();
    }
    std::optional<model::PrescriptionId> removedId;
    {
        auto&& db = database();
        std::vector<model::ChargeItem> chargeItems;
        ASSERT_NO_THROW(chargeItems = db.retrieveAllChargeItemsForInsurant(*rbegin(insurantIds), std::nullopt));
        ASSERT_GT(chargeItems.size(), 1);
        removedId.emplace(chargeItems[0].id().value());
        db.deleteChargeInformation(*removedId);
        db.commitTransaction();
    }
    {
        auto&& db = database();
        std::vector<model::ChargeItem> chargeItems;
        ASSERT_NO_THROW(chargeItems = db.retrieveAllChargeItemsForInsurant(*rbegin(insurantIds), std::nullopt));
        ASSERT_GE(chargeItems.size(), 1);
        db.commitTransaction();
        for (const auto& item : chargeItems)
        {
            ASSERT_NE(item.id(), *removedId);
        }
    }
}

TEST_F(DatabaseTest, chargeItem_search_by_insurant)//NOLINT(readability-function-cognitive-complexity)
{
    using namespace std::string_literals;
    const auto& insurant0 = data(insurantIds)[0];
    for (const auto& insurant : insurantIds)
    {
        int64_t stamp = 0;
        for (const auto& pharmacy: pharmacyIds)
        {
            createChargeItem(insurant, pharmacy, fhirtools::Timestamp{stamp});
            ++stamp;
        }
    }
    // search equal timestamp
    for (int64_t stamp = 0; size_t(stamp) < pharmacyIds.size(); ++stamp)
    {
        auto&& db = database();
        std::vector<model::ChargeItem> chargeItems;
        ASSERT_NO_THROW(chargeItems = db.retrieveAllChargeItemsForInsurant(insurant0, urlArguments(
            {{"entered-date"s, "eq"s + fhirtools::Timestamp{stamp}.toXsDateTimeWithoutFractionalSeconds()}})));
        EXPECT_EQ(chargeItems.size(), 1);
        for (const auto& item: chargeItems)
        {
            EXPECT_EQ(item.subjectKvnr(), insurant0);
            EXPECT_EQ(item.enteredDate(), fhirtools::Timestamp{stamp});
        }
        db.commitTransaction();
    }
    // search relative timestamp
    for (int64_t stamp = 0; size_t(stamp) < pharmacyIds.size(); ++stamp)
    {
        auto&& db = database();
        std::vector<model::ChargeItem> chargeItems;
        ASSERT_NO_THROW(chargeItems = db.retrieveAllChargeItemsForInsurant(insurant0, urlArguments(
            {{"entered-date"s, "lt"s + fhirtools::Timestamp{stamp}.toXsDateTimeWithoutFractionalSeconds()}})));
        EXPECT_EQ(chargeItems.size(), stamp);
        for (const auto& item: chargeItems)
        {
            EXPECT_EQ(item.subjectKvnr(), insurant0);
            EXPECT_LT(item.enteredDate(), fhirtools::Timestamp{stamp});
        }
        db.commitTransaction();
    }
}
