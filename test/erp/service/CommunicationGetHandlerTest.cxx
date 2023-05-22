/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/service/CommunicationGetHandler.hxx"

#include "erp/crypto/Certificate.hxx"
#include "erp/database/DatabaseModel.hxx"
#include "erp/database/PostgresBackend.hxx"
#include "erp/util/FileHelper.hxx"
#include "test/erp/model/CommunicationTest.hxx"
#include "test/mock/ClientTeeProtocol.hxx"
#include "test/util/JsonTestUtils.hxx"
#include "test/util/ServerTestBaseAutoCleanup.hxx"
#include "test/util/StaticData.hxx"

#include <pqxx/connection>
#include <pqxx/pqxx>


class CommunicationGetHandlerTest : public ServerTestBaseAutoCleanup
{
public:
    CommunicationGetHandlerTest (void)
        : ServerTestBaseAutoCleanup(),
          mIsInitialCleanupRequired(mHasPostgresSupport)
    {
    }

    void SetUp (void) override
    {
        ASSERT_NO_FATAL_FAILURE(ServerTestBase::SetUp());

        if (mIsInitialCleanupRequired && mHasPostgresSupport)
        {
            mIsInitialCleanupRequired = false;

            auto connection = std::make_unique<pqxx::connection>(PostgresBackend::defaultConnectString());

            // There is no predefined method for this. Run an adhoc SQL query.
            for (const std::string user : {InsurantF, InsurantG, InsurantH})
            {
                auto transaction = pqxx::work(*connection);
                auto kvnrHashed = mServer->serviceContext().getKeyDerivation().hashKvnr(model::Kvnr{user});
                transaction.exec_params0("DELETE FROM erp.communication WHERE sender = $1 OR recipient = $1",
                                         kvnrHashed.binarystring());
                transaction.commit();
            }
        }
    }

    //NOLINTNEXTLINE(readability-function-cognitive-complexity)
    std::optional<model::Bundle> expectGetCommunicationResponse (
        const ClientResponse& outerResponse,
        std::vector<Uuid> expectedCommunicationIds,
        bool requireOrder = true,
        HttpStatus expectedHttpStatus = HttpStatus::OK,
        const std::string& expectedMimeType = ContentMimeType::fhirJsonUtf8)
    {
        if (!requireOrder)
        {
            std::sort(expectedCommunicationIds.begin(), expectedCommunicationIds.end());
        }
        // Verify and decrypt the outer response. Also the generic part of the inner response.
        auto innerResponse = verifyOuterResponse(outerResponse);
        verifyGenericInnerResponse(innerResponse, expectedHttpStatus, expectedMimeType);

        // Verify the returned bundle.
        if (expectedHttpStatus == HttpStatus::OK)
        {
            model::Bundle bundle(model::BundleType::searchset, ::model::ResourceBase::NoProfile);
            if (expectedMimeType == std::string(ContentMimeType::fhirJsonUtf8))
            {
                bundle = model::Bundle::fromJsonNoValidation(innerResponse.getBody());
            }
            else
            {
                bundle = model::Bundle::fromXmlNoValidation(innerResponse.getBody());
            }
            if (bundle.getResourceCount() != expectedCommunicationIds.size())
            {
                EXPECT_EQ(bundle.getResourceCount(), expectedCommunicationIds.size());
            }
            else
            {
                std::vector<model::Communication> communications;
                for (size_t index = 0; index < bundle.getResourceCount(); ++index)
                {
                    communications.emplace_back(model::Communication::fromJson(bundle.getResource(index)));
                }
                if (!requireOrder)
                {
                    std::sort(communications.begin(), communications.end(),
                            [](const model::Communication& lhs, const model::Communication& rhs){
                                return lhs.id() < rhs.id();
                            });
                }
                for (size_t index=0; index<expectedCommunicationIds.size(); ++index)
                {
                    const auto& communication = communications.at(index);
                    const auto messageType = communication.messageType();
                    auto currentGematik = model::ResourceVersion::current<model::ResourceVersion::DeGematikErezeptWorkflowR4>();
                    bool allowGenericValidation = model::Communication::canValidateGeneric(messageType, currentGematik);
                    EXPECT_NO_THROW((void)model::Communication::fromXml(
                        communication.serializeToXmlString(), *StaticData::getXmlValidator(),
                        *StaticData::getInCodeValidator(),
                        model::Communication::messageTypeToSchemaType(messageType),
                        model::ResourceVersion::supportedBundles(), allowGenericValidation));
                    EXPECT_EQ(communication.id().value().toString(), expectedCommunicationIds[index].toString());
                }
            }
            return bundle;
        }
        return {};
    }

    /**
     * Helper function aimed at verifying link URLs from a REST response.
     */
    std::string extractPathAndArguments (const std::string_view& url)
    {
        const auto pathStart = url.find("/Communication");
        if (pathStart == std::string::npos)
            return std::string(url);
        else
            return std::string(url.substr(pathStart));
    }

    std::optional<model::Bundle> getBundle (const ClientResponse& outerResponse,
                                           const std::string& expectedMimeType = ContentMimeType::fhirJsonUtf8)
    {
        // Verify and decrypt the outer response. Also the generic part of the inner response.
        auto innerResponse = verifyOuterResponse(outerResponse);
        verifyGenericInnerResponse(innerResponse, HttpStatus::OK, expectedMimeType);

        // Verify the returned bundle.
        model::Bundle bundle(model::BundleType::searchset, ::model::ResourceBase::NoProfile);
        if (expectedMimeType == std::string(ContentMimeType::fhirJsonUtf8))
        {
            bundle = model::Bundle::fromJsonNoValidation(innerResponse.getBody());
        }
        else
        {
            bundle = model::Bundle::fromXmlNoValidation(innerResponse.getBody());
        }
        return bundle;
    }

private:
    bool mIsInitialCleanupRequired;
};


TEST_F(CommunicationGetHandlerTest, getAllCommunications_noFilter)
{
    // Setup the database.
    const auto givenTask = addTaskToDatabase({model::Task::Status::ready, InsurantE});
    const auto givenCommunication = addCommunicationToDatabase({
        givenTask.prescriptionId(),
        model::Communication::MessageType::Representative,
        {ActorRole::Insurant, InsurantE},
        {ActorRole::Representative, InsurantF},
        std::string(givenTask.accessCode()),
        RepresentativeMessageByInsurant,
        model::Timestamp::now()});

    // Create the inner request
    const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(InsurantE) };
    ClientRequest request(
        createGetHeader("/Communication", jwtInsurant),
        "");

    // Send the request.
    auto client = createClient();
    auto outerResponse = client.send(encryptRequest(request, jwtInsurant));

    // Verify and decrypt the outer response. Also the generic part of the inner response.
    auto innerResponse = verifyOuterResponse(outerResponse);
    verifyGenericInnerResponse(innerResponse);

    // Verify the returned bundle.
    const auto bundle = model::Bundle::fromJsonNoValidation(innerResponse.getBody());
    ASSERT_EQ(bundle.getResourceCount(), 1);
    const auto communication = model::Communication::fromJson(bundle.getResource(0));

    EXPECT_NO_THROW((void)model::Communication::fromXml(
        communication.serializeToXmlString(), *StaticData::getXmlValidator(), *StaticData::getInCodeValidator(),
        model::Communication::messageTypeToSchemaType(communication.messageType())));

    ASSERT_EQ(communication.id(), givenCommunication.id());
}


// GEMREQ-start A_19520-01#getAllCommunications_filterByRecipient
TEST_F(CommunicationGetHandlerTest, getAllCommunications_filterByRecipient)
{
    // Setup the database.
    const auto givenTask = addTaskToDatabase({model::Task::Status::ready, InsurantE});
    const auto givenCommunication1 = addCommunicationToDatabase({
        givenTask.prescriptionId(), model::Communication::MessageType::Representative,
        {ActorRole::Insurant, InsurantE}, {ActorRole::Representative, InsurantF},
        std::string(givenTask.accessCode()),
        RepresentativeMessageByInsurant, model::Timestamp::fromXsDateTime("2022-01-23T12:34:56Z") });
    const auto givenCommunication2 = addCommunicationToDatabase({
        givenTask.prescriptionId(), model::Communication::MessageType::Representative,
        {ActorRole::Insurant, InsurantE}, {ActorRole::Representative, InsurantG},
        std::string(givenTask.accessCode()),
        RepresentativeMessageByInsurant, model::Timestamp::fromXsDateTime("2022-01-23T12:34:00Z") });
    const auto givenCommunication3 = addCommunicationToDatabase({
        givenTask.prescriptionId(), model::Communication::MessageType::Representative,
        {ActorRole::Insurant, InsurantG}, {ActorRole::Representative, InsurantE},
        std::string(givenTask.accessCode()),
        RepresentativeMessageByRepresentative, model::Timestamp::fromXsDateTime("2022-01-23T12:34:00Z") });

    // Create the inner request
    const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(InsurantF) };
    ClientRequest request(
        createGetHeader("/Communication", jwtInsurant),
        "");

    // Send the request (as InsurantF).
    auto client = createClient();
    auto outerResponse = client.send(encryptRequest(request, jwtInsurant));

    // Verify and decrypt the outer response. Also the generic part of the inner response.
    auto innerResponse = verifyOuterResponse(outerResponse);
    verifyGenericInnerResponse(innerResponse);

    // Verify the returned bundle. Of the two Communication objects in the DB, only one is expected, based on the recipient.
    const auto bundle = model::Bundle::fromJsonNoValidation(innerResponse.getBody());
    ASSERT_EQ(bundle.getResourceCount(), 1);
    const auto communication = model::Communication::fromJson(bundle.getResource(0));

    EXPECT_NO_THROW((void)model::Communication::fromXml(
        communication.serializeToXmlString(), *StaticData::getXmlValidator(), *StaticData::getInCodeValidator(),
        model::Communication::messageTypeToSchemaType(communication.messageType())));

    ASSERT_EQ(communication.id(), givenCommunication1.id());
}
// GEMREQ-end A_19520-01#getAllCommunications_filterByRecipient


// GEMREQ-start A_19520-01#getAllCommunications_filterBySender
TEST_F(CommunicationGetHandlerTest, getAllCommunications_filterBySender)//NOLINT(readability-function-cognitive-complexity)
{
    // Setup the database.
    const auto givenTask = addTaskToDatabase({model::Task::Status::ready, InsurantE});
    const auto givenCommunication1 = addCommunicationToDatabase({
        givenTask.prescriptionId(), model::Communication::MessageType::Representative,
        {ActorRole::Insurant, InsurantF}, {ActorRole::Representative, InsurantE},
        std::string(givenTask.accessCode()),
        RepresentativeMessageByRepresentative, model::Timestamp::fromXsDateTime("2022-01-23T12:34:56Z")});
    const auto givenCommunication2 = addCommunicationToDatabase({
        givenTask.prescriptionId(), model::Communication::MessageType::Representative,
        {ActorRole::Insurant, InsurantG}, {ActorRole::Representative, InsurantE},
        std::string(givenTask.accessCode()),
        RepresentativeMessageByRepresentative, model::Timestamp::fromXsDateTime("2022-01-23T12:34:00Z")});
    const auto givenCommunication3 = addCommunicationToDatabase({
        givenTask.prescriptionId(), model::Communication::MessageType::Representative,
        {ActorRole::Insurant, InsurantF}, {ActorRole::Representative, InsurantE},
        std::string(givenTask.accessCode()),
        RepresentativeMessageByRepresentative, model::Timestamp::fromXsDateTime("2022-01-23T12:34:00Z") });

    // Create the inner request (as InsurantF)
    const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(std::string(InsurantF)) };
    ClientRequest request(
        createGetHeader("/Communication", jwtInsurant),
        "");

    // Send the request.
    auto client = createClient();
    auto outerResponse = client.send(encryptRequest(request, jwtInsurant));

    // Verify and decrypt the outer response. Also the generic part of the inner response.
    auto innerResponse = verifyOuterResponse(outerResponse);
    verifyGenericInnerResponse(innerResponse);

    // Verify the returned bundle. Of the two Communication objects in the DB, only one is expected, based on the recipient.
    const auto bundle = model::Bundle::fromJsonNoValidation(innerResponse.getBody());
    ASSERT_EQ(bundle.getResourceCount(), 2);
    const auto communication1 = model::Communication::fromJson(bundle.getResource(0));
    const auto communication2 = model::Communication::fromJson(bundle.getResource(1));

    EXPECT_NO_THROW((void)model::Communication::fromXml(communication1.serializeToXmlString(), *StaticData::getXmlValidator(),
                                                  *StaticData::getInCodeValidator(),
                                                  SchemaType::Gem_erxCommunicationRepresentative));
    EXPECT_NO_THROW((void)model::Communication::fromXml(communication2.serializeToXmlString(), *StaticData::getXmlValidator(),
                                                  *StaticData::getInCodeValidator(),
                                                  SchemaType::Gem_erxCommunicationRepresentative));

    ASSERT_TRUE(communication1.id() == givenCommunication1.id() || communication1.id() == givenCommunication3.id());
    ASSERT_TRUE(communication2.id() == givenCommunication1.id() || communication2.id() == givenCommunication3.id());
}
// GEMREQ-end A_19520-01#getAllCommunications_filterBySender


TEST_F(CommunicationGetHandlerTest, getAllCommunications_searchBySent)
{
    // Setup the database.
    const auto givenTask = addTaskToDatabase({model::Task::Status::draft, InsurantF});
    const auto givenCommunication1 = addCommunicationToDatabase({
        givenTask.prescriptionId(), model::Communication::MessageType::Representative,
        {ActorRole::Insurant, InsurantF}, {ActorRole::Representative, InsurantG},
        std::string(givenTask.accessCode()),
        RepresentativeMessageByInsurant, model::Timestamp::fromXsDateTime("2022-01-23T12:34:56Z") });
    const auto givenCommunication2 = addCommunicationToDatabase({
        givenTask.prescriptionId(), model::Communication::MessageType::Representative,
        {ActorRole::Insurant, InsurantF}, {ActorRole::Representative, InsurantG},
        std::string(givenTask.accessCode()),
        RepresentativeMessageByInsurant, model::Timestamp::fromXsDateTime("2022-01-23T12:34:00Z") });

    // Send the request.
    const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(InsurantF) };
    auto outerResponse = createClient().send(encryptRequest(
        ClientRequest(
            createGetHeader("/Communication?sent=2022-01-23T12%3A34:00-00:00", jwtInsurant),
            //                              ^ missing prefix defaults to eq:
            //                                                ^ quoted colon
            //                                                      ^ 00 seconds matches second communication
            //                                                         ^ -00:00 timezone is expected to match Z(ulu)
            ""), jwtInsurant));

    // Verify the response.
    const auto bundle = expectGetCommunicationResponse(outerResponse, {givenCommunication2.id().value()});
    const auto selfLink = bundle->getLink(model::Link::Type::Self);
    EXPECT_TRUE(selfLink.has_value());
    EXPECT_EQ(UrlHelper::unescapeUrl(extractPathAndArguments(selfLink.value())), "/Communication?sent=eq2022-01-23T12:34:00+00:00");
}


TEST_F(CommunicationGetHandlerTest, getAllCommunications_searchByReceived)
{
    const auto givenTask = addTaskToDatabase({model::Task::Status::draft, InsurantF});

    // Received at 2022-01-23 01:00, local timezone, i.e UTC = two hours earlier -> previous day 2022-01-22
    const auto givenCommunication1 =
        addCommunicationToDatabase({givenTask.prescriptionId(),
                                    model::Communication::MessageType::Representative,
                                    {ActorRole::Insurant, InsurantF},
                                    {ActorRole::Representative, InsurantG},
                                    std::string(givenTask.accessCode()),
                                    RepresentativeMessageByInsurant,
                                    model::Timestamp::fromXsDateTime("2022-01-22T12:00:00+02:00"),
                                    model::Timestamp::fromXsDateTime("2022-01-23T01:00:00+02:00")});

    {
        // search for 2022-01-23, german time, should be found
        auto outerResponse = createClient().send(
            encryptRequest(ClientRequest(createGetHeader("/Communication?received=2022-01-23"), "")));

        // Verify the response.
        const auto bundle = expectGetCommunicationResponse(outerResponse, {givenCommunication1.id().value()});
        const auto selfLink = bundle->getLink(model::Link::Type::Self);
        EXPECT_TRUE(selfLink.has_value());
        EXPECT_EQ(extractPathAndArguments(selfLink.value()), "/Communication?received=eq2022-01-23");
    }

    {
        // search for 2022-01-22, german time should NOT be found
        auto outerResponse = createClient().send(
            encryptRequest(ClientRequest(createGetHeader("/Communication?received=2022-01-22"), "")));

        // Verify the response.
        const auto bundle = expectGetCommunicationResponse(outerResponse, {});
        const auto selfLink = bundle->getLink(model::Link::Type::Self);
        EXPECT_TRUE(selfLink.has_value());
        EXPECT_EQ(extractPathAndArguments(selfLink.value()), "/Communication?received=eq2022-01-22");
    }
}


TEST_F(CommunicationGetHandlerTest, getAllCommunications_searchBySentAndReceived)
{
    // Setup the database.
    const auto givenTask = addTaskToDatabase({ model::Task::Status::draft, InsurantF });
    const auto givenCommunication1 =
        addCommunicationToDatabase({givenTask.prescriptionId(),
                                    model::Communication::MessageType::Representative,
                                    {ActorRole::Insurant, InsurantF},
                                    {ActorRole::Representative, InsurantG},
                                    std::string(givenTask.accessCode()),
                                    RepresentativeMessageByInsurant,
                                    model::Timestamp::fromXsDateTime("2022-01-23T12:34:56Z"),
                                    model::Timestamp::fromXsDate("2022-01-03", model::Timestamp::UTCTimezone)});
    const auto givenCommunication2 =
        addCommunicationToDatabase({givenTask.prescriptionId(),
                                    model::Communication::MessageType::Representative,
                                    {ActorRole::Insurant, InsurantF},
                                    {ActorRole::Representative, InsurantG},
                                    std::string(givenTask.accessCode()),
                                    RepresentativeMessageByInsurant,
                                    model::Timestamp::fromXsDate("2022-01-02", model::Timestamp::UTCTimezone),
                                    model::Timestamp::fromXsDate("2022-01-04", model::Timestamp::UTCTimezone)});
    const auto givenCommunication3 = addCommunicationToDatabase({
        givenTask.prescriptionId(), model::Communication::MessageType::Representative,
        {ActorRole::Insurant, InsurantF}, {ActorRole::Representative, InsurantG},
        std::string(givenTask.accessCode()),
        RepresentativeMessageByInsurant, model::Timestamp::fromXsDate("2022-01-02", model::Timestamp::UTCTimezone) });

    // Send the request.
    const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(InsurantF) };
    auto outerResponse = createClient().send(encryptRequest(
        ClientRequest(
            createGetHeader("/Communication?sent=ge2022-01-01&received=le2022-01-04&ignored=also-ignored", jwtInsurant),
            //                                              ^ missing time means that 'sent' becomes a range
            //                                                                    ^ same for 'received'
            //                                                                        ^ unsupported argument is ignored
            ""), jwtInsurant));

    // Verify the response.
    const auto bundle =
        expectGetCommunicationResponse(outerResponse, {givenCommunication1.id().value(), givenCommunication2.id().value()}, false);
    const auto selfLink = bundle->getLink(model::Link::Type::Self);
    EXPECT_TRUE(selfLink.has_value());
    EXPECT_EQ(extractPathAndArguments(selfLink.value()), "/Communication?sent=ge2022-01-01&received=le2022-01-04");
}


TEST_F(CommunicationGetHandlerTest, getAllCommunications_searchByReceivedTimeRange) // Created for ticket ERP-9253
{
    // Setup the database.
    const auto givenTask = addTaskToDatabase({ model::Task::Status::draft, InsurantF });
    const auto givenCommunication1 = addCommunicationToDatabase({
        givenTask.prescriptionId(), model::Communication::MessageType::Representative,
        {ActorRole::Insurant, InsurantF}, {ActorRole::Representative, InsurantG},
        std::string(givenTask.accessCode()),
        RepresentativeMessageByInsurant,
        model::Timestamp::fromXsDateTime("2022-02-28T23:59:56+01:00"), model::Timestamp::fromXsDateTime("2022-02-28T23:59:59+01:00") });// not found
    const auto givenCommunication2 = addCommunicationToDatabase({
        givenTask.prescriptionId(), model::Communication::MessageType::Representative,
        {ActorRole::Insurant, InsurantF}, {ActorRole::Representative, InsurantG},
        std::string(givenTask.accessCode()),
        RepresentativeMessageByInsurant,
        model::Timestamp::fromXsDateTime("2022-03-01T12:34:56+01:00"), model::Timestamp::fromXsDateTime("2022-03-01T12:35:00+01:00") });// found
    const auto givenCommunication3 = addCommunicationToDatabase({
        givenTask.prescriptionId(), model::Communication::MessageType::Representative,
        {ActorRole::Insurant, InsurantF}, {ActorRole::Representative, InsurantG},
        std::string(givenTask.accessCode()),
        RepresentativeMessageByInsurant,
        model::Timestamp::fromXsDateTime("2022-02-28T23:59:59+01:00"), model::Timestamp::fromXsDateTime("2022-03-01T00:00:00+01:00") });// found
    const auto givenCommunication4 = addCommunicationToDatabase({
        givenTask.prescriptionId(), model::Communication::MessageType::Representative,
        {ActorRole::Insurant, InsurantF}, {ActorRole::Representative, InsurantG},
        std::string(givenTask.accessCode()),
        RepresentativeMessageByInsurant,
        model::Timestamp::fromXsDateTime("2022-03-01T23:59:56+01:00"), model::Timestamp::fromXsDateTime("2022-03-02T00:00:00+01:00") });// not found

    // Send the request.
    const auto jwtInsurant = mJwtBuilder.makeJwtVersicherter(InsurantF);
    const char* const urlPath = "/Communication?received=gt2022-02-28&received=lt2022-03-02&_sort=received";
    const auto outerResponse = createClient().send(
        encryptRequest(ClientRequest(createGetHeader(urlPath, jwtInsurant), ""/*body*/), jwtInsurant));

    // Verify the response.
    const auto bundle = expectGetCommunicationResponse(
        outerResponse, {givenCommunication3.id().value(), givenCommunication2.id().value()}, true);
    const auto selfLink = bundle->getLink(model::Link::Type::Self);
    EXPECT_TRUE(selfLink.has_value());
    EXPECT_EQ(extractPathAndArguments(selfLink.value()), urlPath);
}


TEST_F(CommunicationGetHandlerTest, getAllCommunications_searchBySender)
{
    // Setup the database.
    const auto givenTask = addTaskToDatabase({ model::Task::Status::ready, InsurantF });

    const std::string kvnrInsurant = givenTask.kvnr().value().id();
    const std::string pharmacyA = "3-SMC-B-Testkarte-883110000129068";
    const std::string pharmacyB = "3-SMC-B-Testkarte-883110000129069";

    const auto givenCommunication1 = addCommunicationToDatabase({
        givenTask.prescriptionId(), model::Communication::MessageType::InfoReq,
        {ActorRole::Insurant, kvnrInsurant}, {ActorRole::Pharmacists, pharmacyA},
        {}, InfoReqMessage,
        model::Timestamp::fromXsDateTime("2022-01-23T12:34:56Z"), model::Timestamp::fromXsDate("2022-01-03", model::Timestamp::UTCTimezone) });
    const auto givenCommunication2 = addCommunicationToDatabase({
        givenTask.prescriptionId(), model::Communication::MessageType::Reply,
        {ActorRole::Pharmacists, pharmacyA}, {ActorRole::Insurant, kvnrInsurant},
        {}, ReplyMessage,
        model::Timestamp::fromXsDateTime("2022-01-23T12:34:56Z"), model::Timestamp::fromXsDate("2022-01-03", model::Timestamp::UTCTimezone) });
    const auto givenCommunication3 = addCommunicationToDatabase({
        givenTask.prescriptionId(), model::Communication::MessageType::InfoReq,
        {ActorRole::Insurant, kvnrInsurant}, {ActorRole::Pharmacists, pharmacyB},
        {}, InfoReqMessage,
        model::Timestamp::fromXsDateTime("2022-01-23T12:34:56Z"), model::Timestamp::fromXsDate("2022-01-03", model::Timestamp::UTCTimezone) });
    const auto givenCommunication4 = addCommunicationToDatabase({
        givenTask.prescriptionId(), model::Communication::MessageType::Reply,
        {ActorRole::Pharmacists, pharmacyB}, {ActorRole::Insurant, kvnrInsurant},
        {}, ReplyMessage,
        model::Timestamp::fromXsDateTime("2022-01-23T12:34:56Z"), model::Timestamp::fromXsDate("2022-01-03", model::Timestamp::UTCTimezone) });

    // Send the request.
    const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(kvnrInsurant) };
    auto outerResponse = createClient().send(encryptRequest(
        ClientRequest(
            createGetHeader(std::string("/Communication?sender=") + pharmacyA, jwtInsurant),
            ""), jwtInsurant));

    // Verify the response.
    const auto bundle = expectGetCommunicationResponse(outerResponse, { *givenCommunication2.id() }, false);
    const auto selfLink = bundle->getLink(model::Link::Type::Self);
    EXPECT_TRUE(selfLink.has_value());
    EXPECT_EQ(extractPathAndArguments(selfLink.value()), std::string("/Communication?sender=") + pharmacyA);
}


TEST_F(CommunicationGetHandlerTest, getAllCommunications_searchByRecipient)
{
    // Setup the database.
    const auto givenTask = addTaskToDatabase({ model::Task::Status::ready, InsurantF });

    const std::string kvnrInsurant = givenTask.kvnr().value().id();
    const std::string pharmacyA = "3-SMC-B-Testkarte-883110000129068";
    const std::string pharmacyB = "3-SMC-B-Testkarte-883110000129069";

    const auto givenCommunication1 = addCommunicationToDatabase({
        givenTask.prescriptionId(), model::Communication::MessageType::InfoReq,
        {ActorRole::Insurant, kvnrInsurant}, {ActorRole::Pharmacists, pharmacyA},
        {}, InfoReqMessage,
        model::Timestamp::fromXsDateTime("2022-01-23T12:34:56Z"), model::Timestamp::fromXsDate("2022-01-03", model::Timestamp::UTCTimezone) });
    const auto givenCommunication2 = addCommunicationToDatabase({
        givenTask.prescriptionId(), model::Communication::MessageType::Reply,
        {ActorRole::Pharmacists, pharmacyA}, {ActorRole::Insurant, kvnrInsurant},
        {}, ReplyMessage,
        model::Timestamp::fromXsDateTime("2022-01-23T12:34:56Z"), model::Timestamp::fromXsDate("2022-01-03", model::Timestamp::UTCTimezone) });
    const auto givenCommunication3 = addCommunicationToDatabase({
        givenTask.prescriptionId(), model::Communication::MessageType::InfoReq,
        {ActorRole::Insurant, kvnrInsurant}, {ActorRole::Pharmacists, pharmacyB},
        {}, InfoReqMessage,
        model::Timestamp::fromXsDateTime("2022-01-23T12:34:56Z"), model::Timestamp::fromXsDate("2022-01-03", model::Timestamp::UTCTimezone) });
    const auto givenCommunication4 = addCommunicationToDatabase({
        givenTask.prescriptionId(), model::Communication::MessageType::Reply,
        {ActorRole::Pharmacists, pharmacyB}, {ActorRole::Insurant, kvnrInsurant},
        {}, ReplyMessage,
        model::Timestamp::fromXsDateTime("2022-01-23T12:34:56Z"), model::Timestamp::fromXsDate("2022-01-03", model::Timestamp::UTCTimezone) });

    // Send the request.
    const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(kvnrInsurant) };
    auto outerResponse = createClient().send(encryptRequest(
        ClientRequest(
            createGetHeader(std::string("/Communication?recipient=") + pharmacyA, jwtInsurant),
            ""), jwtInsurant));

    // Verify the response.
    const auto bundle = expectGetCommunicationResponse(outerResponse, { *givenCommunication1.id() }, false);
    const auto selfLink = bundle->getLink(model::Link::Type::Self);
    EXPECT_TRUE(selfLink.has_value());
    EXPECT_EQ(extractPathAndArguments(selfLink.value()), std::string("/Communication?recipient=") + pharmacyA);
}


TEST_F(CommunicationGetHandlerTest, getAllCommunications_searchBySenderAndRecipient)
{
    // Setup the database.
    const auto givenTask = addTaskToDatabase({ model::Task::Status::draft, InsurantF });
    const auto givenCommunication1 = addCommunicationToDatabase({
        givenTask.prescriptionId(), model::Communication::MessageType::Representative,
        {ActorRole::Insurant, InsurantF}, {ActorRole::Representative, InsurantG},
        std::string(givenTask.accessCode()),
        RepresentativeMessageByInsurant, model::Timestamp::fromXsDateTime("2022-01-23T12:34:56Z"), model::Timestamp::fromXsDate("2022-01-03", model::Timestamp::UTCTimezone) });
    const auto givenCommunication2 = addCommunicationToDatabase({
        givenTask.prescriptionId(), model::Communication::MessageType::Representative,
        {ActorRole::Insurant, InsurantF}, {ActorRole::Representative, InsurantH},
        std::string(givenTask.accessCode()),
        RepresentativeMessageByInsurant, model::Timestamp::fromXsDate("2022-01-02", model::Timestamp::UTCTimezone), model::Timestamp::fromXsDate("2022-01-04", model::Timestamp::UTCTimezone) });
    const auto givenCommunication3 = addCommunicationToDatabase({
        givenTask.prescriptionId(), model::Communication::MessageType::Representative,
        {ActorRole::Representative, InsurantG}, {ActorRole::Insurant, InsurantF},
        std::string(givenTask.accessCode()),
        RepresentativeMessageByRepresentative, model::Timestamp::fromXsDate("2022-01-02", model::Timestamp::UTCTimezone), model::Timestamp::fromXsDate("2022-01-04", model::Timestamp::UTCTimezone) });

    // Send the request.
    const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(InsurantF) };
    auto outerResponse = createClient().send(encryptRequest(
        ClientRequest(
            createGetHeader(std::string("/Communication?sender=") + InsurantF + "&ignored-argument=in-the-middle&recipient=" + InsurantG, jwtInsurant),
            //                                                                ^ unsupported argument is ignored.
            //                                                          ^ search for communications A -> B                     ^
            ""), jwtInsurant));

    // Verify the response.
    const auto bundle = expectGetCommunicationResponse(outerResponse, {givenCommunication1.id().value()});
    const auto selfLink = bundle->getLink(model::Link::Type::Self);
    EXPECT_TRUE(selfLink.has_value());
    EXPECT_EQ(extractPathAndArguments(selfLink.value()), std::string("/Communication?sender=")+InsurantF+"&recipient="+InsurantG);
}


TEST_F(CommunicationGetHandlerTest, getAllCommunications_searchBySenders)
{
    // Setup the database.
    const auto givenTask = addTaskToDatabase({ model::Task::Status::ready, InsurantF });

    const std::string kvnrInsurant = givenTask.kvnr().value().id();
    const std::string pharmacyA = "3-SMC-B-Testkarte-883110000129068";
    const std::string pharmacyB = "3-SMC-B-Testkarte-883110000129069";

    const auto givenCommunication1 = addCommunicationToDatabase({
        givenTask.prescriptionId(), model::Communication::MessageType::InfoReq,
        {ActorRole::Insurant, kvnrInsurant}, {ActorRole::Pharmacists, pharmacyA},
        {}, InfoReqMessage,
        model::Timestamp::fromXsDateTime("2022-01-23T12:34:56Z"), model::Timestamp::fromXsDate("2022-01-03", model::Timestamp::UTCTimezone) });
    const auto givenCommunication2 = addCommunicationToDatabase({
        givenTask.prescriptionId(), model::Communication::MessageType::Reply,
        {ActorRole::Pharmacists, pharmacyA}, {ActorRole::Insurant, kvnrInsurant},
        {}, ReplyMessage,
        model::Timestamp::fromXsDateTime("2022-01-23T12:34:56Z"), model::Timestamp::fromXsDate("2022-01-03", model::Timestamp::UTCTimezone)});
    const auto givenCommunication3 = addCommunicationToDatabase({
        givenTask.prescriptionId(), model::Communication::MessageType::InfoReq,
        {ActorRole::Insurant, kvnrInsurant}, {ActorRole::Pharmacists, pharmacyB},
        {}, InfoReqMessage,
        model::Timestamp::fromXsDateTime("2022-01-23T12:34:56Z"), model::Timestamp::fromXsDate("2022-01-03", model::Timestamp::UTCTimezone)});
    const auto givenCommunication4 = addCommunicationToDatabase({
        givenTask.prescriptionId(), model::Communication::MessageType::Reply,
        {ActorRole::Pharmacists, pharmacyB}, {ActorRole::Insurant, kvnrInsurant},
        {}, ReplyMessage,
        model::Timestamp::fromXsDateTime("2022-01-23T12:34:56Z"), model::Timestamp::fromXsDate("2022-01-03", model::Timestamp::UTCTimezone)});

    // Send the request.
    const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(kvnrInsurant) };
    auto outerResponse = createClient().send(encryptRequest(
        ClientRequest(
            createGetHeader(std::string("/Communication?sender=") + pharmacyA + "%2C" + pharmacyB, jwtInsurant),
            ""), jwtInsurant));

    // Verify the response.
    const auto bundle = expectGetCommunicationResponse(outerResponse, { *givenCommunication2.id(), *givenCommunication4.id() }, false);
    const auto selfLink = bundle->getLink(model::Link::Type::Self);
    EXPECT_TRUE(selfLink.has_value());
    EXPECT_EQ(extractPathAndArguments(selfLink.value()), std::string("/Communication?sender=") + pharmacyA + "%2c" + pharmacyB);
}


TEST_F(CommunicationGetHandlerTest, getAllCommunications_searchByRecipients)
{
    // Setup the database.
    const auto givenTask = addTaskToDatabase({ model::Task::Status::ready, InsurantF });

    const std::string kvnrInsurant = givenTask.kvnr().value().id();
    const std::string pharmacyA = "3-SMC-B-Testkarte-883110000129068";
    const std::string pharmacyB = "3-SMC-B-Testkarte-883110000129069";

    const auto givenCommunication1 = addCommunicationToDatabase({
        givenTask.prescriptionId(), model::Communication::MessageType::InfoReq,
        {ActorRole::Insurant, kvnrInsurant}, {ActorRole::Pharmacists, pharmacyA},
        {}, InfoReqMessage,
        model::Timestamp::fromXsDateTime("2022-01-23T12:34:56Z"), model::Timestamp::fromXsDate("2022-01-03", model::Timestamp::UTCTimezone)});
    const auto givenCommunication2 = addCommunicationToDatabase({
        givenTask.prescriptionId(), model::Communication::MessageType::Reply,
        {ActorRole::Pharmacists, pharmacyA}, {ActorRole::Insurant, kvnrInsurant},
        {}, ReplyMessage,
        model::Timestamp::fromXsDateTime("2022-01-23T12:34:56Z"), model::Timestamp::fromXsDate("2022-01-03", model::Timestamp::UTCTimezone)});
    const auto givenCommunication3 = addCommunicationToDatabase({
        givenTask.prescriptionId(), model::Communication::MessageType::InfoReq,
        {ActorRole::Insurant, kvnrInsurant}, {ActorRole::Pharmacists, pharmacyB},
        {}, InfoReqMessage,
        model::Timestamp::fromXsDateTime("2022-01-23T12:34:56Z"), model::Timestamp::fromXsDate("2022-01-03", model::Timestamp::UTCTimezone)});
    const auto givenCommunication4 = addCommunicationToDatabase({
        givenTask.prescriptionId(), model::Communication::MessageType::Reply,
        {ActorRole::Pharmacists, pharmacyB}, {ActorRole::Insurant, kvnrInsurant},
        {}, ReplyMessage,
        model::Timestamp::fromXsDateTime("2022-01-23T12:34:56Z"), model::Timestamp::fromXsDate("2022-01-03", model::Timestamp::UTCTimezone)});

    // Send the request.
    const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(kvnrInsurant) };
    auto outerResponse = createClient().send(encryptRequest(
        ClientRequest(
            createGetHeader(std::string("/Communication?sender=") + pharmacyA + "%2C" + pharmacyB, jwtInsurant),
            ""), jwtInsurant));

    // Verify the response.
    const auto bundle = expectGetCommunicationResponse(outerResponse, { *givenCommunication2.id(), *givenCommunication4.id() }, false);
    const auto selfLink = bundle->getLink(model::Link::Type::Self);
    EXPECT_TRUE(selfLink.has_value());
    EXPECT_EQ(extractPathAndArguments(selfLink.value()), std::string("/Communication?sender=") + pharmacyA + "%2c" + pharmacyB);
}


TEST_F(CommunicationGetHandlerTest, getAllCommunications_searchBySendersAndRecipients)
{
    // Setup the database.
    const auto givenTask = addTaskToDatabase({ model::Task::Status::ready, InsurantF });

    const std::string kvnrInsurant = givenTask.kvnr().value().id();
    const std::string pharmacyA = "3-SMC-B-Testkarte-883110000129068";
    const std::string pharmacyB = "3-SMC-B-Testkarte-883110000129069";

    const auto givenCommunication1 = addCommunicationToDatabase({
        givenTask.prescriptionId(), model::Communication::MessageType::InfoReq,
        {ActorRole::Insurant, kvnrInsurant}, {ActorRole::Pharmacists, pharmacyA},
        {}, InfoReqMessage,
        model::Timestamp::fromXsDateTime("2022-01-23T12:34:56Z"), model::Timestamp::fromXsDate("2022-01-03", model::Timestamp::UTCTimezone)});
    const auto givenCommunication2 = addCommunicationToDatabase({
        givenTask.prescriptionId(), model::Communication::MessageType::Reply,
        {ActorRole::Pharmacists, pharmacyA}, {ActorRole::Insurant, kvnrInsurant},
        {}, ReplyMessage,
        model::Timestamp::fromXsDateTime("2022-01-23T12:34:56Z"), model::Timestamp::fromXsDate("2022-01-03", model::Timestamp::UTCTimezone)});
    const auto givenCommunication3 = addCommunicationToDatabase({
        givenTask.prescriptionId(), model::Communication::MessageType::InfoReq,
        {ActorRole::Insurant, kvnrInsurant}, {ActorRole::Pharmacists, pharmacyB},
        {}, InfoReqMessage,
        model::Timestamp::fromXsDateTime("2022-01-23T12:34:56Z"), model::Timestamp::fromXsDate("2022-01-03", model::Timestamp::UTCTimezone)});
    const auto givenCommunication4 = addCommunicationToDatabase({
        givenTask.prescriptionId(), model::Communication::MessageType::Reply,
        {ActorRole::Pharmacists, pharmacyB}, {ActorRole::Insurant, kvnrInsurant},
        {}, ReplyMessage,
        model::Timestamp::fromXsDateTime("2022-01-23T12:34:56Z"), model::Timestamp::fromXsDate("2022-01-03", model::Timestamp::UTCTimezone)});

    // Send the request.
    const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(kvnrInsurant) };
    auto outerResponse = createClient().send(encryptRequest(
        ClientRequest(
            createGetHeader(std::string("/Communication?sender=") + pharmacyA + "%2C" + pharmacyB + "&recipient=" + kvnrInsurant + "," + InsurantA, jwtInsurant),
            ""), jwtInsurant));

    // Verify the response.
    const auto bundle = expectGetCommunicationResponse(outerResponse, { *givenCommunication2.id(), *givenCommunication4.id() }, false);
    const auto selfLink = bundle->getLink(model::Link::Type::Self);
    EXPECT_TRUE(selfLink.has_value());
    EXPECT_EQ(extractPathAndArguments(selfLink.value()), std::string("/Communication?sender=") + pharmacyA + "%2c" + pharmacyB + "&recipient=" + kvnrInsurant + "%2c" + InsurantA);
}


TEST_F(CommunicationGetHandlerTest, getAllCommunications_searchBySendersAndSent)
{
    // Setup the database.
    const auto givenTask = addTaskToDatabase({ model::Task::Status::ready, InsurantF });

    const std::string kvnrInsurant = givenTask.kvnr().value().id();
    const std::string pharmacyA = "3-SMC-B-Testkarte-883110000129068";
    const std::string pharmacyB = "3-SMC-B-Testkarte-883110000129069";

    const auto givenCommunication1 = addCommunicationToDatabase({
        givenTask.prescriptionId(), model::Communication::MessageType::InfoReq,
        {ActorRole::Insurant, kvnrInsurant}, {ActorRole::Pharmacists, pharmacyA},
        {}, InfoReqMessage,
        model::Timestamp::fromXsDateTime("2022-01-23T12:34:56Z"), model::Timestamp::fromXsDate("2022-01-03", model::Timestamp::UTCTimezone)});
    const auto givenCommunication2 = addCommunicationToDatabase({
        givenTask.prescriptionId(), model::Communication::MessageType::Reply,
        {ActorRole::Pharmacists, pharmacyA}, {ActorRole::Insurant, kvnrInsurant},
        {}, ReplyMessage,
        model::Timestamp::fromXsDateTime("2022-01-23T12:34:56Z"), model::Timestamp::fromXsDate("2022-01-03", model::Timestamp::UTCTimezone)});
    const auto givenCommunication3 = addCommunicationToDatabase({
        givenTask.prescriptionId(), model::Communication::MessageType::InfoReq,
        {ActorRole::Insurant, kvnrInsurant}, {ActorRole::Pharmacists, pharmacyB},
        {}, InfoReqMessage,
        model::Timestamp::fromXsDateTime("2022-01-23T12:34:56Z"), model::Timestamp::fromXsDate("2022-01-03", model::Timestamp::UTCTimezone)});
    const auto givenCommunication4 = addCommunicationToDatabase({
        givenTask.prescriptionId(), model::Communication::MessageType::Reply,
        {ActorRole::Pharmacists, pharmacyB}, {ActorRole::Insurant, kvnrInsurant},
        {}, ReplyMessage,
        model::Timestamp::fromXsDateTime("2022-01-23T12:34:56Z"), model::Timestamp::fromXsDate("2022-01-03", model::Timestamp::UTCTimezone)});

    // Send the request.
    const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(kvnrInsurant) };
    auto outerResponse = createClient().send(encryptRequest(
        ClientRequest(
            createGetHeader(std::string("/Communication?sender=") + pharmacyA + "," + pharmacyB + "&sent=ge2022-01-01", jwtInsurant),
            ""), jwtInsurant));

    // Verify the response.
    const auto bundle = expectGetCommunicationResponse(outerResponse, { *givenCommunication2.id(), *givenCommunication4.id() }, false);
    const auto selfLink = bundle->getLink(model::Link::Type::Self);
    EXPECT_TRUE(selfLink.has_value());
    EXPECT_EQ(extractPathAndArguments(selfLink.value()), std::string("/Communication?sender=") + pharmacyA + "%2c" + pharmacyB + "&sent=ge2022-01-01");
}


TEST_F(CommunicationGetHandlerTest, getAllCommunications_searchByRecipientAndReceived)
{
    // Setup the database.
    const auto givenTask = addTaskToDatabase({ model::Task::Status::draft, InsurantF });
    const auto givenCommunication1 = addCommunicationToDatabase({
        givenTask.prescriptionId(), model::Communication::MessageType::Representative,
        {ActorRole::Insurant, InsurantF}, {ActorRole::Representative, InsurantG},
        std::string(givenTask.accessCode()),
        RepresentativeMessageByInsurant, model::Timestamp::fromXsDateTime("2022-01-23T12:34:56Z"), model::Timestamp::fromXsDate("2022-01-03", model::Timestamp::UTCTimezone)});
    const auto givenCommunication2 = addCommunicationToDatabase({
        givenTask.prescriptionId(), model::Communication::MessageType::Representative,
        {ActorRole::Insurant, InsurantF}, {ActorRole::Representative, InsurantH},
        std::string(givenTask.accessCode()),
        RepresentativeMessageByInsurant, model::Timestamp::fromXsDate("2022-01-02", model::Timestamp::UTCTimezone) });
    const auto givenCommunication3 = addCommunicationToDatabase({
        givenTask.prescriptionId(), model::Communication::MessageType::Representative,
        {ActorRole::Insurant, InsurantF}, {ActorRole::Representative, InsurantH},
        std::string(givenTask.accessCode()),
        RepresentativeMessageByInsurant, model::Timestamp::fromXsDate("2022-01-02", model::Timestamp::UTCTimezone), model::Timestamp::fromXsDate("2022-01-04", model::Timestamp::UTCTimezone) });

    // Send the request.
    const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(InsurantF) };
    auto outerResponse = createClient().send(encryptRequest(
        ClientRequest(
            createGetHeader(std::string("/Communication?recipient=" + std::string(InsurantH) + "&received=sa2022-01-03T12:34:56Z"), jwtInsurant),
            ""), jwtInsurant));

    // Verify the response.
    const auto bundle = expectGetCommunicationResponse(outerResponse, {givenCommunication3.id().value()});
    const auto selfLink = bundle->getLink(model::Link::Type::Self);
    EXPECT_TRUE(selfLink.has_value());
    EXPECT_EQ(UrlHelper::unescapeUrl(extractPathAndArguments(selfLink.value())),
            std::string("/Communication?recipient=")+InsurantH+"&received=sa2022-01-03T12:34:56+00:00");
}


TEST_F(CommunicationGetHandlerTest, getAllCommunications_keyWithoutValueIsIgnored)
{
    // Setup the database.
    const auto givenTask = addTaskToDatabase({ model::Task::Status::draft, InsurantF });
    const auto givenCommunication1 = addCommunicationToDatabase({
        givenTask.prescriptionId(), model::Communication::MessageType::Representative,
        {ActorRole::Insurant, InsurantF}, {ActorRole::Representative, InsurantG},
        std::string(givenTask.accessCode()),
        RepresentativeMessageByInsurant, model::Timestamp::fromXsDateTime("2022-01-23T12:34:56Z"), model::Timestamp::fromXsDate("2022-01-03", model::Timestamp::UTCTimezone)});
    const auto givenCommunication2 = addCommunicationToDatabase({
        givenTask.prescriptionId(), model::Communication::MessageType::Representative,
        {ActorRole::Insurant, InsurantF}, {ActorRole::Representative, InsurantH},
        std::string(givenTask.accessCode()),
        RepresentativeMessageByInsurant, model::Timestamp::fromXsDate("2022-01-02", model::Timestamp::UTCTimezone), model::Timestamp::fromXsDate("2022-01-04", model::Timestamp::UTCTimezone) });

    // Send the request.
    const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(InsurantF) };
    auto outerResponse = createClient().send(encryptRequest(
        ClientRequest(
            createGetHeader(std::string("/Communication?recipient="), jwtInsurant),
            //                                                    ^ missing value
            ""), jwtInsurant));

    // Verify the response. The missing value means that the key is ignored => no reduction of result set beyond filtering by caller.
    const auto bundle = expectGetCommunicationResponse(outerResponse, {givenCommunication1.id().value(), givenCommunication2.id().value() }, false);
    const auto selfLink = bundle->getLink(model::Link::Type::Self);
    EXPECT_TRUE(selfLink.has_value());
    EXPECT_EQ(extractPathAndArguments(selfLink.value()), std::string("/Communication"));
}


TEST_F(CommunicationGetHandlerTest, getAllCommunications_ignoredValueCanContainSpaces)
{
    // Setup the database.
    const auto givenTask = addTaskToDatabase({ model::Task::Status::draft, InsurantF });
    const auto givenCommunication1 = addCommunicationToDatabase({
        givenTask.prescriptionId(), model::Communication::MessageType::Representative,
        {ActorRole::Insurant, InsurantF}, {ActorRole::Representative, InsurantG},
        std::string(givenTask.accessCode()),
        RepresentativeMessageByInsurant, model::Timestamp::fromXsDateTime("2022-01-23T12:34:56Z"), model::Timestamp::fromXsDate("2022-01-03", model::Timestamp::UTCTimezone)});
    const auto givenCommunication2 = addCommunicationToDatabase({
        givenTask.prescriptionId(), model::Communication::MessageType::Representative,
        {ActorRole::Insurant, InsurantF}, {ActorRole::Representative, InsurantH},
        std::string(givenTask.accessCode()),
        RepresentativeMessageByInsurant, model::Timestamp::fromXsDate("2022-01-02", model::Timestamp::UTCTimezone), model::Timestamp::fromXsDate("2022-01-04", model::Timestamp::UTCTimezone) });

    // Send the request.
    const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(InsurantF) };
    auto outerResponse = createClient().send(encryptRequest(
        ClientRequest(
            createGetHeader(std::string("/Communication?ignored=text+with%20spaces"), jwtInsurant),
            //                                                      ^    ^ spaces
            ""), jwtInsurant));

    // Verify the response. The only argument is ignored => no reduction of result set beyond filtering by caller.
    const auto bundle =
        expectGetCommunicationResponse(outerResponse, {givenCommunication1.id().value(), givenCommunication2.id().value() }, false);
    const auto selfLink = bundle->getLink(model::Link::Type::Self);
    EXPECT_TRUE(selfLink.has_value());
    EXPECT_EQ(extractPathAndArguments(selfLink.value()), std::string("/Communication"));
}


TEST_F(CommunicationGetHandlerTest, getAllCommunications_sort_receivedSent)
{
    // Setup the database.
    const auto givenTask = addTaskToDatabase({ model::Task::Status::draft, InsurantF });
    const auto givenCommunicationIds = addCommunicationsToDatabase({
        //             v 1:sent v    ^2:received^
        {givenTask.prescriptionId(), model::Communication::MessageType::Representative,
            {ActorRole::Insurant, InsurantF}, {ActorRole::Representative, InsurantG},
            std::string(givenTask.accessCode()),
            RepresentativeMessageByInsurant, model::Timestamp::fromXsDate("2022-01-01", model::Timestamp::UTCTimezone), model::Timestamp::fromXsDate("2022-01-04", model::Timestamp::UTCTimezone)},
        {givenTask.prescriptionId(), model::Communication::MessageType::Representative,
            {ActorRole::Insurant, InsurantF}, {ActorRole::Representative, InsurantH},
            std::string(givenTask.accessCode()),
            RepresentativeMessageByInsurant, model::Timestamp::fromXsDate("2022-01-01", model::Timestamp::UTCTimezone), model::Timestamp::fromXsDate("2022-01-03", model::Timestamp::UTCTimezone)},
        {givenTask.prescriptionId(), model::Communication::MessageType::Representative,
            {ActorRole::Insurant, InsurantF}, {ActorRole::Representative, InsurantG},
            std::string(givenTask.accessCode()),
            RepresentativeMessageByInsurant, model::Timestamp::fromXsDate("2022-01-02", model::Timestamp::UTCTimezone), model::Timestamp::fromXsDate("2022-01-03", model::Timestamp::UTCTimezone)},
        {givenTask.prescriptionId(), model::Communication::MessageType::Representative,
            {ActorRole::Insurant, InsurantF}, {ActorRole::Representative, InsurantH},
            std::string(givenTask.accessCode()),
            RepresentativeMessageByInsurant, model::Timestamp::fromXsDate("2022-01-03", model::Timestamp::UTCTimezone), model::Timestamp::fromXsDate("2022-01-04", model::Timestamp::UTCTimezone)}
        });

    // Send the request for _sort=sent,-received"
    const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(InsurantF) };
    auto outerResponse = createClient().send(encryptRequest(ClientRequest(
        createGetHeader(std::string("/Communication?_sort=-received&ignored&_sort=sent"), jwtInsurant), ""), jwtInsurant));

    // Verify the response. _sort=-received,sent => input values are sorted accordingly
    const auto bundle = expectGetCommunicationResponse(outerResponse,
        {   givenCommunicationIds[0],
            givenCommunicationIds[3],
            givenCommunicationIds[1],
            givenCommunicationIds[2]});
    const auto selfLink = bundle->getLink(model::Link::Type::Self);
    EXPECT_TRUE(selfLink.has_value());
    EXPECT_EQ(extractPathAndArguments(selfLink.value()), "/Communication?_sort=-received,sent");
}


TEST_F(CommunicationGetHandlerTest, getAllCommunications_sort_receivedSentAndSearch)
{
    // Setup the database.
    const auto givenTask = addTaskToDatabase({ model::Task::Status::draft, InsurantF });
    const auto givenCommunicationIds = addCommunicationsToDatabase({
        //             v 1:sent v    ^2:received^
        {givenTask.prescriptionId(), model::Communication::MessageType::Representative,
            {ActorRole::Insurant, InsurantF}, {ActorRole::Representative, InsurantG},
            std::string(givenTask.accessCode()),
            RepresentativeMessageByInsurant, model::Timestamp::fromXsDate("2022-01-01", model::Timestamp::UTCTimezone), model::Timestamp::fromXsDate("2022-01-04", model::Timestamp::UTCTimezone)},
        {givenTask.prescriptionId(), model::Communication::MessageType::Representative,
            {ActorRole::Insurant, InsurantF}, {ActorRole::Representative, InsurantH},
            std::string(givenTask.accessCode()),
            RepresentativeMessageByInsurant, model::Timestamp::fromXsDate("2022-01-01", model::Timestamp::UTCTimezone), model::Timestamp::fromXsDate("2022-01-03", model::Timestamp::UTCTimezone)},
        {givenTask.prescriptionId(), model::Communication::MessageType::Representative,
            {ActorRole::Insurant, InsurantF}, {ActorRole::Representative, InsurantG},
            std::string(givenTask.accessCode()),
            RepresentativeMessageByInsurant, model::Timestamp::fromXsDate("2022-01-02", model::Timestamp::UTCTimezone), model::Timestamp::fromXsDate("2022-01-03", model::Timestamp::UTCTimezone)},
        {givenTask.prescriptionId(), model::Communication::MessageType::Representative,
            {ActorRole::Insurant, InsurantF}, {ActorRole::Representative, InsurantH},
            std::string(givenTask.accessCode()),
            RepresentativeMessageByInsurant, model::Timestamp::fromXsDate("2022-01-03", model::Timestamp::UTCTimezone), model::Timestamp::fromXsDate("2022-01-04", model::Timestamp::UTCTimezone)}
        });

    // Send the request for _sort=sent,-received and filter (search) by recipient=InsurantH"
    const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(InsurantF) };
    auto outerResponse = createClient().send(encryptRequest(ClientRequest(
        createGetHeader(std::string("/Communication?recipient=")+InsurantH+"&_sort=sent,-received", jwtInsurant), ""), jwtInsurant));

    // Verify the response. recipient=InsurantH => only two of the input values remain
    const auto bundle = expectGetCommunicationResponse(outerResponse,
        {   givenCommunicationIds[1],
            givenCommunicationIds[3]});
    const auto selfLink = bundle->getLink(model::Link::Type::Self);
    EXPECT_EQ(extractPathAndArguments(selfLink.value()), "/Communication?recipient=X234567892&_sort=sent,-received");
}


TEST_F(CommunicationGetHandlerTest, getAllCommunications_paging_firstPage)//NOLINT(readability-function-cognitive-complexity)
{
    // Setup the database.
    const auto givenTask = addTaskToDatabase({ model::Task::Status::draft, InsurantF });
    const auto givenCommunicationIds = addCommunicationsToDatabase({
        //             v 1:sent v    ^2:received^
        {givenTask.prescriptionId(), model::Communication::MessageType::Representative,
            {ActorRole::Insurant, InsurantF}, {ActorRole::Representative, InsurantG},
            std::string(givenTask.accessCode()),
            RepresentativeMessageByInsurant, model::Timestamp::fromXsDate("2022-01-01", model::Timestamp::UTCTimezone)},
        {givenTask.prescriptionId(), model::Communication::MessageType::Representative,
            {ActorRole::Insurant, InsurantF}, {ActorRole::Representative, InsurantH},
            std::string(givenTask.accessCode()),
            RepresentativeMessageByInsurant, model::Timestamp::fromXsDate("2022-01-02", model::Timestamp::UTCTimezone)},
        {givenTask.prescriptionId(), model::Communication::MessageType::Representative,
            {ActorRole::Insurant, InsurantF}, {ActorRole::Representative, InsurantG},
            std::string(givenTask.accessCode()),
            RepresentativeMessageByInsurant, model::Timestamp::fromXsDate("2022-01-03", model::Timestamp::UTCTimezone)},
        {givenTask.prescriptionId(), model::Communication::MessageType::Representative,
            {ActorRole::Insurant, InsurantF}, {ActorRole::Representative, InsurantH},
            std::string(givenTask.accessCode()),
            RepresentativeMessageByInsurant, model::Timestamp::fromXsDate("2022-01-04", model::Timestamp::UTCTimezone)}
        });

    // Send the request for _sort=sent,-received and filter (search) by recipient=InsurantH"
    const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(InsurantF) };
    auto outerResponse = createClient().send(encryptRequest(ClientRequest(
        createGetHeader(std::string("/Communication?_sort=-sent&_count=2&count=20"), jwtInsurant),
        //                                                               ^ missing underscore => "count" is ignored
        ""), jwtInsurant));

    // Verify the response. The missing value means that the key is ignored => no reduction of result set beyond filtering by caller.
    const auto bundle = expectGetCommunicationResponse(outerResponse,
        {   givenCommunicationIds[3],
            givenCommunicationIds[2]});
    const auto selfLink = bundle->getLink(model::Link::Type::Self);
    EXPECT_TRUE(selfLink.has_value());
    EXPECT_EQ(extractPathAndArguments(selfLink.value()), "/Communication?_sort=-sent&_count=2&__offset=0");
    EXPECT_FALSE(bundle->getLink(model::Link::Type::Prev).has_value()); // The first page does not have a prev link ...
    const auto next = bundle->getLink(model::Link::Type::Next);
    EXPECT_TRUE(next.has_value());                                     // ... but it has a next link
    EXPECT_EQ(extractPathAndArguments(next.value()), "/Communication?_sort=-sent&_count=2&__offset=2");
    EXPECT_EQ(bundle->getTotalSearchMatches(), givenCommunicationIds.size());
}


TEST_F(CommunicationGetHandlerTest, getAllCommunications_paging_nextPage)//NOLINT(readability-function-cognitive-complexity)
{
    // Setup the database.
    const auto givenTask = addTaskToDatabase({ model::Task::Status::draft, InsurantF });
    const auto givenCommunicationIds = addCommunicationsToDatabase({
        //             v 1:sent v    ^2:received^
        {givenTask.prescriptionId(), model::Communication::MessageType::Representative,
            {ActorRole::Insurant, InsurantF}, {ActorRole::Representative, InsurantG},
            std::string(givenTask.accessCode()),
            RepresentativeMessageByInsurant, model::Timestamp::fromXsDate("2022-01-01", model::Timestamp::UTCTimezone)},
        {givenTask.prescriptionId(), model::Communication::MessageType::Representative,
            {ActorRole::Insurant, InsurantF}, {ActorRole::Representative, InsurantH},
            std::string(givenTask.accessCode()),
            RepresentativeMessageByInsurant, model::Timestamp::fromXsDate("2022-01-02", model::Timestamp::UTCTimezone)},
        {givenTask.prescriptionId(), model::Communication::MessageType::Representative,
            {ActorRole::Insurant, InsurantF}, {ActorRole::Representative, InsurantG},
            std::string(givenTask.accessCode()),
            RepresentativeMessageByInsurant, model::Timestamp::fromXsDate("2022-01-03", model::Timestamp::UTCTimezone)},
        {givenTask.prescriptionId(), model::Communication::MessageType::Representative,
            {ActorRole::Insurant, InsurantF}, {ActorRole::Representative, InsurantH},
            std::string(givenTask.accessCode()),
            RepresentativeMessageByInsurant, model::Timestamp::fromXsDate("2022-01-04", model::Timestamp::UTCTimezone)}
        });

    for (const auto& communicationId : givenCommunicationIds)
        std::cerr << communicationId.toString() << "\n";

    // Get the second page.
    const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(InsurantF) };
    auto outerResponse = createClient().send(encryptRequest(ClientRequest(
        createGetHeader("/Communication?_sort=-sent&_count=2&__offset=2", jwtInsurant),
        ""), jwtInsurant));

    // Verify the response. _count=2 => 2 entries in the result. -sent&__offset=2 => second page of the reverted input list
    const auto bundle = expectGetCommunicationResponse(outerResponse,
        {   givenCommunicationIds[1],
            givenCommunicationIds[0]});

    const auto selfLink = bundle->getLink(model::Link::Type::Self);
    EXPECT_TRUE(selfLink.has_value());
    EXPECT_EQ(extractPathAndArguments(selfLink.value()), "/Communication?_sort=-sent&_count=2&__offset=2");

    const auto prevLink = bundle->getLink(model::Link::Type::Prev);
    EXPECT_TRUE(prevLink.has_value());
    EXPECT_EQ(extractPathAndArguments(prevLink.value()), "/Communication?_sort=-sent&_count=2&__offset=0");

    // For the test setup with four entries, there does not have to be a 'next' link.
    const auto nextLink = bundle->getLink(model::Link::Type::Next);
    EXPECT_FALSE(nextLink.has_value());

    EXPECT_EQ(bundle->getTotalSearchMatches(), givenCommunicationIds.size());
}


TEST_F(CommunicationGetHandlerTest, getAllCommunications_searchByReceivedNull)//NOLINT(readability-function-cognitive-complexity)
{
    std::string kvnrInsurant = InsurantA;
    std::string kvnrRepresentative1 = InsurantB;
    std::string kvnrRepresentative2 = InsurantC;
    std::string kvnrRepresentative3 = InsurantD;
    std::string kvnrRepresentative4 = InsurantE;
    std::string pharmacy1 = "3-SMC-B-Testkarte-883110000120312";
    std::string pharmacy2 = "3-SMC-B-Testkarte-883110000231423";
    std::string pharmacy3 = "3-SMC-B-Testkarte-883110000342534";
    std::string pharmacy4 = "3-SMC-B-Testkarte-883110000453645";

    const auto task = addTaskToDatabase({ model::Task::Status::ready, kvnrInsurant });

    model::Communication infoReqByRepresentative1ToPharmacy1 = addCommunicationToDatabase({
        task.prescriptionId(), model::Communication::MessageType::InfoReq,
            {ActorRole::Insurant, kvnrRepresentative1}, {ActorRole::Pharmacists, pharmacy1},
            "", InfoReqMessage });
    model::Communication replyByPharmacy1ToRepresentative1 = addCommunicationToDatabase({
        task.prescriptionId(), model::Communication::MessageType::Reply,
            {ActorRole::Pharmacists, pharmacy1}, {ActorRole::Insurant, kvnrRepresentative1},
            "", ReplyMessage });
    model::Communication infoReqByRepresentative2ToPharmacy2 = addCommunicationToDatabase({
        task.prescriptionId(), model::Communication::MessageType::InfoReq,
            {ActorRole::Insurant, kvnrRepresentative2}, {ActorRole::Pharmacists, pharmacy2},
            "", InfoReqMessage });
    model::Communication replyByPharmacy2ToRepresentative2 = addCommunicationToDatabase({
        task.prescriptionId(), model::Communication::MessageType::Reply,
            {ActorRole::Pharmacists, pharmacy2}, {ActorRole::Insurant, kvnrRepresentative2},
            "", ReplyMessage });
    model::Communication dispReqByRepresentative3ToPharmacy3 = addCommunicationToDatabase({
        task.prescriptionId(), model::Communication::MessageType::DispReq,
            {ActorRole::Insurant, kvnrRepresentative3}, {ActorRole::Pharmacists, pharmacy3},
            "", ReplyMessage });
    model::Communication dispReqByRepresentative4ToPharmacy4 = addCommunicationToDatabase({
        task.prescriptionId(), model::Communication::MessageType::DispReq,
            {ActorRole::Insurant, kvnrRepresentative4}, {ActorRole::Pharmacists, pharmacy4},
            "", ReplyMessage });

    // Search for "sender=<Insurant>&received=NULL". Expected result: all unread messages for the sender.
    {
        const JWT jwtPharmacy{ mJwtBuilder.makeJwtApotheke(pharmacy1) };
        std::string pathAndArguments = "/Communication?received=NULL&sender=" + kvnrRepresentative1;
        ClientRequest request(createGetHeader(pathAndArguments, jwtPharmacy), "");
        auto outerResponse = createClient().send(encryptRequest(request, jwtPharmacy));
        const auto bundle = expectGetCommunicationResponse(
            outerResponse, { infoReqByRepresentative1ToPharmacy1.id().value() }, true, HttpStatus::OK, ContentMimeType::fhirXmlUtf8);
        const auto selfLink = bundle->getLink(model::Link::Type::Self);
        EXPECT_TRUE(selfLink.has_value());
        EXPECT_EQ(extractPathAndArguments(selfLink.value()), "/Communication?received=eqNULL&sender=" + kvnrRepresentative1);
    }
    // Search for "sender=<Pharmacy>&received=NULL". Expected result: all unread messages for the sender.
    {
        const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(kvnrRepresentative1) };
        std::string pathAndArguments = "/Communication?received=NULL&sender=" + pharmacy1;
        ClientRequest request(createGetHeader(pathAndArguments, jwtInsurant), "");
        auto outerResponse = createClient().send(encryptRequest(request, jwtInsurant));
        const auto bundle = expectGetCommunicationResponse(
            outerResponse, { replyByPharmacy1ToRepresentative1.id().value() }, true, HttpStatus::OK, ContentMimeType::fhirJsonUtf8);
        const auto selfLink = bundle->getLink(model::Link::Type::Self);
        EXPECT_TRUE(selfLink.has_value());
        EXPECT_EQ(extractPathAndArguments(selfLink.value()), "/Communication?received=eqNULL&sender=" + pharmacy1);
    }

    // Search for "recipient=<Insurant>&received=NULL". Expected result: all unread messages for the sender.
    {
        const JWT jwtPharmacy{ mJwtBuilder.makeJwtApotheke(pharmacy2) };
        std::string pathAndArguments = "/Communication?received=NULL&recipient=" + kvnrRepresentative2;
        ClientRequest request(createGetHeader(pathAndArguments, jwtPharmacy), "");
        auto outerResponse = createClient().send(encryptRequest(request, jwtPharmacy));
        const auto bundle = expectGetCommunicationResponse(
            outerResponse, { replyByPharmacy2ToRepresentative2.id().value() }, true, HttpStatus::OK, ContentMimeType::fhirXmlUtf8);
        const auto selfLink = bundle->getLink(model::Link::Type::Self);
        EXPECT_TRUE(selfLink.has_value());
        EXPECT_EQ(extractPathAndArguments(selfLink.value()), "/Communication?received=eqNULL&recipient=" + kvnrRepresentative2);
    }
    // Search for "recipient=<Pharmacy>&received=NULL". Expected result: all unread messages for the sender.
    {
        const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(kvnrRepresentative2) };
        std::string pathAndArguments = "/Communication?received=NULL&recipient=" + pharmacy2;
        ClientRequest request(createGetHeader(pathAndArguments, jwtInsurant), "");
        auto outerResponse = createClient().send(encryptRequest(request, jwtInsurant));
        const auto bundle = expectGetCommunicationResponse(
        outerResponse, { infoReqByRepresentative2ToPharmacy2.id().value() }, true, HttpStatus::OK, ContentMimeType::fhirJsonUtf8);
        const auto selfLink = bundle->getLink(model::Link::Type::Self);
        EXPECT_TRUE(selfLink.has_value());
        EXPECT_EQ(extractPathAndArguments(selfLink.value()), "/Communication?received=eqNULL&recipient=" + pharmacy2);
    }

    // Search for "received=NULL". Expected result: all unread messages for the sender (in AccessToken) of the query.
    {
        const JWT jwtPharmacy{ mJwtBuilder.makeJwtApotheke(pharmacy3) };
        std::string pathAndArguments = "/Communication?received=NULL";
        ClientRequest request(createGetHeader(pathAndArguments, jwtPharmacy), "");
        auto outerResponse = createClient().send(encryptRequest(request, jwtPharmacy));
        const auto bundle = expectGetCommunicationResponse(
            outerResponse, { dispReqByRepresentative3ToPharmacy3.id().value() }, true, HttpStatus::OK, ContentMimeType::fhirXmlUtf8);
        const auto selfLink = bundle->getLink(model::Link::Type::Self);
        EXPECT_TRUE(selfLink.has_value());
        EXPECT_EQ(extractPathAndArguments(selfLink.value()), "/Communication?received=eqNULL");
    }
    {
        const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(kvnrRepresentative4) };
        std::string pathAndArguments = "/Communication?received=NULL";
        ClientRequest request(createGetHeader(pathAndArguments, jwtInsurant), "");
        auto outerResponse = createClient().send(encryptRequest(request, jwtInsurant));
        const auto bundle = expectGetCommunicationResponse(
            outerResponse, { dispReqByRepresentative4ToPharmacy4.id().value() }, true, HttpStatus::OK, ContentMimeType::fhirJsonUtf8);
        const auto selfLink = bundle->getLink(model::Link::Type::Self);
        EXPECT_TRUE(selfLink.has_value());
        EXPECT_EQ(extractPathAndArguments(selfLink.value()), "/Communication?received=eqNULL");
    }
}

TEST_F(CommunicationGetHandlerTest, getAllCommunications_searchBySentNull)//NOLINT(readability-function-cognitive-complexity)
{
    std::string kvnrInsurant = InsurantA;
    std::string kvnrRepresentative1 = InsurantB;
    std::string kvnrRepresentative2 = InsurantC;
    std::string kvnrRepresentative3 = InsurantD;
    std::string kvnrRepresentative4 = InsurantE;
    std::string pharmacy1 = "3-SMC-B-Testkarte-883110000120312";
    std::string pharmacy2 = "3-SMC-B-Testkarte-883110000231423";
    std::string pharmacy3 = "3-SMC-B-Testkarte-883110000342534";
    std::string pharmacy4 = "3-SMC-B-Testkarte-883110000453645";

    const auto task = addTaskToDatabase({ model::Task::Status::ready, kvnrInsurant });

    model::Communication infoReqByRepresentative1ToPharmacy1 = addCommunicationToDatabase({
        task.prescriptionId(), model::Communication::MessageType::InfoReq,
            {ActorRole::Insurant, kvnrRepresentative1}, {ActorRole::Pharmacists, pharmacy1},
            "", InfoReqMessage });
    model::Communication replyByPharmacy1ToRepresentative1 = addCommunicationToDatabase({
        task.prescriptionId(), model::Communication::MessageType::Reply,
            {ActorRole::Pharmacists, pharmacy1}, {ActorRole::Insurant, kvnrRepresentative1},
            "", ReplyMessage });
    model::Communication infoReqByRepresentative2ToPharmacy2 = addCommunicationToDatabase({
        task.prescriptionId(), model::Communication::MessageType::InfoReq,
            {ActorRole::Insurant, kvnrRepresentative2}, {ActorRole::Pharmacists, pharmacy2},
            "", InfoReqMessage });
    model::Communication replyByPharmacy2ToRepresentative2 = addCommunicationToDatabase({
        task.prescriptionId(), model::Communication::MessageType::Reply,
            {ActorRole::Pharmacists, pharmacy2}, {ActorRole::Insurant, kvnrRepresentative2},
            "", ReplyMessage });
    model::Communication dispReqByRepresentative3ToPharmacy3 = addCommunicationToDatabase({
        task.prescriptionId(), model::Communication::MessageType::DispReq,
            {ActorRole::Insurant, kvnrRepresentative3}, {ActorRole::Pharmacists, pharmacy3},
            "", ReplyMessage });
    model::Communication dispReqByRepresentative4ToPharmacy4 = addCommunicationToDatabase({
        task.prescriptionId(), model::Communication::MessageType::DispReq,
            {ActorRole::Insurant, kvnrRepresentative4}, {ActorRole::Pharmacists, pharmacy4},
            "", ReplyMessage });

    // Search for "sender=<Insurant>&sent=NULL". Expected result: no "unsent" messages.
    {
        const JWT jwtPharmacy{ mJwtBuilder.makeJwtApotheke(pharmacy1) };
        std::string pathAndArguments = "/Communication?sent=NULL&sender=" + kvnrRepresentative1;
        ClientRequest request(createGetHeader(pathAndArguments, jwtPharmacy), "");
        auto outerResponse = createClient().send(encryptRequest(request, jwtPharmacy));
        const auto bundle = expectGetCommunicationResponse(
            outerResponse, { }, true, HttpStatus::OK, ContentMimeType::fhirXmlUtf8);
        const auto selfLink = bundle->getLink(model::Link::Type::Self);
        EXPECT_TRUE(selfLink.has_value());
        EXPECT_EQ(extractPathAndArguments(selfLink.value()), "/Communication?sent=eqNULL&sender=" + kvnrRepresentative1);
    }

    // Search for "recipient=<Insurant>&sent=NULL". Expected result: no "unsent" messages.
    {
        const JWT jwtPharmacy{ mJwtBuilder.makeJwtApotheke(pharmacy2) };
        std::string pathAndArguments = "/Communication?sent=NULL&recipient=" + kvnrRepresentative2;
        ClientRequest request(createGetHeader(pathAndArguments, jwtPharmacy), "");
        auto outerResponse = createClient().send(encryptRequest(request, jwtPharmacy));
        const auto bundle = expectGetCommunicationResponse(
            outerResponse, { }, true, HttpStatus::OK, ContentMimeType::fhirXmlUtf8);
        const auto selfLink = bundle->getLink(model::Link::Type::Self);
        EXPECT_TRUE(selfLink.has_value());
        EXPECT_EQ(extractPathAndArguments(selfLink.value()), "/Communication?sent=eqNULL&recipient=" + kvnrRepresentative2);
    }

    // Search for "sent=NULL". Expected result: no "unsent" messages.
    {
        const JWT jwtPharmacy{ mJwtBuilder.makeJwtApotheke(pharmacy3) };
        std::string pathAndArguments = "/Communication?sent=NULL";
        ClientRequest request(createGetHeader(pathAndArguments, jwtPharmacy), "");
        auto outerResponse = createClient().send(encryptRequest(request, jwtPharmacy));
        const auto bundle = expectGetCommunicationResponse(
            outerResponse, { }, true, HttpStatus::OK, ContentMimeType::fhirXmlUtf8);
        const auto selfLink = bundle->getLink(model::Link::Type::Self);
        EXPECT_TRUE(selfLink.has_value());
        EXPECT_EQ(extractPathAndArguments(selfLink.value()), "/Communication?sent=eqNULL");
    }

    // Search for "sent=neNULL". Expected result: all messages sent or received by pharmacy3
    {
        const JWT jwtPharmacy{ mJwtBuilder.makeJwtApotheke(pharmacy3) };
        std::string pathAndArguments = "/Communication?sent=neNULL";
        ClientRequest request(createGetHeader(pathAndArguments, jwtPharmacy), "");
        auto outerResponse = createClient().send(encryptRequest(request, jwtPharmacy));
        const auto bundle = expectGetCommunicationResponse(
            outerResponse, { dispReqByRepresentative3ToPharmacy3.id().value() }, true, HttpStatus::OK, ContentMimeType::fhirXmlUtf8);
        const auto selfLink = bundle->getLink(model::Link::Type::Self);
        EXPECT_TRUE(selfLink.has_value());
        EXPECT_EQ(extractPathAndArguments(selfLink.value()), "/Communication?sent=neNULL");
    }

    // Search for "sent=gtNULL". Expected result: BadRequest.
    {
        const JWT jwtPharmacy{ mJwtBuilder.makeJwtApotheke(pharmacy3) };
        std::string pathAndArguments = "/Communication?sent=gtNULL";
        ClientRequest request(createGetHeader(pathAndArguments, jwtPharmacy), "");
        auto outerResponse = createClient().send(encryptRequest(request, jwtPharmacy));
        const auto bundle = expectGetCommunicationResponse(
            outerResponse, { }, true, HttpStatus::BadRequest, ContentMimeType::fhirXmlUtf8);
    }
}


TEST_F(CommunicationGetHandlerTest, getAllCommunications_pagingSearch)
{
    // Setup the database.
    const auto givenTask = addTaskToDatabase({ model::Task::Status::draft, InsurantF });
    std::vector<Uuid> givenCommunicationIds1;
    std::vector<Uuid> givenCommunicationIds2;
    auto ts = model::Timestamp::fromXsDateTime("2022-01-23T12:00:00Z");
    const std::size_t messageNum = 10;
    for(unsigned int i = 0; i < messageNum; ++i)
    {
        givenCommunicationIds1.emplace_back(addCommunicationToDatabase({
            givenTask.prescriptionId(), model::Communication::MessageType::Representative,
            {ActorRole::Insurant, InsurantE}, {ActorRole::Insurant, InsurantF},
            std::string(givenTask.accessCode()),
            RepresentativeMessageByInsurant, ts}).id().value());
        givenCommunicationIds2.emplace_back(addCommunicationToDatabase({
            givenTask.prescriptionId(), model::Communication::MessageType::Representative,
            {ActorRole::Insurant, InsurantG}, {ActorRole::Insurant, InsurantF},
            std::string(givenTask.accessCode()),
            RepresentativeMessageByInsurant, ts}).id().value());
        ts = ts + std::chrono::duration_cast<model::Timestamp::timepoint_t::duration>(std::chrono::seconds(60));
    }

    const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(InsurantF) };

    // Send the request (first page)
    {
        auto outerResponse = createClient().send(encryptRequest(ClientRequest(
            createGetHeader("/Communication?sender=" + std::string(InsurantE) + "&_sort=sent&_count=6&__offset=0", jwtInsurant),
            ""), jwtInsurant));
        const auto communicationIdsPage = std::vector<Uuid>{givenCommunicationIds1.begin(), givenCommunicationIds1.begin() + 6};
        const auto bundle = expectGetCommunicationResponse(outerResponse, communicationIdsPage, false);
        EXPECT_EQ(bundle->getTotalSearchMatches(), messageNum);
    }
    // Send the request (second page)
    {
        auto outerResponse = createClient().send(encryptRequest(ClientRequest(
            createGetHeader("/Communication?sender=" + std::string(InsurantE) + "&_sort=sent&_count=6&__offset=6", jwtInsurant),
            ""), jwtInsurant));
        const auto communicationIdsPage = std::vector<Uuid>{givenCommunicationIds1.begin() + 6, givenCommunicationIds1.end()};
        const auto bundle = expectGetCommunicationResponse(outerResponse, communicationIdsPage, false);
        EXPECT_EQ(bundle->getTotalSearchMatches(), messageNum);
    }
}

// ^^ get all ^^
//////////////////////////////////////////////////////////////
// vv get by id vv



TEST_F(CommunicationGetHandlerTest, getCommunicationById_success)
{
    // Setup the database.
    const auto givenTask = addTaskToDatabase({model::Task::Status::ready, InsurantE});
    const auto givenCommunication1 = addCommunicationToDatabase({
        givenTask.prescriptionId(), model::Communication::MessageType::Representative,
        {ActorRole::Insurant, InsurantE}, {ActorRole::Insurant, InsurantF},
        std::string(givenTask.accessCode()),
        RepresentativeMessageByInsurant, model::Timestamp::fromXsDateTime("2022-01-23T12:34:56Z")});
    const auto givenCommunication2 = addCommunicationToDatabase({
        givenTask.prescriptionId(), model::Communication::MessageType::Representative,
        {ActorRole::Insurant, InsurantE}, {ActorRole::Insurant, InsurantG},
        std::string(givenTask.accessCode()),
        RepresentativeMessageByInsurant, model::Timestamp::fromXsDateTime("2022-01-23T12:35:11Z")});

    // Create the inner request
    const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(InsurantE) };
    ClientRequest request(
        createGetHeader("/Communication/" + givenCommunication1.id().value().toString(), jwtInsurant),
        "");

    // Send the request.
    auto client = createClient();
    auto outerResponse = client.send(encryptRequest(request, jwtInsurant));

    // Verify and decrypt the outer response. Also the generic part of the inner response.
    auto innerResponse = verifyOuterResponse(outerResponse);
    verifyGenericInnerResponse(innerResponse);

    // Verify the returned Communication object.
    std::optional<model::Communication> communication;
    ASSERT_NO_THROW(communication = model::Communication::fromJson(
                        innerResponse.getBody(), *StaticData::getJsonValidator(), *StaticData::getXmlValidator(),
                        *StaticData::getInCodeValidator(), SchemaType::Gem_erxCommunicationRepresentative));

    ASSERT_EQ(communication->id(), givenCommunication1.id());
}


/**
 * Call GET /Communication/<id> with an id for which there is NO object in the database.
 */
TEST_F(CommunicationGetHandlerTest, getCommunicationById_failForMissingObject)
{
    // Keep the database empty.

    // Create the inner request
    const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(InsurantE) };
    ClientRequest request(
        createGetHeader("/Communication/12300000-0000-0000-0000-000000000000", jwtInsurant),
        "");

    // Send the request. Note that the client uses a jwt that has `InsurantF` as caller.
    auto client = createClient();
    auto outerResponse = client.send(encryptRequest(request, jwtInsurant));

    // Verify and decrypt the outer response. Also the generic part of the inner response.
    auto innerResponse = verifyOuterResponse(outerResponse);
    ASSERT_EQ(innerResponse.getHeader().status(), HttpStatus::NotFound);
    ASSERT_TRUE(innerResponse.getHeader().header(Header::ContentType).has_value());  // error response
    ASSERT_TRUE(innerResponse.getHeader().hasHeader(Header::ContentLength));
    ASSERT_GT(innerResponse.getHeader().contentLength(), 0);
}


// GEMREQ-start A_19520-01#getCommunicationById_failForObjectDirectedAtOtherUser
/**
 * Call GET /Communication/<id> with an id for which there IS an object in the database BUT it has a different
 * sender and a different recipient from the one that is making the request.
 */
TEST_F(CommunicationGetHandlerTest, getCommunicationById_failForObjectDirectedAtOtherUser)
{
    // Setup the database.
    const auto givenTask = addTaskToDatabase({model::Task::Status::ready, InsurantE});
    const auto givenCommunication = addCommunicationToDatabase({
        givenTask.prescriptionId(), model::Communication::MessageType::Representative,
        {ActorRole::Insurant, InsurantG}, {ActorRole::Insurant, InsurantH},
        std::string(givenTask.accessCode()),
        RepresentativeMessageByInsurant, model::Timestamp::fromXsDateTime("2022-01-23T12:34:56+01:00")});

    // Create the inner request.
    const JWT jwtInsurant{ mJwtBuilder.makeJwtVersicherter(InsurantE) };
    ClientRequest request(
        createGetHeader("/Communication/" + givenCommunication.id().value().toString(), jwtInsurant),
        "");

    // Send the request.  Note that the JWT uses InsurantF, not InsurantG, as user.
    auto client = createClient();
    auto outerResponse = client.send(encryptRequest(request, jwtInsurant));

    // Verify and decrypt the outer response. Also the generic part of the inner response.
    auto innerResponse = verifyOuterResponse(outerResponse);
    ASSERT_EQ(innerResponse.getHeader().status(), HttpStatus::NotFound);
    ASSERT_TRUE(innerResponse.getHeader().header(Header::ContentType).has_value()); // error response
    ASSERT_TRUE(innerResponse.getHeader().hasHeader(Header::ContentLength));
    ASSERT_GT(innerResponse.getHeader().contentLength(), 0);
}
// GEMREQ-end A_19520-01#getCommunicationById_failForObjectDirectedAtOtherUser

TEST_F(CommunicationGetHandlerTest, getCommunicationById_ignoreCancelledTasks)
{
    const JWT jwtPharmacy = mJwtBuilder.makeJwtApotheke();
    const std::string pharmacy = jwtPharmacy.stringForClaim(JWT::idNumberClaim).value();

    // Setup the database.
    auto givenTask = addTaskToDatabase({ model::Task::Status::ready, InsurantE });
    const auto givenCommunication1 = addCommunicationToDatabase({
        givenTask.prescriptionId(), model::Communication::MessageType::InfoReq,
        {ActorRole::Insurant, InsurantE}, {ActorRole::Pharmacists, pharmacy},
        std::string(givenTask.accessCode()),
        RepresentativeMessageByInsurant, model::Timestamp::fromXsDateTime("2022-01-23T12:34:56Z") });
    abortTask(givenTask);

    auto client = createClient();

    ClientRequest request(createGetHeader("/Communication/" + givenCommunication1.id().value().toString(), jwtPharmacy), "");
    auto outerResponse = client.send(encryptRequest(request, jwtPharmacy));
    auto innerResponse = verifyOuterResponse(outerResponse);
    verifyGenericInnerResponse(innerResponse, HttpStatus::NotFound);
}

TEST_F(CommunicationGetHandlerTest, getAllCommunications_searching_paging)
{
    std::string kvnrInsurant = InsurantA;
    std::string pharmacy1 = "3-SMC-B-Testkarte-883110000120312";

    const auto task = addTaskToDatabase({ model::Task::Status::ready, InsurantA });


    model::Timestamp timestamp = model::Timestamp::fromXsDateTime("2021-09-25T12:34:56+01:00");
    for (int idxPatient = 0; idxPatient < 18; ++idxPatient)
    {
         const auto communication = addCommunicationToDatabase({
            task.prescriptionId(), model::Communication::MessageType::InfoReq,
            {ActorRole::Insurant, InsurantA}, {ActorRole::Pharmacists, pharmacy1},
            std::string(task.accessCode()),
            RepresentativeMessageByInsurant, timestamp + std::chrono::hours(-24 * idxPatient) });
    }

    {
        const JWT jwtPharmacy{ mJwtBuilder.makeJwtApotheke(pharmacy1) };
        std::string pathAndArguments = UrlHelper::escapeUrl("/Communication?sent=le2021-09-25T12:34:56+01:00&_count=10&__offset=0");
        ClientRequest request(createGetHeader(pathAndArguments, jwtPharmacy), "");
        ClientResponse outerResponse;
        EXPECT_NO_FATAL_FAILURE(outerResponse = createClient().send(encryptRequest(request, jwtPharmacy)));
        std::optional<model::Bundle> bundle;
        EXPECT_NO_FATAL_FAILURE(bundle = getBundle(outerResponse, ContentMimeType::fhirXmlUtf8));
        auto nextLink = bundle->getLink(model::Link::Type::Next);
        EXPECT_TRUE(nextLink.has_value());
        auto nextPathAndArguments = extractPathAndArguments(nextLink.value());
        EXPECT_EQ(UrlHelper::unescapeUrl(nextPathAndArguments), "/Communication?sent=le2021-09-25T11:34:56+00:00&_count=10&__offset=10");

        // Get next request
        ClientRequest nextRrequest(createGetHeader(nextPathAndArguments, jwtPharmacy), "");
        EXPECT_NO_FATAL_FAILURE(outerResponse = createClient().send(encryptRequest(nextRrequest, jwtPharmacy)));
        EXPECT_NO_FATAL_FAILURE(bundle = getBundle(outerResponse, ContentMimeType::fhirXmlUtf8));
        nextLink = bundle->getLink(model::Link::Type::Next);
        EXPECT_FALSE(nextLink.has_value());
    }

   {
       const JWT jwtPharmacy{ mJwtBuilder.makeJwtApotheke(pharmacy1) };
       std::string pathAndArguments = UrlHelper::escapeUrl("/Communication?sent=le2021-09-25T12:34:56+01:00&_count=9&__offset=0");
       ClientRequest request(createGetHeader(pathAndArguments, jwtPharmacy), "");
       ClientResponse outerResponse;
       EXPECT_NO_FATAL_FAILURE(outerResponse = createClient().send(encryptRequest(request, jwtPharmacy)));
       std::optional<model::Bundle> bundle;
       EXPECT_NO_FATAL_FAILURE(bundle = getBundle(outerResponse, ContentMimeType::fhirXmlUtf8));
       auto nextLink = bundle->getLink(model::Link::Type::Next);
       EXPECT_TRUE(nextLink.has_value());
       auto nextPathAndArguments = extractPathAndArguments(nextLink.value());
       EXPECT_EQ(UrlHelper::unescapeUrl(nextPathAndArguments), "/Communication?sent=le2021-09-25T11:34:56+00:00&_count=9&__offset=9");

       // Get next request
       ClientRequest nextRrequest(createGetHeader(nextPathAndArguments, jwtPharmacy), "");
       EXPECT_NO_FATAL_FAILURE(outerResponse = createClient().send(encryptRequest(nextRrequest, jwtPharmacy)));
       EXPECT_NO_FATAL_FAILURE(bundle = getBundle(outerResponse, ContentMimeType::fhirXmlUtf8));
       nextLink = bundle->getLink(model::Link::Type::Next);
       EXPECT_FALSE(nextLink.has_value());
   }

   {
       const JWT jwtPharmacy{ mJwtBuilder.makeJwtApotheke(pharmacy1) };
       std::string pathAndArguments = UrlHelper::escapeUrl("/Communication?sent=le2021-09-25T12:34:56+01:00&_count=4&__offset=0");
       ClientRequest request(createGetHeader(pathAndArguments, jwtPharmacy), "");
       ClientResponse outerResponse;
       EXPECT_NO_FATAL_FAILURE(outerResponse = createClient().send(encryptRequest(request, jwtPharmacy)));
       std::optional<model::Bundle> bundle;
       EXPECT_NO_FATAL_FAILURE(bundle = getBundle(outerResponse, ContentMimeType::fhirXmlUtf8));
       auto nextLink = bundle->getLink(model::Link::Type::Next);
       EXPECT_TRUE(nextLink.has_value());
       auto nextPathAndArguments =  extractPathAndArguments(nextLink.value());
       EXPECT_EQ(UrlHelper::unescapeUrl(nextPathAndArguments), "/Communication?sent=le2021-09-25T11:34:56+00:00&_count=4&__offset=4");

       // Get next request
       for (size_t idxPatient = 0; idxPatient < 3; ++idxPatient)
       {
           ClientRequest nextRrequest(createGetHeader(nextPathAndArguments, jwtPharmacy), "");
           EXPECT_NO_FATAL_FAILURE(outerResponse = createClient().send(encryptRequest(nextRrequest, jwtPharmacy)));
           EXPECT_NO_FATAL_FAILURE(bundle = getBundle(outerResponse, ContentMimeType::fhirXmlUtf8));
           nextLink = bundle->getLink(model::Link::Type::Next);
           EXPECT_TRUE(nextLink.has_value());
           nextPathAndArguments = extractPathAndArguments(nextLink.value());
       }

       ClientRequest nextRrequest(createGetHeader(nextPathAndArguments, jwtPharmacy), "");
       EXPECT_NO_FATAL_FAILURE(outerResponse = createClient().send(encryptRequest(nextRrequest, jwtPharmacy)));
       EXPECT_NO_FATAL_FAILURE(bundle = getBundle(outerResponse, ContentMimeType::fhirXmlUtf8));
       nextLink = bundle->getLink(model::Link::Type::Next);
       EXPECT_FALSE(nextLink.has_value());
   }
}
