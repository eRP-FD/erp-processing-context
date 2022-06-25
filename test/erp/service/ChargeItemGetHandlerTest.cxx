/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/service/chargeitem/ChargeItemGetHandler.hxx"
#include "erp/ErpRequirements.hxx"
#include "erp/model/ChargeItem.hxx"
#include "erp/model/Patient.hxx"
#include "erp/util/Base64.hxx"
#include "erp/util/ByteHelper.hxx"
#include "erp/util/Uuid.hxx"
#include "erp/util/search/UrlArguments.hxx"

#include "test/erp/model/CommunicationTest.hxx"
#include "test/mock/MockDatabase.hxx"
#include "test/workflow-test/ErpWorkflowTestFixture.hxx"
#include "test/util/ResourceManager.hxx"
#include "test/util/ServerTestBase.hxx"

#include <chrono>

class ChargeItemGetHandlerTest : public ServerTestBase
{
public:
    ChargeItemGetHandlerTest() :
        ServerTestBase()
    {
    }
protected:

    void insertChargeItem( // NOLINT(readability-function-cognitive-complexity)
        const std::vector<std::tuple<std::string, std::string, std::optional<model::Timestamp>>>& patientsPharmaciesChargeItem,
        std::vector<std::string>& prescriptionIds,
        const std::optional<std::string_view>& accessCode = std::nullopt)
    {
        ResourceManager& resourceManager = ResourceManager::instance();

        for (const auto& patientAndPharmacy : patientsPharmaciesChargeItem)
        {
            std::string chargeItemXML = resourceManager.getStringResource("test/EndpointHandlerTest/charge_item_input.xml");
            chargeItemXML = String::replaceAll(chargeItemXML, "72bd741c-7ad8-41d8-97c3-9aabbdd0f5b4", "fe4a04af-0828-4977-a5ce-bfeed16ebf10");
            auto chargeItem = model::ChargeItem::fromXmlNoValidation(chargeItemXML);
            const auto& dispenseItemXML = resourceManager.getStringResource("test/EndpointHandlerTest/dispense_item.xml");
            auto dispenseItem = model::Bundle::fromXmlNoValidation(dispenseItemXML);
            const auto& [insurant, pharmacy, enteredDate] = patientAndPharmacy;

            chargeItem.setSubjectKvnr(insurant);
            chargeItem.setEntererTelematikId(pharmacy);
            chargeItem.setEnteredDate(enteredDate.value_or(model::Timestamp::now()));
            if (accessCode.has_value())
            {
                chargeItem.setAccessCode(*accessCode);
            }
            auto db = createDatabase();
            model::Task task(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, "09409348029834029384023984209");
            task.setKvnr(insurant);
            std::optional<model::PrescriptionId> prescriptionId;
            ASSERT_NO_THROW(prescriptionId.emplace(db->storeTask(task)));
            task.setPrescriptionId(*prescriptionId);
            task.setExpiryDate(model::Timestamp::now());
            task.setAcceptDate(model::Timestamp::now());
            ASSERT_NO_THROW(db->activateTask(task, model::Binary("","")));
            chargeItem.setId(*prescriptionId);
            ASSERT_NO_THROW(db->storeChargeInformation(pharmacy, chargeItem, dispenseItem));
            db->commitTransaction();

            prescriptionIds.push_back(prescriptionId->toString());
        }
    }

    void sendRequest( // NOLINT(readability-function-cognitive-complexity)
        HttpsClient& client,
        JWT& jwt,
        std::vector<model::ChargeItem>& chargeItemsResponse,
        std::size_t& totalSearchMatches,
        std::optional<std::vector<std::string>>&& filters = {},
        std::optional<std::pair<size_t, size_t>>&& paging = {},
        HttpStatus expectedHttpStatus = HttpStatus::OK)
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

        // Create the inner request
        ClientRequest request(createGetHeader(path, jwt), "");

        // Send the request.
        auto outerResponse = client.send(encryptRequest(request, jwt));

        // Verify and decrypt the outer response. Also the generic part of the inner response.
        auto innerResponse = verifyOuterResponse(outerResponse);

        ASSERT_FALSE(innerResponse.getHeader().hasHeader(Header::Warning));
        ASSERT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, expectedHttpStatus,
                                                           innerResponse.getHeader().contentType().value()));

        if (expectedHttpStatus == HttpStatus::OK)
        {
            model::Bundle responseBundle(model::BundleType::searchset, ::model::ResourceBase::NoProfile);
            if(innerResponse.getHeader().contentType() == std::string(ContentMimeType::fhirJsonUtf8))
            {
                ASSERT_NO_FATAL_FAILURE(responseBundle = model::Bundle::fromJsonNoValidation(innerResponse.getBody()));
            }
            else
            {
                ASSERT_NO_FATAL_FAILURE(responseBundle = model::Bundle::fromXmlNoValidation(innerResponse.getBody()));
            }

            for (size_t index = 0; index < responseBundle.getResourceCount(); ++index)
            {
                ASSERT_NO_FATAL_FAILURE(chargeItemsResponse.emplace_back(
                    model::ChargeItem::fromJson(responseBundle.getResource(index))));
            }

            totalSearchMatches = responseBundle.getTotalSearchMatches();
        }
    }

    std::optional<model::Bundle> sendRequest(
        HttpsClient& client,
        JWT& jwt,
        std::size_t& totalSearchMatches,
        std::string& argument)
    {
        // Create the inner request
        ClientRequest request(createGetHeader(argument, jwt), "");

        // Send the request.
        auto outerResponse = client.send(encryptRequest(request, jwt));
        auto innerResponse = verifyOuterResponse(outerResponse);
        if (innerResponse.getHeader().status() == HttpStatus::OK)
        {
            model::Bundle responseBundle(model::BundleType::searchset, ::model::ResourceBase::NoProfile);
            if(innerResponse.getHeader().contentType() == std::string(ContentMimeType::fhirJsonUtf8))
            {
            responseBundle = model::Bundle::fromJsonNoValidation(innerResponse.getBody());
            }
            else
            {
            responseBundle = model::Bundle::fromXmlNoValidation(innerResponse.getBody());
            }
            totalSearchMatches = responseBundle.getTotalSearchMatches();
            return responseBundle;
        }
        return {};
    }

    void sendRequest(
        HttpsClient& client,
        JWT& jwt,
        const std::string& argument,
        std::optional<model::Bundle>& chargeItemBundle,
        HttpStatus expectedHttpStatus = HttpStatus::OK)
    {
        // Create the inner request
        ClientRequest request(createGetHeader(argument, jwt), "");
        PcServiceContext mContext = StaticData::makePcServiceContext();

        // Send the request.
        auto outerResponse = client.send(encryptRequest(request, jwt));
        auto innerResponse = verifyOuterResponse(outerResponse);

        EXPECT_TRUE(innerResponse.getHeader().status() == expectedHttpStatus);

        if (innerResponse.getHeader().status() == HttpStatus::OK)
        {
            if(innerResponse.getHeader().contentType() == std::string(ContentMimeType::fhirJsonUtf8))
            {
                chargeItemBundle = model::Bundle::fromJsonNoValidation(innerResponse.getBody());
            }
            else
            {
                chargeItemBundle = model::Bundle::fromXmlNoValidation(innerResponse.getBody());
            }
        }
    }

    void checkChargeItemSupportingInfoElement(const std::vector<model::ChargeItem>& chargeItems)
    {
        for (const auto& chargeItem : chargeItems)
        {
            auto supportingInfoElement = chargeItem.supportingInfoReference(model::ChargeItem::SupportingInfoType::prescriptionItem);
            ASSERT_FALSE(supportingInfoElement.has_value());

            supportingInfoElement = chargeItem.supportingInfoReference(model::ChargeItem::SupportingInfoType::receipt);
            ASSERT_FALSE(supportingInfoElement.has_value());

            supportingInfoElement = chargeItem.supportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItem);
            ASSERT_FALSE(supportingInfoElement.has_value());
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

TEST_F(ChargeItemGetHandlerTest, ChargeItemGetAllInsurant)
{
    // Insert chargeItem into database
    //--------------------------

    std::string pharmacyA = "606358757";
    std::string pharmacyB = "606358758";

    std::vector<std::tuple<std::string, std::string, std::optional<model::Timestamp>>> patientsPharmaciesChargeItems = {
        {InsurantA, pharmacyA, std::nullopt},
        {InsurantA, pharmacyB, std::nullopt},
        {InsurantB, pharmacyA, std::nullopt},
        {InsurantA, pharmacyA, std::nullopt},
        {InsurantC, pharmacyB, std::nullopt}
    };

    std::vector<std::string> prescriptionIDs;
    insertChargeItem(patientsPharmaciesChargeItems, prescriptionIDs);

    // GET ChargeItems
    //-------------------------

    // Create a client
    HttpsClient client = createClient();

    std::vector<model::ChargeItem> chargeItems;
    std::size_t totalSearchMatches = 0;

    JWT jwt = mJwtBuilder.makeJwtVersicherter(InsurantA);
    sendRequest(client, jwt, chargeItems, totalSearchMatches);
    EXPECT_EQ(chargeItems.size(), 3);
    EXPECT_EQ(totalSearchMatches, 3);

    checkChargeItemSupportingInfoElement(chargeItems);
}

TEST_F(ChargeItemGetHandlerTest, ChargeItemGetAllPharmacie)
{
    std::string pharmacyA = "606358757";
    std::string pharmacyB = "606358758";

    std::vector<std::tuple<std::string, std::string, std::optional<model::Timestamp>>> patientsPharmaciesChargeItems = {
        {InsurantA, pharmacyA, std::nullopt},
        {InsurantA, pharmacyB, std::nullopt},
        {InsurantB, pharmacyA, std::nullopt},
        {InsurantA, pharmacyA, std::nullopt},
        {InsurantC, pharmacyB, std::nullopt}
    };

    std::vector<std::string> prescriptionIDs;
    insertChargeItem(patientsPharmaciesChargeItems, prescriptionIDs);

    // GET ChargeItems
    //-------------------------

    // Create a client
    HttpsClient client = createClient();

    std::vector<model::ChargeItem> chargeItems;
    std::size_t totalSearchMatches = 0;
    JWT jwt = mJwtBuilder.makeJwtApotheke(pharmacyB);
    sendRequest(client, jwt, chargeItems, totalSearchMatches);
    EXPECT_EQ(chargeItems.size(), 2);
    EXPECT_EQ(totalSearchMatches, 2);

    checkChargeItemSupportingInfoElement(chargeItems);
}

TEST_F(ChargeItemGetHandlerTest, ChargeItemGet_Filter) // NOLINT(readability-function-cognitive-complexity)
{
    // Insert ChargeItem into database
    //--------------------------

    std::string pharmacyA = "606358757";
    std::string pharmacyB = "606358758";

    std::vector<std::tuple<std::string, std::string, std::optional<model::Timestamp>>> patientsPharmaciesChargeItems = {
        {InsurantA, pharmacyA, model::Timestamp::fromXsDateTime("2021-01-01T17:13:00+01:00")},
        {InsurantA, pharmacyA, model::Timestamp::fromXsDateTime("2021-01-01T17:13:30+01:00")},
        {InsurantA, pharmacyB, model::Timestamp::fromXsDateTime("2021-02-02T17:13:00+01:00")},
        {InsurantB, pharmacyA, model::Timestamp::fromXsDateTime("2021-03-03T17:13:00+01:00")},
        {InsurantA, pharmacyA, model::Timestamp::fromXsDateTime("2021-04-04T17:13:00+01:00")},
        {InsurantC, pharmacyB, model::Timestamp::fromXsDateTime("2021-05-05T17:13:00+01:00")}
    };

    std::vector<std::string> prescriptionIDs;
    insertChargeItem(patientsPharmaciesChargeItems, prescriptionIDs);

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
        sendRequest(client, jwt, chargeItems, totalSearchMatches, std::move(filters));
        EXPECT_EQ(chargeItems.size(), 0);
    }

    {
        std::vector<std::string> filters = { "entered-date=lt2021-06-06" };
        std::vector<model::ChargeItem> chargeItems;
        JWT jwt = mJwtBuilder.makeJwtVersicherter(InsurantA);
        sendRequest(client, jwt, chargeItems, totalSearchMatches, std::move(filters));
        EXPECT_EQ(chargeItems.size(), 4);
    }

    {
        std::vector<std::string> filters = { "entered-date=gt2021-01-01T17:13:00%2B01:00" };
        std::vector<model::ChargeItem> chargeItems;
        JWT jwt = mJwtBuilder.makeJwtVersicherter(InsurantA);
        sendRequest(client, jwt, chargeItems, totalSearchMatches, std::move(filters));
        EXPECT_EQ(chargeItems.size(), 3);
    }

    {
        std::vector<std::string> filters = { "entered-date=ge2021-01-01T17:13:00%2B01:00" };
        std::vector<model::ChargeItem> chargeItems;
        JWT jwt = mJwtBuilder.makeJwtVersicherter(InsurantA);
        sendRequest(client, jwt, chargeItems, totalSearchMatches, std::move(filters));
        EXPECT_EQ(chargeItems.size(), 4);
    }

    // Pharmacy
    {
        std::vector<std::string> filters = { "entered-date=gt2021-01-02" };
        std::vector<model::ChargeItem> chargeItems;
        JWT jwt = mJwtBuilder.makeJwtApotheke(pharmacyB);
        sendRequest(client, jwt, chargeItems, totalSearchMatches, std::move(filters));
        EXPECT_EQ(chargeItems.size(), 2);
    }

    {
        std::vector<std::string> filters = { "entered-date=eq2021-02-02T17:13:00%2B01:00" };
        std::vector<model::ChargeItem> chargeItems;
        JWT jwt = mJwtBuilder.makeJwtApotheke(pharmacyB);
        sendRequest(client, jwt, chargeItems, totalSearchMatches, std::move(filters));
        EXPECT_EQ(chargeItems.size(), 1);
    }

    {
        std::vector<std::string> filters = { "entered-date=eq2021-02-02T17:13:00%2B01:00" };
        std::vector<model::ChargeItem> chargeItems;
        JWT jwt = mJwtBuilder.makeJwtApotheke(pharmacyA);
        sendRequest(client, jwt, chargeItems, totalSearchMatches, std::move(filters));
        EXPECT_EQ(chargeItems.size(), 0);
    }

    {
        std::vector<std::string> filters = { "entered-date=lt2021-04-04T17:13:00%2B01:00" };
        std::vector<model::ChargeItem> chargeItems;
        JWT jwt = mJwtBuilder.makeJwtApotheke(pharmacyA);
        sendRequest(client, jwt, chargeItems, totalSearchMatches, std::move(filters));
        EXPECT_EQ(chargeItems.size(), 3);
    }

    {
        std::vector<std::string> filters = { "entered-date=le2021-04-04T17:13:00%2B01:00" };
        std::vector<model::ChargeItem> chargeItems;
        JWT jwt = mJwtBuilder.makeJwtApotheke(pharmacyA);
        sendRequest(client, jwt, chargeItems, totalSearchMatches, std::move(filters));
        EXPECT_EQ(chargeItems.size(), 4);
    }

    {
        std::vector<std::string> filters = { "entered-date=gt2021-06-06" };
        std::vector<model::ChargeItem> chargeItems;
        JWT jwt = mJwtBuilder.makeJwtApotheke(pharmacyB);
        sendRequest(client, jwt, chargeItems, totalSearchMatches, std::move(filters));
        EXPECT_EQ(chargeItems.size(), 0);
    }

    // Pharmacy with kvnr filter
    {
        std::vector<std::string> filters = { "entered-date=gt2021-01-02",
                                             "patient=" + std::string(InsurantA) };
        std::vector<model::ChargeItem> chargeItems;
        JWT jwt = mJwtBuilder.makeJwtApotheke(pharmacyB);
        sendRequest(client, jwt, chargeItems, totalSearchMatches, std::move(filters));
        EXPECT_EQ(chargeItems.size(), 1);
    }

    {
        std::vector<std::string> filters = { "entered-date=eq2021-02-02T17:13:00%2B01:00",
                                             "patient=" + std::string(InsurantC) };
        std::vector<model::ChargeItem> chargeItems;
        JWT jwt = mJwtBuilder.makeJwtApotheke(pharmacyB);
        sendRequest(client, jwt, chargeItems, totalSearchMatches, std::move(filters));
        EXPECT_EQ(chargeItems.size(), 0);
    }

    {
        std::vector<std::string> filters = { "entered-date=ge2021-01-01T17:13:30%2B01:00",
                                             "patient=" + std::string(InsurantA) };
        std::vector<model::ChargeItem> chargeItems;
        JWT jwt = mJwtBuilder.makeJwtApotheke(pharmacyA);
        sendRequest(client, jwt, chargeItems, totalSearchMatches, std::move(filters));
        EXPECT_EQ(chargeItems.size(), 2);
    }
}

TEST_F(ChargeItemGetHandlerTest, ChargeItemGetAll_Paging) // NOLINT(readability-function-cognitive-complexity)
{
    // Insert tasks into database
    //---------------------------

    std::string pharmacyA = "606358757";
    std::string pharmacyB = "606358758";
    std::string pharmacyC = "606358759";

    std::vector<std::tuple<std::string, std::string, std::optional<model::Timestamp>>> patientsPharmacies;

    int countInsurantPharmacy = 11;
	patientsPharmacies.reserve(100);
    for (int idxPatient = 0; idxPatient < countInsurantPharmacy; ++idxPatient)
    {
        patientsPharmacies.emplace_back(std::make_tuple(InsurantA, pharmacyA, std::nullopt));
    }

    for (int idxPatient = 0; idxPatient < countInsurantPharmacy; ++idxPatient)
    {
        patientsPharmacies.emplace_back(std::make_tuple(InsurantA, pharmacyB, std::nullopt));
        patientsPharmacies.emplace_back(std::make_tuple(InsurantB, pharmacyA, std::nullopt));
    }

    for (int idxPatient = 0; idxPatient < countInsurantPharmacy; ++idxPatient)
    {
        patientsPharmacies.emplace_back(std::make_tuple(InsurantA, pharmacyC, std::nullopt));
        patientsPharmacies.emplace_back(std::make_tuple(InsurantB, pharmacyB, std::nullopt));
        patientsPharmacies.emplace_back(std::make_tuple(InsurantC, pharmacyA, std::nullopt));
    }

    // insert ChargeItem
    std::vector<std::string> prescriptionIDs;
    insertChargeItem(patientsPharmacies, prescriptionIDs);

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
        sendRequest(client, jwt, chargeItems, totalSearchMatches, {}, paging);
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
        sendRequest(client, jwt, chargeItems, totalSearchMatches, {}, paging);
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
        sendRequest(client, jwt, chargeItems, totalSearchMatches, {}, paging);
        auto remainder = totalSearch - paging.second;
        auto expectedPage = (remainder >= paging.first) ? paging.first : remainder <= 0 ? 0 : remainder;
        ASSERT_EQ(chargeItems.size(), expectedPage);
        ASSERT_EQ(totalSearchMatches, totalSearch);
    }

    // Pharmacy paging
    pagings = {{10, 0}, {10, 27}, {3, 30}, {5, 33}};
    jwt = mJwtBuilder.makeJwtApotheke(pharmacyA);
    totalSearch = 33;
    for(const auto& paging : pagings)
    {
        std::vector<model::ChargeItem> chargeItems;
        sendRequest(client, jwt, chargeItems, totalSearchMatches, {}, paging);
        auto remainder = totalSearch - paging.second;
        auto expectedPage = (remainder >= paging.first) ? paging.first : remainder <= 0 ? 0 : remainder;
        ASSERT_EQ(chargeItems.size(), expectedPage);
        ASSERT_EQ(totalSearchMatches, totalSearch);
    }
}

TEST_F(ChargeItemGetHandlerTest, ChargeItemGetAll_FiltersPaging) // NOLINT(readability-function-cognitive-complexity)
{
    // Insert tasks into database
    //---------------------------

    std::string pharmacyA = "606358757";
    std::string pharmacyB = "606358758";
    std::string pharmacyC = "606358759";

    std::vector<std::tuple<std::string, std::string, std::optional<model::Timestamp>>> patientsPharmacies;

    model::Timestamp timestamp = model::Timestamp::fromXsDateTime("2021-09-25T12:34:56+01:00");
    int countInsurantPharmacy = 11;
	patientsPharmacies.reserve(100);
    for (int idxPatient = 0; idxPatient < countInsurantPharmacy; ++idxPatient)
    {
        patientsPharmacies.emplace_back(std::make_tuple(InsurantA, pharmacyA, timestamp + std::chrono::hours(-24 * idxPatient)));
    }

    timestamp = model::Timestamp::fromXsDateTime("2021-10-25T12:34:56+01:00");
    for (int idxPatient = 0; idxPatient < countInsurantPharmacy; ++idxPatient)
    {
        patientsPharmacies.emplace_back(std::make_tuple(InsurantA, pharmacyB, timestamp + std::chrono::hours(-24 * idxPatient)));
        patientsPharmacies.emplace_back(std::make_tuple(InsurantB, pharmacyA, timestamp + std::chrono::hours(-24 * idxPatient)));
    }

    timestamp = model::Timestamp::fromXsDateTime("2021-11-25T12:34:56+01:00");
    for (int idxPatient = 0; idxPatient < countInsurantPharmacy; ++idxPatient)
    {
        patientsPharmacies.emplace_back(std::make_tuple(InsurantA, pharmacyC, timestamp + std::chrono::hours(-24 * idxPatient)));
        patientsPharmacies.emplace_back(std::make_tuple(InsurantB, pharmacyB, timestamp + std::chrono::hours(-24 * idxPatient)));
        patientsPharmacies.emplace_back(std::make_tuple(InsurantC, pharmacyA, timestamp + std::chrono::hours(-24 * idxPatient)));
    }

    // insert ChargeItem
    std::vector<std::string> prescriptionIDs;
    insertChargeItem(patientsPharmacies, prescriptionIDs);

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
        sendRequest(client, jwt, chargeItems, totalSearchMatches, filters, paging);
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
        sendRequest(client, jwt, chargeItems, totalSearchMatches, {}, paging);
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
        sendRequest(client, jwt, chargeItems, totalSearchMatches, {}, paging);
        auto remainder = totalSearch - std::get<1>(paging);
        auto expectedPage = (remainder >= std::get<0>(paging)) ? std::get<0>(paging) : remainder <= 0 ? 0 : remainder;
        ASSERT_EQ(chargeItems.size(), expectedPage);
        ASSERT_EQ(totalSearchMatches, totalSearch);
    }

    // Pharmacy paging
    pagings = {{10, 0}, {10, 1}, {3, 5}, {5, 11}};
    jwt = mJwtBuilder.makeJwtApotheke(pharmacyA);
    filters = { "entered-date=lt2021-10-25T12:34:56%2b01:00", "patient=" + std::string(InsurantB) };
    totalSearch = 10;
    for(const auto& paging : pagings)
    {
        std::vector<model::ChargeItem> chargeItems;
        sendRequest(client, jwt, chargeItems, totalSearchMatches, filters, paging);
        auto remainder = totalSearch - paging.second;
        auto expectedPage = (remainder >= paging.first) ? paging.first : remainder <= 0 ? 0 : remainder;
        ASSERT_EQ(chargeItems.size(), expectedPage);
        ASSERT_EQ(totalSearchMatches, totalSearch);
    }
}

TEST_F(ChargeItemGetHandlerTest, ChargeItemGetAll_PagingLinks) // NOLINT(readability-function-cognitive-complexity)
{
    // Insert tasks into database
    //---------------------------

    std::string pharmacyA = "606358757";
    std::string pharmacyB = "606358758";
    std::string pharmacyC = "606358759";

    std::vector<std::tuple<std::string, std::string, std::optional<model::Timestamp>>> patientsPharmacies;

    model::Timestamp timestamp = model::Timestamp::fromXsDateTime("2021-09-25T12:34:56+01:00");
    int countInsurantPharmacy = 11;
	patientsPharmacies.reserve(100);
    for (int idxPatient = 0; idxPatient < countInsurantPharmacy; ++idxPatient)
    {
        patientsPharmacies.emplace_back(std::make_tuple(InsurantA, pharmacyA, timestamp + std::chrono::hours(-24 * idxPatient)));
    }

    timestamp = model::Timestamp::fromXsDateTime("2021-10-25T12:34:56+01:00");
    for (int idxPatient = 0; idxPatient < countInsurantPharmacy; ++idxPatient)
    {
        patientsPharmacies.emplace_back(std::make_tuple(InsurantA, pharmacyB, timestamp + std::chrono::hours(-24 * idxPatient)));
        patientsPharmacies.emplace_back(std::make_tuple(InsurantB, pharmacyA, timestamp + std::chrono::hours(-24 * idxPatient)));
    }

    timestamp = model::Timestamp::fromXsDateTime("2021-11-25T12:34:56+01:00");
    for (int idxPatient = 0; idxPatient < countInsurantPharmacy; ++idxPatient)
    {
        patientsPharmacies.emplace_back(std::make_tuple(InsurantA, pharmacyC, timestamp + std::chrono::hours(-24 * idxPatient)));
        patientsPharmacies.emplace_back(std::make_tuple(InsurantB, pharmacyB, timestamp + std::chrono::hours(-24 * idxPatient)));
        patientsPharmacies.emplace_back(std::make_tuple(InsurantC, pharmacyA, timestamp + std::chrono::hours(-24 * idxPatient)));
    }

    // insert ChargeItem
    std::vector<std::string> prescriptionIDs;
    insertChargeItem(patientsPharmacies, prescriptionIDs);

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
            bundle = sendRequest(client, jwt, totalSearchMatches, argument);
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
        bundle = sendRequest(client, jwt, totalSearchMatches, argument);
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
            bundle = sendRequest(client, jwt, totalSearchMatches, argument);
            EXPECT_EQ(totalSearchMatches, totalSearch);
            nextLink = bundle->getLink(model::Link::Type::Next);
            EXPECT_TRUE(nextLink.has_value());
            argument = extractPathAndArguments(nextLink.value());
            EXPECT_EQ(argument,
                      "/ChargeItem?entered-date=le2021-11-25T11%3a34%3a56%2b00%3a00&_count=" + std::to_string(paging.first) +
                          "&__offset="+ std::to_string(((idx+1) * paging.first) + paging.second));
            EXPECT_EQ(UrlHelper::unescapeUrl(argument),
                      "/ChargeItem?entered-date=le2021-11-25T11:34:56+00:00&_count=" + std::to_string(paging.first) +
                          "&__offset="+ std::to_string(((idx+1) * paging.first) + paging.second));
        }
        bundle = sendRequest(client, jwt, totalSearchMatches, argument);
        EXPECT_EQ(totalSearchMatches, totalSearch);
        nextLink = bundle->getLink(model::Link::Type::Next);
        EXPECT_FALSE(nextLink.has_value());
    }

    // Pharmacy paging
    pagings = {{10, 0}, {10, 1}, {3, 5}, {5, 11}};
    jwt = mJwtBuilder.makeJwtApotheke(pharmacyA);
    totalSearch = 11;
    for(const auto& paging : pagings)
    {
        int hasLinkCount = (totalSearch - paging.second)  / paging.first;
        hasLinkCount -= ((totalSearch - paging.second) % paging.first) == 0 ? 1 : 0;
        std::string argument = { "/ChargeItem?entered-date=le2021-10-25T12%3a34%3a56%2B01%3a00&patient=" +
                                std::string(InsurantB) + "&_count=" + std::to_string(paging.first) +
                                "&__offset=" + std::to_string(paging.second)};
        std::optional<model::Bundle> bundle = {};
        std::optional<std::string> nextLink = {};
        for (int idx = 0; idx < hasLinkCount; ++idx)
        {
            bundle = sendRequest(client, jwt, totalSearchMatches, argument);
            EXPECT_EQ(totalSearchMatches, totalSearch);
            nextLink = bundle->getLink(model::Link::Type::Next);
            EXPECT_TRUE(nextLink.has_value());
            argument = extractPathAndArguments(nextLink.value());
            EXPECT_EQ(argument,
                      "/ChargeItem?entered-date=le2021-10-25T11%3a34%3a56%2b00%3a00&patient=" +
                          std::string(InsurantB) + "&_count=" + std::to_string(paging.first) +
                          "&__offset="+ std::to_string(((idx+1) * paging.first) + paging.second));
            EXPECT_EQ(UrlHelper::unescapeUrl(argument),
                      "/ChargeItem?entered-date=le2021-10-25T11:34:56+00:00&patient=" +
                          std::string(InsurantB) + "&_count=" + std::to_string(paging.first) +
                          "&__offset="+ std::to_string(((idx+1) * paging.first) + paging.second));
        }
        bundle = sendRequest(client, jwt, totalSearchMatches, argument);
        EXPECT_EQ(totalSearchMatches, totalSearch);
        nextLink = bundle->getLink(model::Link::Type::Next);
        EXPECT_FALSE(nextLink.has_value());
    }
}

TEST_F(ChargeItemGetHandlerTest, ChargeItemGet)//NOLINT(readability-function-cognitive-complexity)
{
    // Insert chargeItem into database
    //--------------------------

    std::string pharmacyA = "606358757";
    std::string pharmacyB = "606358758";

    std::vector<std::tuple<std::string, std::string, std::optional<model::Timestamp>>> patientsPharmaciesChargeItems = {
        {InsurantA, pharmacyA, std::nullopt},
        {InsurantB, pharmacyB, std::nullopt},
    };

    std::vector<std::string> prescriptionIDs;
    insertChargeItem(patientsPharmaciesChargeItems, prescriptionIDs, MockDatabase::mockAccessCode);

    // GET ChargeItems
    //-------------------------

    // Create a client
    HttpsClient client = createClient();
    std::optional<model::Bundle> chargeItemBundle;

    // PharmacyA, InsurantA
    {
        std::string argument = {"/ChargeItem/" + prescriptionIDs[0]};
        JWT jwt = mJwtBuilder.makeJwtApotheke(pharmacyA);
        sendRequest(client, jwt, argument, chargeItemBundle, HttpStatus::Forbidden);
        EXPECT_FALSE(chargeItemBundle.has_value());

        argument += std::string{"?ac="} + MockDatabase::mockAccessCode.data();
        sendRequest(client, jwt, argument, chargeItemBundle);
        EXPECT_TRUE(chargeItemBundle.has_value());

        auto chargeItem = chargeItemBundle->getResourcesByType<model::ChargeItem>("ChargeItem");
        ASSERT_EQ(chargeItem.size(), 1);

        A_22125.test("Insurant check");
        EXPECT_EQ(chargeItem[0].subjectKvnr(), InsurantA);

        A_22126.test("Telematik-ID check");
        EXPECT_EQ(chargeItem[0].entererTelematikId(), pharmacyA);

        A_22128.test("Check ChargeItem.supportingInformation");
        auto dispenseItem = chargeItemBundle->getResourcesByType<model::Bundle>("Bundle");
        ASSERT_EQ(dispenseItem.size(), 1);

        EXPECT_TRUE(
            chargeItem[0].supportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItem).has_value());
        ASSERT_EQ(dispenseItem[0].getId().toString(),
                  chargeItem[0].supportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItem).value());
    }

    // PharmacyB, InsurantB
    {
        std::string argument = {"/ChargeItem/" + prescriptionIDs[1]};
        JWT jwt = mJwtBuilder.makeJwtApotheke(pharmacyB);
        chargeItemBundle.reset();
        sendRequest(client, jwt, argument, chargeItemBundle, HttpStatus::Forbidden);
        EXPECT_FALSE(chargeItemBundle.has_value());

        argument += std::string{"?ac="} + MockDatabase::mockAccessCode.data();
        sendRequest(client, jwt, argument, chargeItemBundle);
        EXPECT_TRUE(chargeItemBundle.has_value());

        auto chargeItem = chargeItemBundle->getResourcesByType<model::ChargeItem>("ChargeItem");
        ASSERT_EQ(chargeItem.size(), 1);

        A_22125.test("Insurant check");
        EXPECT_EQ(chargeItem[0].subjectKvnr(), InsurantB);

        A_22126.test("Telematik-ID check");
        EXPECT_EQ(chargeItem[0].entererTelematikId(), pharmacyB);

        A_22128.test("Check ChargeItem.supportingInformation");
        auto dispenseItem = chargeItemBundle->getResourcesByType<model::Bundle>("Bundle");
        ASSERT_EQ(dispenseItem.size(), 1);

        EXPECT_TRUE(chargeItem[0].supportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItem).has_value());
        ASSERT_EQ(dispenseItem[0].getId().toString(),
            chargeItem[0].supportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItem).value());
    }
}

TEST_F(ChargeItemGetHandlerTest, ChargeItemGet_BAD)
{
    // Insert chargeItem into database
    //--------------------------

    std::string pharmacyA = "606358757";
    std::string pharmacyB = "606358758";

    std::vector<std::tuple<std::string, std::string, std::optional<model::Timestamp>>> patientsPharmaciesChargeItems = {
        {InsurantA, pharmacyA, std::nullopt},
        {InsurantA, pharmacyB, std::nullopt},
    };

    std::vector<std::string> prescriptionIDs;
    insertChargeItem(patientsPharmaciesChargeItems, prescriptionIDs);

    // GET ChargeItems
    //-------------------------

    // Create a client
    HttpsClient client = createClient();
    std::optional<model::Bundle> chargeItemBundle;

    // Without id
    {
        std::string argument = {"/ChargeItem/"};
        JWT jwt = mJwtBuilder.makeJwtArzt(pharmacyA);
        sendRequest(client, jwt, argument, chargeItemBundle, HttpStatus::Forbidden);
        EXPECT_FALSE(chargeItemBundle.has_value());
    }

    // PharmacyA (prescriptionID[1] is InsurantA, pharmacyB)
    {
        std::string argument = {"/ChargeItem/" + prescriptionIDs[1]};
        JWT jwt = mJwtBuilder.makeJwtApotheke(pharmacyA);
        sendRequest(client, jwt, argument, chargeItemBundle, HttpStatus::Forbidden);
        EXPECT_FALSE(chargeItemBundle.has_value());
    }

    // PharmacyB (prescriptionID[0] is InsurantA, pharmacyA)
    {
        std::string argument = {"/ChargeItem/" + prescriptionIDs[0]};
        JWT jwt = mJwtBuilder.makeJwtApotheke(pharmacyB);
        sendRequest(client, jwt, argument, chargeItemBundle, HttpStatus::Forbidden);
        EXPECT_FALSE(chargeItemBundle.has_value());
    }
}
