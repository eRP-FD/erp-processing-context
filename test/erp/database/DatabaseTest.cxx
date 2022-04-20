/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/model/Bundle.hxx"
#include "erp/model/ChargeItem.hxx"
#include "test/erp/database/DatabaseTestFixture.hxx"
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

    void createChargeItem(std::string_view insurant, std::string_view pharmacy,
                          const model::Timestamp& enteredDate = model::Timestamp::now())
    {
        ResourceManager& resourceManager = ResourceManager::instance();
        const auto& chargeItemXML = resourceManager.getStringResource("test/EndpointHandlerTest/charge_item_input.xml");
        auto chargeItem = model::ChargeItem::fromXmlNoValidation(chargeItemXML);
        const auto& dispenseItemXML = resourceManager.getStringResource("test/EndpointHandlerTest/dispense_item.xml");
        auto dispenseItem = model::Bundle::fromXmlNoValidation(dispenseItemXML);

        chargeItem.setSubjectKvnr(insurant);
        chargeItem.setEntererTelematikId(pharmacy);
        chargeItem.setEnteredDate(enteredDate);
        auto&& db = database();
        model::Task task(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, "09409348029834029384023984209");
        task.setKvnr(insurant);
        std::optional<model::PrescriptionId> id;
        ASSERT_NO_THROW(id.emplace(db.storeTask(task)));
        task.setPrescriptionId(*id);
        task.setExpiryDate(model::Timestamp::now());
        task.setAcceptDate(model::Timestamp::now());
        ASSERT_NO_THROW(db.activateTask(task, model::Binary("","")));
        chargeItem.setId(*id);
        ASSERT_NO_THROW(db.storeChargeInformation(pharmacy, chargeItem, dispenseItem));
        db.commitTransaction();
    }
    UrlArguments urlArguments(const std::vector<std::pair<std::string, std::string>>& arguments)
    {
        UrlArguments urlArgs{{
            {"entered-date", "charge_item_entered_date", SearchParameter::Type::Date},
            {"patient", "kvnr_hashed", SearchParameter::Type::HashedIdentity},
            {"enterer", "charge_item_enterer", SearchParameter::Type::HashedIdentity},
        }};
        urlArgs.parse(arguments, keyDerivation());
        return urlArgs;
    }
};

TEST_F(DatabaseTest, chargeItem_basic)
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
            std::optional<std::tuple<model::ChargeItem, model::Bundle>> chargeInformation;
            ASSERT_NO_THROW(chargeInformation.emplace(db.retrieveChargeInformation(item.id())));
            ASSERT_EQ(std::get<model::ChargeItem>(*chargeInformation).id(), item.id());
        }
        db.commitTransaction();
    }
    // check retrieval of items for the pharmacies
    for (const auto& pharmacy : pharmacyIds)
    {
        auto&& db = database();
        std::vector<model::ChargeItem> chargeItems;
        ASSERT_NO_THROW(chargeItems = db.retrieveAllChargeItemsForPharmacy(pharmacy, std::nullopt));
        db.commitTransaction();
        ASSERT_EQ(chargeItems.size(), insurantIds.size());
        for (const auto& item: chargeItems)
        {
            EXPECT_EQ(item.entererTelematikId(), pharmacy);
            EXPECT_NE(std::find(insurantIds.begin(), insurantIds.end(), item.subjectKvnr()), insurantIds.end());
        }
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
    // also check that the pharmacies have the charge-information removed
    for (const auto& pharmacy : pharmacyIds)
    {
        auto&& db = database();
        std::vector<model::ChargeItem> chargeItems;
        ASSERT_NO_THROW(chargeItems = db.retrieveAllChargeItemsForPharmacy(pharmacy, std::nullopt));
        db.commitTransaction();
        ASSERT_EQ(chargeItems.size(), insurantIds.size() - 1);
        for (const auto& item: chargeItems)
        {
            EXPECT_EQ(item.entererTelematikId(), pharmacy);
            EXPECT_NE(std::find(insurantIds.begin() + 1, insurantIds.end(), item.subjectKvnr()), insurantIds.end());
        }
    }
    std::optional<model::PrescriptionId> removedId;
    {
        auto&& db = database();
        std::vector<model::ChargeItem> chargeItems;
        ASSERT_NO_THROW(chargeItems = db.retrieveAllChargeItemsForInsurant(*rbegin(insurantIds), std::nullopt));
        ASSERT_GT(chargeItems.size(), 1);
        removedId.emplace(chargeItems[0].id());
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

TEST_F(DatabaseTest, chargeItem_search_by_pharmacy)
{
    using namespace std::string_literals;
    const auto& pharmacy0 = data(pharmacyIds)[0];
    const auto& insurant0 = data(insurantIds)[0];
    for (const auto& pharmacy: pharmacyIds)
    {
        int64_t stamp = 0;
        for (const auto& insurant : insurantIds)
        {
            createChargeItem(insurant, pharmacy, model::Timestamp{stamp});
            ++stamp;
        }
    }
    // search equal timestamp
    for (int64_t stamp = 0; size_t(stamp) < insurantIds.size(); ++stamp)
    {
        auto&& db = database();
        std::vector<model::ChargeItem> chargeItems;
        ASSERT_NO_THROW(chargeItems = db.retrieveAllChargeItemsForPharmacy(pharmacy0, urlArguments(
            {{"entered-date"s, "eq"s + model::Timestamp{stamp}.toXsDateTimeWithoutFractionalSeconds()}})));
        EXPECT_EQ(chargeItems.size(), 1);
        for (const auto& item: chargeItems)
        {
            EXPECT_EQ(item.entererTelematikId(), pharmacy0);
            EXPECT_EQ(item.enteredDate(), model::Timestamp{stamp});
        }
        db.commitTransaction();
    }
    // search relative timestamp
    for (int64_t stamp = 0; size_t(stamp) < insurantIds.size(); ++stamp)
    {
        auto&& db = database();
        std::vector<model::ChargeItem> chargeItems;
        ASSERT_NO_THROW(chargeItems = db.retrieveAllChargeItemsForPharmacy(pharmacy0, urlArguments(
            {{"entered-date"s, "lt"s + model::Timestamp{stamp}.toXsDateTimeWithoutFractionalSeconds()}})));
        EXPECT_EQ(chargeItems.size(), stamp);
        for (const auto& item: chargeItems)
        {
            EXPECT_EQ(item.entererTelematikId(), pharmacy0);
            EXPECT_LT(item.enteredDate(), model::Timestamp{stamp});
        }
        db.commitTransaction();
    }
    // search for insurant
    for (const auto& insurant: insurantIds)
    {
        auto&& db = database();
        std::vector<model::ChargeItem> chargeItems;
        ASSERT_NO_THROW(chargeItems = db.retrieveAllChargeItemsForPharmacy(pharmacy0, urlArguments(
            {{"patient"s, std::string{insurant}}})));
        EXPECT_EQ(chargeItems.size(), 1);
        for (const auto& item: chargeItems)
        {
            EXPECT_EQ(item.entererTelematikId(), pharmacy0);
            EXPECT_EQ(item.subjectKvnr(), insurant);
        }
        db.commitTransaction();
    }
    // add some more for insurant0/pharmacy0
    for (int64_t stamp = 1; stamp < 10; ++stamp)
    {
        createChargeItem(insurant0, pharmacy0, model::Timestamp{stamp});
    }
    // dual search parameter: insurant + date
    for (int64_t stamp = 0; stamp < 10; ++stamp)
    {
        auto&& db = database();
        std::vector<model::ChargeItem> chargeItems;
        ASSERT_NO_THROW(chargeItems = db.retrieveAllChargeItemsForPharmacy(pharmacy0, urlArguments(
            {
             {"patient"s, std::string{insurant0}},
             {"entered-date"s, "ge"s + model::Timestamp{stamp}.toXsDateTimeWithoutFractionalSeconds()}
            })));
        EXPECT_EQ(chargeItems.size(), 10 - stamp);
        for (const auto& item: chargeItems)
        {
            EXPECT_EQ(item.entererTelematikId(), pharmacy0);
            EXPECT_EQ(item.subjectKvnr(), insurant0);
            EXPECT_GE(item.enteredDate(), model::Timestamp{stamp});
        }
        db.commitTransaction();
    }
}

TEST_F(DatabaseTest, chargeItem_search_by_insurant)
{
    using namespace std::string_literals;
    const auto& pharmacy0 = data(pharmacyIds)[0];
    const auto& insurant0 = data(insurantIds)[0];
    for (const auto& insurant : insurantIds)
    {
        int64_t stamp = 0;
        for (const auto& pharmacy: pharmacyIds)
        {
            createChargeItem(insurant, pharmacy, model::Timestamp{stamp});
            ++stamp;
        }
    }
    // search equal timestamp
    for (int64_t stamp = 0; size_t(stamp) < pharmacyIds.size(); ++stamp)
    {
        auto&& db = database();
        std::vector<model::ChargeItem> chargeItems;
        ASSERT_NO_THROW(chargeItems = db.retrieveAllChargeItemsForInsurant(insurant0, urlArguments(
            {{"entered-date"s, "eq"s + model::Timestamp{stamp}.toXsDateTimeWithoutFractionalSeconds()}})));
        EXPECT_EQ(chargeItems.size(), 1);
        for (const auto& item: chargeItems)
        {
            EXPECT_EQ(item.subjectKvnr(), insurant0);
            EXPECT_EQ(item.enteredDate(), model::Timestamp{stamp});
        }
        db.commitTransaction();
    }
    // search relative timestamp
    for (int64_t stamp = 0; size_t(stamp) < pharmacyIds.size(); ++stamp)
    {
        auto&& db = database();
        std::vector<model::ChargeItem> chargeItems;
        ASSERT_NO_THROW(chargeItems = db.retrieveAllChargeItemsForInsurant(insurant0, urlArguments(
            {{"entered-date"s, "lt"s + model::Timestamp{stamp}.toXsDateTimeWithoutFractionalSeconds()}})));
        EXPECT_EQ(chargeItems.size(), stamp);
        for (const auto& item: chargeItems)
        {
            EXPECT_EQ(item.subjectKvnr(), insurant0);
            EXPECT_LT(item.enteredDate(), model::Timestamp{stamp});
        }
        db.commitTransaction();
    }
    // search for enterer
    for (const auto& pharmacy : pharmacyIds)
    {
        auto&& db = database();
        std::vector<model::ChargeItem> chargeItems;
        ASSERT_NO_THROW(chargeItems = db.retrieveAllChargeItemsForInsurant(insurant0, urlArguments(
            {{"enterer"s, std::string{pharmacy}}})));
        EXPECT_EQ(chargeItems.size(), 1);
        for (const auto& item: chargeItems)
        {
            EXPECT_EQ(item.entererTelematikId(), pharmacy);
            EXPECT_EQ(item.subjectKvnr(), insurant0);
        }
        db.commitTransaction();
    }
    // add some more for insurant0/pharmacy0
    for (int64_t stamp = 1; stamp < 10; ++stamp)
    {
        createChargeItem(insurant0, pharmacy0, model::Timestamp{stamp});
    }
    // dual search parameter: enterer + date
    for (int64_t stamp = 0; stamp < 10; ++stamp)
    {
        auto&& db = database();
        std::vector<model::ChargeItem> chargeItems;
        ASSERT_NO_THROW(chargeItems = db.retrieveAllChargeItemsForInsurant(insurant0, urlArguments(
            {
             {"enterer"s, std::string{pharmacy0}},
             {"entered-date"s, "ge"s + model::Timestamp{stamp}.toXsDateTimeWithoutFractionalSeconds()}
            })));
        EXPECT_EQ(chargeItems.size(), 10 - stamp);
        for (const auto& item: chargeItems)
        {
            EXPECT_EQ(item.entererTelematikId(), pharmacy0);
            EXPECT_EQ(item.subjectKvnr(), insurant0);
            EXPECT_GE(item.enteredDate(), model::Timestamp{stamp});
        }
        db.commitTransaction();
    }
}
