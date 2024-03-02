/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/service/chargeitem/ChargeItemGetHandler.hxx"
#include "erp/ErpRequirements.hxx"
#include "erp/model/ChargeItem.hxx"
#include "erp/util/Base64.hxx"
#include "erp/util/ByteHelper.hxx"
#include "erp/util/search/UrlArguments.hxx"
#include "mock/tsl/MockTslManager.hxx"
#include "test/erp/model/CommunicationTest.hxx"
#include "test/mock/MockDatabase.hxx"
#include "test/util/CertificateDirLoader.h"
#include "test/util/CryptoHelper.hxx"
#include "test/util/ResourceManager.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test/util/ServerTestBase.hxx"
#include "test/workflow-test/ErpWorkflowTestFixture.hxx"

#include <chrono>


class ChargeItemGetHandlerTest : public ServerTestBase
{
public:
    ChargeItemGetHandlerTest() :
        ServerTestBase()
    {
    }

    void SetUp() override
    {
        if (model::ResourceVersion::deprecatedProfile(
            model::ResourceVersion::current<model::ResourceVersion::DeGematikErezeptWorkflowR4>()))
        {
            GTEST_SKIP();
        }
        ASSERT_NO_FATAL_FAILURE(ServerTestBase::SetUp());
    }
protected:

    void insertChargeItem(// NOLINT(readability-function-cognitive-complexity)
        const std::vector<std::tuple<std::string, std::string, std::optional<model::Timestamp>>>&
            patientsPharmaciesChargeItem,
        std::vector<std::string>& prescriptionIds, const std::string_view accessCode, const bool removeTask = false)
    {
        ResourceManager& resourceManager = ResourceManager::instance();

        for (const auto& patientAndPharmacy : patientsPharmaciesChargeItem)
        {
            const auto& [insurant, pharmacy, enteredDate] = patientAndPharmacy;

            auto db = createDatabase();
            model::Task task(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv,
                             "09409348029834029384023984209");
            task.setKvnr(model::Kvnr{insurant, model::Kvnr::Type::pkv});
            std::optional<model::PrescriptionId> prescriptionId;
            ASSERT_NO_THROW(prescriptionId.emplace(db->storeTask(task)));
            task.setPrescriptionId(*prescriptionId);
            task.setExpiryDate(model::Timestamp::now());
            task.setAcceptDate(model::Timestamp::now());

            std::string chargeItemXML =
                resourceManager.getStringResource("test/EndpointHandlerTest/charge_item_input.xml");
            chargeItemXML = String::replaceAll(chargeItemXML, "72bd741c-7ad8-41d8-97c3-9aabbdd0f5b4",
                                               "fe4a04af-0828-4977-a5ce-bfeed16ebf10");
            auto chargeItem = model::ChargeItem::fromXmlNoValidation(chargeItemXML);
            chargeItem.setId(*prescriptionId);
            chargeItem.setPrescriptionId(*prescriptionId);
            chargeItem.setSubjectKvnr(insurant);
            chargeItem.setEntererTelematikId(pharmacy);
            chargeItem.setEnteredDate(enteredDate.value_or(model::Timestamp::now()));
            chargeItem.setAccessCode(accessCode);
            chargeItem.deleteContainedBinary();

            const auto& dispenseItemXML = ResourceTemplates::medicationDispenseBundleXml({.medicationDispenses = {{}}});
            auto dispenseItem = model::AbgabedatenPkvBundle::fromXmlNoValidation(dispenseItemXML);
            auto signedDispenseItem =
                ::model::Binary{dispenseItem.getIdentifier().toString(),
                                ::CryptoHelper::toCadesBesSignature(dispenseItem.serializeToJsonString())};

            auto kbvVersion = model::ResourceVersion::current<model::ResourceVersion::KbvItaErp>();
            const std::string kbvVersionStr{v_str(kbvVersion)};

            auto prescriptionXML = ResourceTemplates::kbvBundlePkvXml({*prescriptionId, model::Kvnr(insurant)});
            auto prescription = ::model::Bundle::fromXmlNoValidation(prescriptionXML);
            auto signedPrescription =
                ::model::Binary{prescription.getId().toString(),
                                ::CryptoHelper::toCadesBesSignature(prescription.serializeToXmlString())};

            auto receiptJson = resourceManager.getStringResource("test/EndpointHandlerTest/receipt_template.json");
            receiptJson = ::String::replaceAll(receiptJson, "##PRESCRIPTION_ID##", prescriptionId->toString());
            auto receipt = ::model::Bundle::fromJsonNoValidation(receiptJson);

            ASSERT_NO_THROW(db->activateTask(task, signedPrescription));
            ASSERT_NO_THROW(db->storeChargeInformation(::model::ChargeInformation{
                ::std::move(chargeItem), ::std::move(signedPrescription), ::std::move(prescription),
                ::std::move(signedDispenseItem), ::std::move(dispenseItem), ::std::move(receipt)}));

            if (removeTask)
            {
                ASSERT_NO_THROW(db->updateTaskClearPersonalData(task));
            }

            db->commitTransaction();

            prescriptionIds.push_back(prescriptionId->toString());
        }
    }

    void deleteChargeItems(::std::vector<::std::string>& prescriptionIds)// NOLINT(readability-function-cognitive-complexity)
    {
        auto db = createDatabase();
        for (const auto& id : prescriptionIds)
        {
            ASSERT_NO_THROW(db->deleteChargeInformation(::model::PrescriptionId::fromString(id)));
        }
        db->commitTransaction();
    }

    void sendGetAllChargeItemsRequest( // NOLINT(readability-function-cognitive-complexity)
        HttpsClient& client,
        JWT& jwt,
        std::vector<model::ChargeItem>& chargeItemsResponse,
        std::size_t& totalSearchMatches,
        std::optional<std::vector<std::string>>&& filters = {},
        std::optional<std::pair<size_t, size_t>>&& paging = {},
        const HttpStatus expectedHttpStatus = HttpStatus::OK)
    {
        totalSearchMatches = 0;
        std::string path = "/ChargeItem";

        if (filters.has_value() && !filters->empty())
        {
            path += "?";
            size_t idx = 0;
            for (const std::string& filter : *filters)
            {
                path += filter;
                if (idx < (filters->size() - 1))
                {
                    path += "&";
                }
                ++idx;
            }
        }

        if (paging.has_value())
        {
            if (path.find('?') == std::string::npos)
            {
                path += "?";
            }
            else
            {
                path += "&";
            }
            path += "_count=" + std::to_string(std::get<0>(*paging));
            path += "&__offset=" + std::to_string(std::get<1>(*paging));
        }

        const std::optional<model::Bundle> result = sendGetRequest(client, jwt, totalSearchMatches, path, expectedHttpStatus);
        if(result.has_value())
        {
            for (size_t index = 0; index < result->getResourceCount(); ++index)
            {
                ASSERT_NO_FATAL_FAILURE(
                    chargeItemsResponse.emplace_back(model::ChargeItem::fromJson(result->getResource(index))));
            }
        }
    }

    std::optional<model::Bundle> sendGetRequest(
        HttpsClient& client,
        JWT& jwt,
        std::size_t& totalSearchMatches,
        std::string& argument,
        const HttpStatus expectedHttpStatus = HttpStatus::OK)
    {
        std::optional<model::Bundle> result;
        sendGetRequest(client, jwt, argument, result, expectedHttpStatus);
        totalSearchMatches = result.has_value() ? result->getTotalSearchMatches() : 0;

        return result;
    }

    void sendGetRequest(
        HttpsClient& client,
        JWT& jwt,
        const std::string& argument,
        std::optional<model::Bundle>& chargeItemBundle,
        const HttpStatus expectedHttpStatus = HttpStatus::OK)
    {
        // Create the inner request
        ClientRequest request(createGetHeader(argument, jwt), "");

        // Send the request.
        const auto outerResponse = client.send(encryptRequest(request, jwt));
        const auto innerResponse = verifyOuterResponse(outerResponse);

        ASSERT_FALSE(innerResponse.getHeader().hasHeader(Header::Warning));
        ASSERT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, expectedHttpStatus,
                                                           innerResponse.getHeader().contentType().value()));

        EXPECT_TRUE(innerResponse.getHeader().status() == expectedHttpStatus);

        if (innerResponse.getHeader().status() == HttpStatus::OK)
        {
            if(innerResponse.getHeader().contentType() == std::string(ContentMimeType::fhirJsonUtf8))
            {
                ASSERT_NO_FATAL_FAILURE(chargeItemBundle = model::Bundle::fromJsonNoValidation(innerResponse.getBody()));
            }
            else
            {
                ASSERT_NO_FATAL_FAILURE(chargeItemBundle = model::Bundle::fromXmlNoValidation(innerResponse.getBody()));
            }
        }
    }

    std::string extractPathAndArguments (const std::string_view& url)
    {
        const auto pathStart = url.find("/ChargeItem");
        if (pathStart == std::string::npos)
            return std::string(url);
        else
            return std::string(url.substr(pathStart));
    }
};

// GEMREQ-start A_22119
TEST_F(ChargeItemGetHandlerTest, ChargeItemGetAllInsurant)
{
    // Insert chargeItem into database
    //--------------------------

    static const std::string pharmacyA = "606358757";
    static const std::string pharmacyB = "606358758";

    static const std::vector<std::tuple<std::string, std::string, std::optional<model::Timestamp>>> patientsPharmaciesChargeItems = {
        {InsurantA, pharmacyA, std::nullopt},
        {InsurantA, pharmacyB, std::nullopt},
        {InsurantB, pharmacyA, std::nullopt},
        {InsurantA, pharmacyA, std::nullopt},
        {InsurantC, pharmacyB, std::nullopt}
    };

    std::vector<std::string> prescriptionIDs;
    insertChargeItem(patientsPharmaciesChargeItems, prescriptionIDs, ::MockDatabase::mockAccessCode);

    // GET ChargeItems
    //-------------------------

    // Create a client
    HttpsClient client = createClient();

    std::vector<model::ChargeItem> chargeItems;
    std::size_t totalSearchMatches = 0;

    JWT jwt = mJwtBuilder.makeJwtVersicherter(InsurantA);

    A_22122.test("Existing supportingInformation, no referenced data sets");
    {
        // non-existence of referenced data sets is verified by the construction of charge items from all resources of the result bundle in sendGetRequest()
        sendGetAllChargeItemsRequest(client, jwt, chargeItems, totalSearchMatches);
        A_22119.test("Only charge items with KV number from authorization header found");
        EXPECT_EQ(chargeItems.size(), 3);
        EXPECT_EQ(totalSearchMatches, 3);

        for (const auto& chargeItem : chargeItems)
        {
            A_22119.test("Only charge items with KV number from authorization header found");
            EXPECT_EQ(chargeItem.subjectKvnr(), InsurantA);

            EXPECT_TRUE(chargeItem.supportingInfoReference(model::ChargeItem::SupportingInfoType::receiptBundle));
            EXPECT_FALSE(chargeItem.supportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItemBinary));
            EXPECT_FALSE(chargeItem.containedBinary());
        }
    }

    deleteChargeItems(prescriptionIDs);
}
// GEMREQ-end A_22119

TEST_F(ChargeItemGetHandlerTest, ChargeItemGetAllPharmacy)
{
    static const std::string pharmacyA = "606358757";
    static const std::string pharmacyB = "606358758";

    static const std::vector<std::tuple<std::string, std::string, std::optional<model::Timestamp>>> patientsPharmaciesChargeItems = {
        {InsurantA, pharmacyA, std::nullopt},
        {InsurantA, pharmacyB, std::nullopt},
        {InsurantB, pharmacyA, std::nullopt},
        {InsurantA, pharmacyA, std::nullopt},
        {InsurantC, pharmacyB, std::nullopt}
    };

    std::vector<std::string> prescriptionIDs;
    insertChargeItem(patientsPharmaciesChargeItems, prescriptionIDs, ::MockDatabase::mockAccessCode);

    // GET ChargeItems
    //-------------------------

    // Create a client
    HttpsClient client = createClient();

    std::vector<model::ChargeItem> chargeItems;
    std::size_t totalSearchMatches = 0;
    JWT jwt = mJwtBuilder.makeJwtApotheke(pharmacyB);
    sendGetAllChargeItemsRequest(client, jwt, chargeItems, totalSearchMatches, {}, {}, ::HttpStatus::Forbidden);
    EXPECT_EQ(chargeItems.size(), 0);
    EXPECT_EQ(totalSearchMatches, 0);

    deleteChargeItems(prescriptionIDs);
}

TEST_F(ChargeItemGetHandlerTest, ChargeItemGetAll_FilterByDate) // NOLINT(readability-function-cognitive-complexity)
{
    // Insert ChargeItem into database
    //--------------------------

    static const std::string pharmacyA = "606358757";
    static const std::string pharmacyB = "606358758";

    static const std::vector<std::tuple<std::string, std::string, std::optional<model::Timestamp>>> patientsPharmaciesChargeItems = {
        {InsurantA, pharmacyA, model::Timestamp::fromXsDateTime("2021-01-01T17:13:00+01:00")},
        {InsurantA, pharmacyA, model::Timestamp::fromXsDateTime("2021-01-01T17:13:30+01:00")},
        {InsurantA, pharmacyB, model::Timestamp::fromXsDateTime("2021-02-02T17:13:00+01:00")},
        {InsurantB, pharmacyA, model::Timestamp::fromXsDateTime("2021-03-03T17:13:00+01:00")},
        {InsurantA, pharmacyA, model::Timestamp::fromXsDateTime("2021-04-04T17:13:00+01:00")},
        {InsurantC, pharmacyB, model::Timestamp::fromXsDateTime("2021-05-05T17:13:00+01:00")}
    };

    std::vector<std::string> prescriptionIDs;
    insertChargeItem(patientsPharmaciesChargeItems, prescriptionIDs, ::MockDatabase::mockAccessCode);

    // GET ChargeItems
    //-------------------------

    // Create a client
    auto client = createClient();

    std::size_t totalSearchMatches = 0;

    // Insurant
    {
        std::vector<std::string> filters = { "entered-date=lt2021-02-02" };
        std::vector<model::ChargeItem> chargeItems;
        JWT jwt = mJwtBuilder.makeJwtVersicherter(InsurantC);
        sendGetAllChargeItemsRequest(client, jwt, chargeItems, totalSearchMatches, std::move(filters));
        EXPECT_EQ(chargeItems.size(), 0);
    }

    {
        std::vector<std::string> filters = { "entered-date=lt2021-06-06" };
        std::vector<model::ChargeItem> chargeItems;
        JWT jwt = mJwtBuilder.makeJwtVersicherter(InsurantA);
        sendGetAllChargeItemsRequest(client, jwt, chargeItems, totalSearchMatches, std::move(filters));
        EXPECT_EQ(chargeItems.size(), 4);
    }

    {
        std::vector<std::string> filters = { "entered-date=gt2021-01-01T17:13:00%2B01:00" };
        std::vector<model::ChargeItem> chargeItems;
        JWT jwt = mJwtBuilder.makeJwtVersicherter(InsurantA);
        sendGetAllChargeItemsRequest(client, jwt, chargeItems, totalSearchMatches, std::move(filters));
        EXPECT_EQ(chargeItems.size(), 3);
    }

    {
        std::vector<std::string> filters = { "entered-date=ge2021-01-01T17:13:00%2B01:00" };
        std::vector<model::ChargeItem> chargeItems;
        JWT jwt = mJwtBuilder.makeJwtVersicherter(InsurantA);
        sendGetAllChargeItemsRequest(client, jwt, chargeItems, totalSearchMatches, std::move(filters));
        EXPECT_EQ(chargeItems.size(), 4);
    }

    using namespace std::chrono_literals;

    {
        auto lastUpdatedTime = (model::Timestamp::now() - 5s).toXsDateTimeWithoutFractionalSeconds(model::Timestamp::GermanTimezone);
        lastUpdatedTime = lastUpdatedTime.substr(0, 19);

        std::vector<std::string> filters = { "_lastUpdated=ge" + lastUpdatedTime };
        std::vector<model::ChargeItem> chargeItems;
        JWT jwt = mJwtBuilder.makeJwtVersicherter(InsurantA);
        sendGetAllChargeItemsRequest(client, jwt, chargeItems, totalSearchMatches, std::move(filters));
        EXPECT_EQ(chargeItems.size(), 4);
    }

    std::this_thread::sleep_for(1s);

    {
        auto lastUpdatedTime = model::Timestamp::now().toXsDateTimeWithoutFractionalSeconds(model::Timestamp::GermanTimezone);
        insertChargeItem({{InsurantA, pharmacyB, model::Timestamp::fromXsDateTime("2021-06-06T17:13:00+01:00")}},
                         prescriptionIDs, MockDatabase::mockAccessCode);
        lastUpdatedTime = lastUpdatedTime.substr(0, 19);

        std::vector<std::string> filters = {"_lastUpdated=ge" + lastUpdatedTime};
        std::vector<model::ChargeItem> chargeItems;
        JWT jwt = mJwtBuilder.makeJwtVersicherter(InsurantA);
        sendGetAllChargeItemsRequest(client, jwt, chargeItems, totalSearchMatches, std::move(filters));
        EXPECT_EQ(chargeItems.size(), 1);
    }

    deleteChargeItems(prescriptionIDs);
}

TEST_F(ChargeItemGetHandlerTest, ChargeItemGetAll_Paging) // NOLINT(readability-function-cognitive-complexity)
{
    // Insert tasks into database
    //---------------------------

    static const std::string pharmacyA = "606358757";
    static const std::string pharmacyB = "606358758";
    static const std::string pharmacyC = "606358759";

    std::vector<std::tuple<std::string, std::string, std::optional<model::Timestamp>>> patientsPharmacies;

    int countInsurantPharmacy = 11;
	patientsPharmacies.reserve(100);
    for (int idxPatient = 0; idxPatient < countInsurantPharmacy; ++idxPatient)
    {
        patientsPharmacies.emplace_back(InsurantA, pharmacyA, std::nullopt);
    }

    for (int idxPatient = 0; idxPatient < countInsurantPharmacy; ++idxPatient)
    {
        patientsPharmacies.emplace_back(InsurantA, pharmacyB, std::nullopt);
        patientsPharmacies.emplace_back(InsurantB, pharmacyA, std::nullopt);
    }

    for (int idxPatient = 0; idxPatient < countInsurantPharmacy; ++idxPatient)
    {
        patientsPharmacies.emplace_back(InsurantA, pharmacyC, std::nullopt);
        patientsPharmacies.emplace_back(InsurantB, pharmacyB, std::nullopt);
        patientsPharmacies.emplace_back(InsurantC, pharmacyA, std::nullopt);
    }

    // insert ChargeItem
    std::vector<std::string> prescriptionIDs;
    insertChargeItem(patientsPharmacies, prescriptionIDs, ::MockDatabase::mockAccessCode);

    // Create a client
    auto client = createClient();
    std::size_t totalSearchMatches = 0;

    // Insurant paging
    std::vector<std::pair<int, int>> pagings = {{10, 0}, {10, 27}, {3, 30}, {5, 33}};
    JWT jwt = mJwtBuilder.makeJwtVersicherter(InsurantA);
    auto totalSearch = 33;
    for(const auto& paging : pagings)
    {
        std::vector<model::ChargeItem> chargeItems;
        sendGetAllChargeItemsRequest(client, jwt, chargeItems, totalSearchMatches, {}, paging);
        auto remainder = totalSearch - paging.second;
        auto expectedPage = (remainder >= paging.first) ? paging.first : remainder <= 0 ? 0 : remainder;
        ASSERT_EQ(chargeItems.size(), expectedPage);
        ASSERT_EQ(totalSearchMatches, totalSearch);
    }

    pagings = {{10, 0}, {10, 5}, {3, 19}, {5, 22}};
    jwt = mJwtBuilder.makeJwtVersicherter(InsurantB);
    totalSearch = 22;
    for(const auto& paging : pagings)
    {
        std::vector<model::ChargeItem> chargeItems;
        sendGetAllChargeItemsRequest(client, jwt, chargeItems, totalSearchMatches, {}, paging);
        auto remainder = totalSearch - paging.second;
        auto expectedPage = (remainder >= paging.first) ? paging.first : remainder <= 0 ? 0 : remainder;
        ASSERT_EQ(chargeItems.size(), expectedPage);
        ASSERT_EQ(totalSearchMatches, totalSearch);
    }

    pagings = {{10, 0}, {3, 3}, {3, 8}, {5, 11}};
    jwt = mJwtBuilder.makeJwtVersicherter(InsurantC);
    totalSearch = 11;
    for (const auto& paging : pagings)
    {
        std::vector<model::ChargeItem> chargeItems;
        sendGetAllChargeItemsRequest(client, jwt, chargeItems, totalSearchMatches, {}, paging);
        auto remainder = totalSearch - paging.second;
        auto expectedPage = (remainder >= paging.first) ? paging.first : remainder <= 0 ? 0 : remainder;
        ASSERT_EQ(chargeItems.size(), expectedPage);
        ASSERT_EQ(totalSearchMatches, totalSearch);
    }

    deleteChargeItems(prescriptionIDs);
}

TEST_F(ChargeItemGetHandlerTest, ChargeItemGetAll_FiltersPaging) // NOLINT(readability-function-cognitive-complexity)
{
    // Insert tasks into database
    //---------------------------

    static const std::string pharmacyA = "606358757";
    static const std::string pharmacyB = "606358758";
    static const std::string pharmacyC = "606358759";

    std::vector<std::tuple<std::string, std::string, std::optional<model::Timestamp>>> patientsPharmacies;

    model::Timestamp timestamp = model::Timestamp::fromXsDateTime("2021-09-25T12:34:56+01:00");
    static const int countInsurantPharmacy = 11;
	patientsPharmacies.reserve(100);
    for (int idxPatient = 0; idxPatient < countInsurantPharmacy; ++idxPatient)
    {
        patientsPharmacies.emplace_back(InsurantA, pharmacyA, timestamp + std::chrono::hours(-24 * idxPatient));
    }

    timestamp = model::Timestamp::fromXsDateTime("2021-10-25T12:34:56+01:00");
    for (int idxPatient = 0; idxPatient < countInsurantPharmacy; ++idxPatient)
    {
        patientsPharmacies.emplace_back(InsurantA, pharmacyB, timestamp + std::chrono::hours(-24 * idxPatient));
        patientsPharmacies.emplace_back(InsurantB, pharmacyA, timestamp + std::chrono::hours(-24 * idxPatient));
    }

    timestamp = model::Timestamp::fromXsDateTime("2021-11-25T12:34:56+01:00");
    for (int idxPatient = 0; idxPatient < countInsurantPharmacy; ++idxPatient)
    {
        patientsPharmacies.emplace_back(InsurantA, pharmacyC, timestamp + std::chrono::hours(-24 * idxPatient));
        patientsPharmacies.emplace_back(InsurantB, pharmacyB, timestamp + std::chrono::hours(-24 * idxPatient));
        patientsPharmacies.emplace_back(InsurantC, pharmacyA, timestamp + std::chrono::hours(-24 * idxPatient));
    }

    // insert ChargeItem
    std::vector<std::string> prescriptionIDs;
    insertChargeItem(patientsPharmacies, prescriptionIDs, ::MockDatabase::mockAccessCode);

    // Create a client
    auto client = createClient();
    std::size_t totalSearchMatches = 0;

    // Insurant paging
    std::vector<std::pair<int, int>> pagings = {{10, 0}, {10, 27}, {3, 30}, {5, 33}};
    JWT jwt = mJwtBuilder.makeJwtVersicherter(InsurantA);
    std::vector<std::string> filters = { "entered-date=le2021-11-25T12:34:56%2b01:00" };
    auto totalSearch = 33;
    for(const auto& paging : pagings)
    {
        std::vector<model::ChargeItem> chargeItems;
        sendGetAllChargeItemsRequest(client, jwt, chargeItems, totalSearchMatches, filters, paging);
        auto remainder = totalSearch - paging.second;
        auto expectedPage = (remainder >= paging.first) ? paging.first : remainder <= 0 ? 0 : remainder;
        ASSERT_EQ(chargeItems.size(), expectedPage);
        ASSERT_EQ(totalSearchMatches, totalSearch);
    }

    pagings = {{10, 0}, {10, 5}, {3, 19}, {5, 22}};
    jwt = mJwtBuilder.makeJwtVersicherter(InsurantB);
    totalSearch = 22;
    for(const auto& paging : pagings)
    {
        std::vector<model::ChargeItem> chargeItems;
        sendGetAllChargeItemsRequest(client, jwt, chargeItems, totalSearchMatches, {}, paging);
        auto remainder = totalSearch - paging.second;
        auto expectedPage = (remainder >= paging.first) ? paging.first : remainder <= 0 ? 0 : remainder;
        ASSERT_EQ(chargeItems.size(), expectedPage);
        ASSERT_EQ(totalSearchMatches, totalSearch);
    }

    pagings = {{10, 0}, {3, 3}, {3, 8}, {5, 11}};
    jwt = mJwtBuilder.makeJwtVersicherter(InsurantC);
    totalSearch = 11;
    for(const auto& paging : pagings)
    {
        std::vector<model::ChargeItem> chargeItems;
        sendGetAllChargeItemsRequest(client, jwt, chargeItems, totalSearchMatches, {}, paging);
        auto remainder = totalSearch - std::get<1>(paging);
        auto expectedPage = (remainder >= std::get<0>(paging)) ? std::get<0>(paging) : remainder <= 0 ? 0 : remainder;
        ASSERT_EQ(chargeItems.size(), expectedPage);
        ASSERT_EQ(totalSearchMatches, totalSearch);
    }

    deleteChargeItems(prescriptionIDs);
}

TEST_F(ChargeItemGetHandlerTest, ChargeItemGetAll_PagingLinks) // NOLINT(readability-function-cognitive-complexity)
{
    // Insert tasks into database
    //---------------------------

    static const std::string pharmacyA = "606358757";
    static const std::string pharmacyB = "606358758";
    static const std::string pharmacyC = "606358759";

    std::vector<std::tuple<std::string, std::string, std::optional<model::Timestamp>>> patientsPharmacies;

    model::Timestamp timestamp = model::Timestamp::fromXsDateTime("2021-09-25T12:34:56+01:00");
    static const int countInsurantPharmacy = 11;
	patientsPharmacies.reserve(100);
    for (int idxPatient = 0; idxPatient < countInsurantPharmacy; ++idxPatient)
    {
        patientsPharmacies.emplace_back(InsurantA, pharmacyA, timestamp + std::chrono::hours(-24 * idxPatient));
    }

    timestamp = model::Timestamp::fromXsDateTime("2021-10-25T12:34:56+01:00");
    for (int idxPatient = 0; idxPatient < countInsurantPharmacy; ++idxPatient)
    {
        patientsPharmacies.emplace_back(InsurantA, pharmacyB, timestamp + std::chrono::hours(-24 * idxPatient));
        patientsPharmacies.emplace_back(InsurantB, pharmacyA, timestamp + std::chrono::hours(-24 * idxPatient));
    }

    timestamp = model::Timestamp::fromXsDateTime("2021-11-25T12:34:56+01:00");
    for (int idxPatient = 0; idxPatient < countInsurantPharmacy; ++idxPatient)
    {
        patientsPharmacies.emplace_back(InsurantA, pharmacyC, timestamp + std::chrono::hours(-24 * idxPatient));
        patientsPharmacies.emplace_back(InsurantB, pharmacyB, timestamp + std::chrono::hours(-24 * idxPatient));
        patientsPharmacies.emplace_back(InsurantC, pharmacyA, timestamp + std::chrono::hours(-24 * idxPatient));
    }

    // insert ChargeItem
    std::vector<std::string> prescriptionIDs;
    insertChargeItem(patientsPharmacies, prescriptionIDs, ::MockDatabase::mockAccessCode);

    // Create a client
    auto client = createClient();
    std::size_t totalSearchMatches = 0;

    // Insurant InsurantA
    std::vector<std::pair<int, int>> pagings = {{11, 0}, {5, 0}, {3, 19}, {5, 22}};
    JWT jwt = mJwtBuilder.makeJwtVersicherter(InsurantA);
    auto totalSearch = 22;

    for(const auto& paging : pagings)
    {
        auto hasLinkCount = (totalSearch - paging.second)  / paging.first;
        hasLinkCount -= ((totalSearch - paging.second) % paging.first) == 0 ? 1 : 0;
        std::string argument = { "/ChargeItem?entered-date=gt2021-09-25T12%3a34%3a56%2B01%3a00&_count=" +
                                std::to_string(paging.first) + "&__offset=" + std::to_string(paging.second)};
        std::optional<model::Bundle> bundle = {};
        std::optional<std::string> nextLink = {};
        for (int idx = 0; idx < hasLinkCount; ++idx)
        {
            bundle = sendGetRequest(client, jwt, totalSearchMatches, argument);
            EXPECT_EQ(totalSearchMatches, totalSearch);
            nextLink = bundle->getLink(model::Link::Type::Next);
            EXPECT_TRUE(nextLink.has_value());
            argument = extractPathAndArguments(nextLink.value());
            EXPECT_EQ(argument,
                      "/ChargeItem?entered-date=gt2021-09-25T11%3a34%3a56%2b00%3a00&_count=" + std::to_string(paging.first) +
                          "&__offset="+ std::to_string(((idx+1) * paging.first) + paging.second));
            EXPECT_EQ(UrlHelper::unescapeUrl(argument),
                      "/ChargeItem?entered-date=gt2021-09-25T11:34:56+00:00&_count=" + std::to_string(paging.first) +
                          "&__offset="+ std::to_string(((idx+1) * paging.first) + paging.second));
        }
        bundle = sendGetRequest(client, jwt, totalSearchMatches, argument);
        EXPECT_EQ(totalSearchMatches, totalSearch);
        nextLink = bundle->getLink(model::Link::Type::Next);
        EXPECT_FALSE(nextLink.has_value());
    }

    // Insurant InsurantC
    pagings = {{10, 0}, {3, 3}, {3, 8}, {5, 11}};
    jwt = mJwtBuilder.makeJwtVersicherter(InsurantC);
    totalSearch = 11;
    for(const auto& paging : pagings)
    {
        int hasLinkCount = (totalSearch - paging.second)  / paging.first;
        hasLinkCount -= ((totalSearch - paging.second) % paging.first) == 0 ? 1 : 0;
        std::string argument = { "/ChargeItem?entered-date=le2021-11-25T12%3a34%3a56%2B01%3a00&_count=" +
                                std::to_string(paging.first) + "&__offset=" + std::to_string(paging.second)};
        std::optional<model::Bundle> bundle = {};
        std::optional<std::string> nextLink = {};
        for (int idx = 0; idx < hasLinkCount; ++idx)
        {
            bundle = sendGetRequest(client, jwt, totalSearchMatches, argument);
            EXPECT_EQ(totalSearchMatches, totalSearch);
            nextLink = bundle->getLink(model::Link::Type::Next);
            EXPECT_TRUE(nextLink.has_value());
            argument = extractPathAndArguments(nextLink.value());
            EXPECT_EQ(argument,
                      "/ChargeItem?entered-date=le2021-11-25T11%3a34%3a56%2b00%3a00&_count=" + std::to_string(paging.first) +
                          "&__offset="+ std::to_string(((idx+1) * paging.first) + paging.second));
            EXPECT_EQ(UrlHelper::unescapeUrl(argument),
                      "/ChargeItem?entered-date=le2021-11-25T11:34:56+00:00&_count=" + std::to_string(paging.first) +
                          "&__offset=" + std::to_string(((idx + 1) * paging.first) + paging.second));
        }
        bundle = sendGetRequest(client, jwt, totalSearchMatches, argument);
        EXPECT_EQ(totalSearchMatches, totalSearch);
        nextLink = bundle->getLink(model::Link::Type::Next);
        EXPECT_FALSE(nextLink.has_value());
    }

    deleteChargeItems(prescriptionIDs);
}

TEST_F(ChargeItemGetHandlerTest, ChargeItemGetByIdPharmacy)//NOLINT(readability-function-cognitive-complexity)
{
    // Insert chargeItem into database
    //--------------------------

    static const std::string pharmacyA = "606358757";
    static const std::string pharmacyB = "606358758";

    static const std::vector<std::tuple<std::string, std::string, std::optional<model::Timestamp>>> patientsPharmaciesChargeItems = {
        {InsurantA, pharmacyA, std::nullopt},
        {InsurantB, pharmacyB, std::nullopt},
    };

    std::vector<std::string> prescriptionIDs;
    ASSERT_NO_FATAL_FAILURE(
        insertChargeItem(patientsPharmaciesChargeItems, prescriptionIDs, MockDatabase::mockAccessCode));

    // GET ChargeItems
    //-------------------------

    // Create a client
    HttpsClient client = createClient();
    std::optional<model::Bundle> chargeItemBundle;

    // PharmacyA, InsurantA
    {
        std::string argument = {"/ChargeItem/" + prescriptionIDs[0]};
        JWT jwt = mJwtBuilder.makeJwtApotheke(pharmacyA);
        ASSERT_NO_FATAL_FAILURE(sendGetRequest(client, jwt, argument, chargeItemBundle, HttpStatus::Forbidden));
        EXPECT_FALSE(chargeItemBundle.has_value());

        argument += std::string{"?ac="} + MockDatabase::mockAccessCode.data();
        ASSERT_NO_FATAL_FAILURE(sendGetRequest(client, jwt, argument, chargeItemBundle));
        EXPECT_TRUE(chargeItemBundle.has_value());

        auto chargeItem = chargeItemBundle->getResourcesByType<model::ChargeItem>("ChargeItem");
        ASSERT_EQ(chargeItem.size(), 1);

        EXPECT_EQ(chargeItem[0].subjectKvnr(), InsurantA);

        A_22126.test("Telematik-ID check");
        EXPECT_EQ(chargeItem[0].entererTelematikId(), pharmacyA);

        A_22128_01.test("Check ChargeItem.supportingInformation");
        EXPECT_FALSE(chargeItem[0].accessCode());

        const auto bundles = chargeItemBundle->getResourcesByType<::model::Bundle>();

        ASSERT_EQ(bundles.size(), 2); // prescription and dispenseItem
        EXPECT_TRUE(
            chargeItem[0].supportingInfoReference(model::ChargeItem::SupportingInfoType::prescriptionItemBundle).has_value());
        EXPECT_EQ(
            Uuid{chargeItem[0].prescriptionId()->deriveUuid(model::uuidFeaturePrescription)}.toUrn(),
            chargeItem[0].supportingInfoReference(model::ChargeItem::SupportingInfoType::prescriptionItemBundle).value());

        EXPECT_TRUE(
            chargeItem[0].supportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItemBundle).has_value());
        EXPECT_EQ(Uuid{chargeItem[0].prescriptionId()->deriveUuid(model::uuidFeatureDispenseItem)}.toUrn(),
                  chargeItem[0].supportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItemBundle).value());

        EXPECT_TRUE(chargeItem[0].supportingInfoReference(model::ChargeItem::SupportingInfoType::receiptBundle).has_value());

        EXPECT_FALSE(chargeItem[0].containedBinary());

        auto tslMgr = MockTslManager::createMockTslManager(StaticData::getXmlValidator());
        {
            const auto& prescription = bundles[0];
            ASSERT_TRUE(prescription.getSignature().has_value());
            auto sig = prescription.getSignature();
            ASSERT_TRUE(sig.has_value());
            auto prescriptionSigData = sig->data();
            ASSERT_TRUE(prescriptionSigData.has_value());
            EXPECT_NO_THROW(CadesBesSignature(std::string(prescriptionSigData.value().data()), *tslMgr, false));
            EXPECT_FALSE(sig->whoReference().has_value());
            ASSERT_TRUE(sig->whoDisplay().has_value());
            EXPECT_EQ(sig->whoDisplay().value(), "Arzt");
        }

        {
            const auto& dispenseItem = bundles[1];
            ASSERT_TRUE(dispenseItem.getSignature().has_value());
            auto sig = dispenseItem.getSignature();
            ASSERT_TRUE(sig.has_value());
            auto dispenseItemSigData = sig->data();
            ASSERT_TRUE(dispenseItemSigData.has_value());
            EXPECT_NO_THROW(CadesBesSignature(std::string(dispenseItemSigData.value().data()), *tslMgr, false));
            EXPECT_FALSE(sig->whoReference().has_value());
            ASSERT_TRUE(sig->whoDisplay().has_value());
            EXPECT_EQ(sig->whoDisplay().value(), "Apotheke");
        }
    }

    // PharmacyB, InsurantB
    {
        std::string argument = {"/ChargeItem/" + prescriptionIDs[1]};
        JWT jwt = mJwtBuilder.makeJwtApotheke(pharmacyB);
        chargeItemBundle.reset();
        sendGetRequest(client, jwt, argument, chargeItemBundle, HttpStatus::Forbidden);
        EXPECT_FALSE(chargeItemBundle.has_value());

        argument += std::string{"?ac="} + MockDatabase::mockAccessCode.data();
        sendGetRequest(client, jwt, argument, chargeItemBundle);
        EXPECT_TRUE(chargeItemBundle.has_value());

        auto chargeItem = chargeItemBundle->getResourcesByType<model::ChargeItem>("ChargeItem");
        ASSERT_EQ(chargeItem.size(), 1);

        EXPECT_EQ(chargeItem[0].subjectKvnr(), InsurantB);

        A_22126.test("Telematik-ID check");
        EXPECT_EQ(chargeItem[0].entererTelematikId(), pharmacyB);

        A_22128_01.test("Check ChargeItem.supportingInformation");
        EXPECT_FALSE(chargeItem[0].accessCode());

        const auto bundles = chargeItemBundle->getResourcesByType<::model::Bundle>("Bundle");

        ASSERT_EQ(bundles.size(), 2); // prescription and dispenseItem
        EXPECT_TRUE(
            chargeItem[0].supportingInfoReference(model::ChargeItem::SupportingInfoType::prescriptionItemBundle).has_value());
        EXPECT_EQ(
            Uuid{chargeItem[0].prescriptionId()->deriveUuid(model::uuidFeaturePrescription)}.toUrn(),
            chargeItem[0].supportingInfoReference(model::ChargeItem::SupportingInfoType::prescriptionItemBundle).value());

        EXPECT_TRUE(
            chargeItem[0].supportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItemBundle).has_value());
        EXPECT_EQ(Uuid{chargeItem[0].prescriptionId()->deriveUuid(model::uuidFeatureDispenseItem)}.toUrn(),
                  chargeItem[0].supportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItemBundle).value());

        EXPECT_TRUE(chargeItem[0].supportingInfoReference(model::ChargeItem::SupportingInfoType::receiptBundle).has_value());

        EXPECT_FALSE(chargeItem[0].containedBinary());
    }

    deleteChargeItems(prescriptionIDs);
}

TEST_F(ChargeItemGetHandlerTest, ChargeItemGetByIdPharmacyNoTask)//NOLINT(readability-function-cognitive-complexity)
{
    // Insert chargeItem into database
    //--------------------------

    static const ::std::string pharmacyA = "606358757";

    static const ::std::vector<::std::tuple<::std::string, ::std::string, ::std::optional<::model::Timestamp>>>
        patientsPharmaciesChargeItems = {{InsurantA, pharmacyA, std::nullopt}};

    ::std::vector<::std::string> prescriptionIDs;
    insertChargeItem(patientsPharmaciesChargeItems, prescriptionIDs, MockDatabase::mockAccessCode, true);

    //-------------------------

    // Create a client
    auto client = createClient();
    ::std::optional<::model::Bundle> chargeItemBundle;

    ::std::string argument = {"/ChargeItem/" + prescriptionIDs[0]};
    auto jwt = mJwtBuilder.makeJwtApotheke(pharmacyA);
    sendGetRequest(client, jwt, argument, chargeItemBundle, HttpStatus::Forbidden);
    EXPECT_FALSE(chargeItemBundle.has_value());

    argument += ::std::string{"?ac="} + ::std::string{::MockDatabase::mockAccessCode};
    sendGetRequest(client, jwt, argument, chargeItemBundle);
    EXPECT_TRUE(chargeItemBundle.has_value());

    auto chargeItem = chargeItemBundle->getResourcesByType<::model::ChargeItem>("ChargeItem");
    ASSERT_EQ(chargeItem.size(), 1);

    EXPECT_EQ(chargeItem[0].subjectKvnr(), InsurantA);

    A_22126.test("Telematik-ID check");
    EXPECT_EQ(chargeItem[0].entererTelematikId(), pharmacyA);

    A_22128_01.test("Check ChargeItem.supportingInformation");
    auto bundles = chargeItemBundle->getResourcesByType<model::Bundle>("Bundle");
    ASSERT_EQ(bundles.size(), 2); // prescription and dispenseItem

    EXPECT_TRUE(
        chargeItem[0].supportingInfoReference(::model::ChargeItem::SupportingInfoType::dispenseItemBundle).has_value());
    ASSERT_EQ(Uuid{chargeItem[0].prescriptionId()->deriveUuid(model::uuidFeatureDispenseItem)}.toUrn(),
              chargeItem[0].supportingInfoReference(::model::ChargeItem::SupportingInfoType::dispenseItemBundle).value());

    deleteChargeItems({prescriptionIDs});
}

TEST_F(ChargeItemGetHandlerTest, ChargeItemGet_BAD)
{
    // Insert chargeItem into database
    //--------------------------

    static const std::string pharmacyA = "606358757";
    static const std::string pharmacyB = "606358758";

    static const std::vector<std::tuple<std::string, std::string, std::optional<model::Timestamp>>> patientsPharmaciesChargeItems = {
        {InsurantA, pharmacyA, std::nullopt},
        {InsurantA, pharmacyB, std::nullopt},
    };

    std::vector<std::string> prescriptionIDs;
    insertChargeItem(patientsPharmaciesChargeItems, prescriptionIDs, ::MockDatabase::mockAccessCode);

    // GET ChargeItems
    //-------------------------

    // Create a client
    HttpsClient client = createClient();
    std::optional<model::Bundle> chargeItemBundle;

    // Without id
    {
        std::string argument = {"/ChargeItem/"};
        JWT jwt = mJwtBuilder.makeJwtArzt(pharmacyA);
        sendGetRequest(client, jwt, argument, chargeItemBundle, HttpStatus::Forbidden);
        EXPECT_FALSE(chargeItemBundle.has_value());
    }

    // PharmacyA (prescriptionID[1] is InsurantA, pharmacyB)
    {
        std::string argument = {"/ChargeItem/" + prescriptionIDs[1]};
        JWT jwt = mJwtBuilder.makeJwtApotheke(pharmacyA);
        sendGetRequest(client, jwt, argument, chargeItemBundle, HttpStatus::Forbidden);
        EXPECT_FALSE(chargeItemBundle.has_value());
    }

    // PharmacyB (prescriptionID[0] is InsurantA, pharmacyA)
    {
        std::string argument = {"/ChargeItem/" + prescriptionIDs[0]};
        JWT jwt = mJwtBuilder.makeJwtApotheke(pharmacyB);
        sendGetRequest(client, jwt, argument, chargeItemBundle, HttpStatus::Forbidden);
        EXPECT_FALSE(chargeItemBundle.has_value());
    }

    deleteChargeItems(prescriptionIDs);
}
