#include "EndpointHandlerTestFixture.hxx"

#include "erp/model/GemErpPrMedication.hxx"
#include "erp/model/MedicationDispense.hxx"
#include "erp/service/task/DispenseTaskHandler.hxx"
#include "erp/service/MedicationDispenseHandler.hxx"
#include "shared/model/ResourceNames.hxx"
#include "test/util/JwtBuilder.hxx"
#include "test/util/ResourceTemplates.hxx"

class MedicationDispenseGetEndpointTest : public EndpointHandlerTest
{
protected:
    void dispense() {
        ServerRequest serverRequest{{HttpMethod::POST,
            "/Task/" + taskId6.toString() + "/$dispense",
                                    Header::Version_1_1,
                                    {{Header::ContentType, ContentMimeType::fhirXmlUtf8}},
                                    HttpStatus::Unknown}};
        serverRequest.setPathParameters({"id"}, {taskId6.toString()});
        serverRequest.setAccessToken(pharmacyJWT);
        serverRequest.setQueryParameters({{"secret", secret}});
        if (GEM_ERP_current < ResourceTemplates::Versions::GEM_ERP_1_4)
        {
            serverRequest.setBody(ResourceTemplates::medicationDispenseBundleXml(
                {.gematikVersion = GEM_ERP_current,
                 .medicationDispenses =
                     {
                         {
                             .prescriptionId = taskId6,
                         },
                         {
                             .prescriptionId = taskId6,
                         },
                     },
                 .prescriptionId = taskId6}));
        }
        else
        {
            serverRequest.setBody(ResourceTemplates::medicationDispenseOperationParametersXml(
                {.profileType = model::ProfileType::GEM_ERP_PR_PAR_DispenseOperation_Input,
                    .version = GEM_ERP_current,
                    .medicationDispenses = {
                        {
                            .prescriptionId = taskId6,
                            .gematikVersion = GEM_ERP_current,
                            .medication = ResourceTemplates::MedicationOptions{.version = GEM_ERP_current},
                        },
                        {
                            .prescriptionId = taskId6,
                            .gematikVersion = GEM_ERP_current,
                            .medication = ResourceTemplates::MedicationOptions{.version = GEM_ERP_current},
                        },
                    }}));
        }
        ServerResponse serverResponse;
        AccessLog accessLog;
        SessionContext session(mServiceContext, serverRequest, serverResponse, accessLog);
        DispenseTaskHandler handler{{}};
        ASSERT_NO_THROW(handler.preHandleRequestHook(session));
        ASSERT_NO_THROW(handler.handleRequest(session)) << serverRequest.getBody();
    }

    const model::PrescriptionId taskId6 = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4716);
    std::string pharmacyId = "3-SMC-B-Testkarte-883110000120312";
    JWT pharmacyJWT = JwtBuilder::testBuilder().makeJwtApotheke(pharmacyId);
    std::string kvnr = "X234567891";
    JWT insurantJWT = JwtBuilder::testBuilder().makeJwtVersicherter("X234567891");
    std::string secret = "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f";
    ResourceTemplates::Versions::GEM_ERP GEM_ERP_current = ResourceTemplates::Versions::GEM_ERP_current();
};

TEST_F(MedicationDispenseGetEndpointTest, searchMode)
{
    using namespace std::string_literals;
    using model::resource::ElementName;
    namespace elements = model::resource::elements;
    static const rapidjson::Pointer resourcePointer{ElementName::path(elements::resource)};
    static const rapidjson::Pointer searchModePointer{ElementName::path(elements::search, elements::mode)};
    ASSERT_NO_FATAL_FAILURE(dispense());
    std::optional<model::UnspecifiedResource> unspec;
    ASSERT_NO_FATAL_FAILURE(unspec = getMedicationDispenses(kvnr, taskId6));
    ASSERT_TRUE(unspec.has_value());
    auto bundle = model::Bundle::fromJson(std::move(unspec).value().jsonDocument());
    const auto& entries = bundle.getEntries();
    size_t medicationDispenses = 0;
    size_t medications = 0;
    for (const auto& entry: entries)
    {
        const auto* resourceValue = resourcePointer.Get(entry);
        ASSERT_NE(resourceValue, nullptr);
        model::NumberAsStringParserDocument resourceDoc;
        resourceDoc.CopyFrom(*resourceValue, resourceDoc.GetAllocator());
        const auto resource = model::UnspecifiedResource::fromJson(std::move(resourceDoc));
        const auto* searchModeValue = searchModePointer.Get(entry);
        ASSERT_NE( searchModeValue, nullptr);
        std::string searchMode{model::NumberAsStringParserDocument::valueAsString(*searchModeValue)};
        if (resource.getResourceType() == model::GemErpPrMedication::resourceTypeName)
        {
            EXPECT_EQ(searchMode, "include"s);
            ++medications;
        }
        else if (resource.getResourceType() == model::MedicationDispense::resourceTypeName)
        {
            EXPECT_EQ(searchMode, "match"s);
            ++medicationDispenses;
        }
        else
        {
            FAIL() << "Resource must be either MedicationDispense or Medication.";
        }
    }
    if (ResourceTemplates::Versions::GEM_ERP_current() >= ResourceTemplates::Versions::GEM_ERP_1_4)
    {
        EXPECT_EQ(medicationDispenses, 2);
        EXPECT_EQ(medications, 2);
    }
    else
    {
        EXPECT_EQ(medicationDispenses, 2);
        EXPECT_EQ(medications, 0);
    }

    if (HasFailure())
    {
        LOG(INFO) << bundle.serializeToJsonString();
    }
}
